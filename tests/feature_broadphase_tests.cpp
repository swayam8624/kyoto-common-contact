#include "kyoto/feature_broadphase.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kyoto::contact;
using namespace kyoto::contact::features;

static void require(bool ok,const std::string& msg){
    if(!ok)throw std::runtime_error(msg);
}
static Vertex fixed(std::uint64_t id,double x,double y,double z,
                    std::uint64_t object=0){
    CubicVertex p{{Vec3{x,y,z},Vec3{x,y,z},Vec3{x,y,z},Vec3{x,y,z}}};
    return {id,p,object,1,~std::uint64_t{0}};
}
static Vertex cubic(std::uint64_t id,Vec3 a,Vec3 b,Vec3 c,Vec3 d,
                    std::uint64_t object=0) {
    return {id,CubicVertex{{a,b,c,d}},object,1,~std::uint64_t{0}};
}
static bool contains(const Result& r,Kind kind,std::uint64_t a,std::uint64_t b) {
    return std::binary_search(r.pairs.begin(),r.pairs.end(),
                              Candidate{kind,a,b});
}
static void parity(const Mesh& mesh,const std::string& tag) {
    const auto built=build(mesh);
    const auto a=exhaustive(built),b=sweep_and_prune(built);
    require(a.pairs==b.pairs,tag+": typed SAP/exhaustive mismatch");
    require(std::adjacent_find(a.pairs.begin(),a.pairs.end())
            ==a.pairs.end(),tag+": duplicate canonical candidate");
    for(const auto& c:a.pairs)
        if(c.kind==Kind::EdgeEdge)
            require(c.first<c.second,tag+": unsorted EE IDs");
}
static Mesh anchor_case(){
    Mesh m;
    m.thickness=1e-6;
    m.vertices={
        fixed(1,0,0,0,5),fixed(2,1,0,0,5),fixed(3,0,1,0,5),
        cubic(4,{.25,.25,2},{.25,.25,-2},{.25,.25,-2},
                 {.25,.25,2},5),
        fixed(5,0,0,0,6),fixed(6,0,1,0,6),fixed(7,0,0,1,6)
    };
    m.faces={{100,{1,2,3},1,~std::uint64_t{0}},
             {101,{5,6,7},1,~std::uint64_t{0}}};
    m.edges={{10,{1,2},1,~std::uint64_t{0}},
             {11,{2,3},1,~std::uint64_t{0}},
             {12,{5,6},1,~std::uint64_t{0}},
             {13,{6,7},1,~std::uint64_t{0}}};
    return m;
}
int main(){
 try {
    // Mid-step contact exists while both endpoint positions are far from
    // the stationary face. Same object=5 is intentional: NONLOCAL
    // self-collision must not be filtered merely by equal object ID.
    auto m=anchor_case();
    parity(m,"nonlocal_self_contact_and_cross_object");
    auto found=sweep_and_prune(m);
    require(contains(found,Kind::VertexFace,4,100),
            "moving vertex must be candidate with same-object face");
    require(!contains(found,Kind::VertexFace,1,100),
            "incident vertex-face pair must be excluded");
    require(!contains(found,Kind::EdgeEdge,10,11),
            "edges sharing a vertex must be excluded");
    require(contains(found,Kind::EdgeEdge,10,12),
            "cross-object nonincident edge-edge candidate must survive");
    {
        auto invalid=m;
        invalid.vertices.push_back(m.vertices[0]);
        bool threw=false;
        try{(void)build(invalid);}
        catch(const std::invalid_argument&){threw=true;}
        require(threw,"duplicate vertex ID not rejected");
    }
    {
        auto invalid=m;
        invalid.faces[0].vertices={1,1,2};
        bool threw=false;
        try{(void)build(invalid);}
        catch(const std::invalid_argument&){threw=true;}
        require(threw,"duplicate face vertex not rejected");
    }
    {
        auto invalid=m;
        invalid.edges[0].vertices={1,999};
        bool threw=false;
        try{(void)build(invalid);}
        catch(const std::invalid_argument&){threw=true;}
        require(threw,"dangling edge vertex not rejected");
    }
    {
        auto invalid=m;
        invalid.topology_epoch=0;
        bool threw=false;
        try{(void)build(invalid);}
        catch(const std::invalid_argument&){threw=true;}
        require(threw,"invalid epoch not rejected");
    }
    {
        auto invalid=m;
        invalid.vertices[3].path.control[1].y=
            std::numeric_limits<double>::quiet_NaN();
        bool threw=false;
        try{(void)build(invalid);}
        catch(const std::invalid_argument&){threw=true;}
        require(threw,"nonfinite control not rejected");
    }
    {
        auto incompatible=m;
        incompatible.vertices[3].mask=0;
        parity(incompatible,"mask_change");
        require(!contains(sweep_and_prune(incompatible),
                          Kind::VertexFace,4,100),
                "mask changes must affect VF candidate truth");
    }
    {
        // Topology change with identical original motion must not
        // accidentally retain stale pairs: reference is rebuilt afresh.
        auto changed=m;
        changed.faces[0].vertices={2,3,4};
        changed.topology_epoch++;
        parity(changed,"topology_epoch_change");
        require(!contains(sweep_and_prune(changed),Kind::VertexFace,4,100),
                "newly incident vertex-face must be excluded");
    }
    std::mt19937_64 rng(20261123);
    std::uniform_real_distribution<double> coord(-5.,5.);
    std::uniform_int_distribution<int> vertex_pick(0,27);
    for(int scene=0;scene<140;scene++) {
        Mesh random;
        random.topology_epoch=std::uint64_t(scene+1);
        random.thickness=.05;
        for(std::uint64_t j=0;j<28;j++){
            const Vec3 a{coord(rng),coord(rng),coord(rng)};
            const Vec3 b{coord(rng),coord(rng),coord(rng)};
            const Vec3 c{coord(rng),coord(rng),coord(rng)};
            const Vec3 d{coord(rng),coord(rng),coord(rng)};
            random.vertices.push_back(cubic(1000+j,a,b,c,d,j%3));
            if(j%9==0)random.vertices.back().mask=1;
        }
        for(std::uint64_t j=0;j<26;j++){
            const auto a=std::uint64_t(vertex_pick(rng));
            auto b=std::uint64_t(vertex_pick(rng));
            if(b==a)b=(b+1)%28;
            random.edges.push_back({2000+j,{1000+a,1000+b},1,
                                   ~std::uint64_t{0}});
        }
        for(std::uint64_t j=0;j<20;j++){
            auto a=std::uint64_t(vertex_pick(rng));
            auto b=std::uint64_t(vertex_pick(rng));
            auto c=std::uint64_t(vertex_pick(rng));
            if(b==a)b=(b+1)%28;
            if(c==a||c==b)c=(b+1)%28;
            if(c==a||c==b)c=(c+1)%28;
            random.faces.push_back({3000+j,{1000+a,1000+b,1000+c},
                                   1,~std::uint64_t{0}});
        }
        parity(random,"randomized mesh "+std::to_string(scene));
        auto reordered=random;
        std::shuffle(reordered.vertices.begin(),reordered.vertices.end(),rng);
        std::shuffle(reordered.edges.begin(),reordered.edges.end(),rng);
        std::shuffle(reordered.faces.begin(),reordered.faces.end(),rng);
        parity(reordered,"reordered mesh "+std::to_string(scene));
        require(sweep_and_prune(random).pairs==sweep_and_prune(reordered).pairs,
                "input iteration order changed typed candidate set");
    }
    std::cout<<"PASS: mesh feature contract; 140 randomized 3D meshes "
                "and 140 reorderings; VF and EE parity; topology changes, "
                "masks, nonlocal self-collision, cubic mid-step motion\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}

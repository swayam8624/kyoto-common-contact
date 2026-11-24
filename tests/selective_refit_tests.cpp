// Pass 20: selective refit correctness and transactionality.
// The independent exhaustive VF/EE oracle is the only source of truth.
// Test scenes are synthetic features; no physics claim is attached.
#include "kyoto/refit_feature_hierarchy.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
using kyoto::contact::atlas::RefitHierarchy;

static void require(bool v,const char* context) {
    if(!v)throw std::runtime_error(context);
}
static f::Mesh make_scene(std::size_t triangles) {
    f::Mesh mesh;
    mesh.thickness=.0005;
    for(std::size_t i=0;i<triangles;++i) {
        const std::uint64_t base=static_cast<std::uint64_t>(i)*3;
        for(int k=0;k<3;++k) {
            const Vec3 p{double(i)*.4+(k==1?.15:0.),
                         k==2?.15:0.,.01*double(i%3)};
            f::Vertex v;
            v.id=base+std::uint64_t(k);
            v.path.control={p,p,p,p};
            mesh.vertices.push_back(v);
        }
        mesh.faces.push_back({static_cast<std::uint64_t>(i),
                              {base,base+1,base+2}});
        for(int k=0;k<3;++k)
            mesh.edges.push_back({static_cast<std::uint64_t>(mesh.edges.size()),
                                  {base+std::uint64_t(k),
                                   base+std::uint64_t((k+1)%3)}});
    }
    return mesh;
}
static void parity(RefitHierarchy& selective,RefitHierarchy& ordinary,
                   RefitHierarchy& optimized,
                   const f::Mesh& mesh,const char* stage) {
    const auto independent=f::exhaustive(mesh).pairs;
    const auto got=selective.query(mesh).pairs;
    const auto control=ordinary.query(mesh).pairs;
    const auto improved=optimized.query(mesh).pairs;
    if(got!=independent || control!=independent ||
       improved!=independent || got!=improved)
        throw std::runtime_error(std::string(stage)+
            ": selective refit changed complete VF/EE candidate set");
}
int main(){
 try{
    auto mesh=make_scene(40);
    RefitHierarchy selective(true,false,false,4,true);
    RefitHierarchy baseline(true,false);
    RefitHierarchy dense(true,false,false,4,true,true);
    parity(selective,baseline,dense,mesh,"initial cold");
    require(selective.rebuilds()==1,"cold source must build hierarchy");

    // 30 static source frames: the queried geometry is identical, but
    // the broadphase still re-evaluates candidate coverage each frame.
    for(int frame=0;frame<30;++frame){
        parity(selective,baseline,dense,mesh,"stationary warm");
        require(selective.dirty_vertex_count()==0,
                "unchanged 3D path was unnecessarily dirtied");
    }
    require(selective.unchanged_refits()==30,
            "stationary geometry did not trigger safe zero-refit path");

    // One moving triangle among 40: 3 changed vertex envelopes,
    // 3 dependent edges, 1 dependent face, then only affected ancestors.
    for(int frame=0;frame<90;++frame){
        for(int k=0;k<3;++k) {
            auto& v=mesh.vertices[3*7+std::size_t(k)];
            const double t=double(frame)*.011;
            Vec3 first{.2+t+(k==1?.15:0.),k==2?.15:0.,.0};
            Vec3 last{first.x+.004,first.y+.003,first.z-.002};
            v.path.control={first,{first.x,first.y,first.z+.04},
                            {last.x,last.y,last.z-.04},last};
        }
        parity(selective,baseline,dense,mesh,"one moving deformable triangle");
        require(selective.dirty_vertex_count()==3,
                "selective path did not limit dirty vertex set");
    }
    require(selective.partial_refits()>=90,
            "selective hierarchy failed to refit affected nodes");

    // An isolated skin change invalidates ALL conservative envelopes
    // even if no source geometry changed.
    mesh.thickness=.025;
    parity(selective,baseline,dense,mesh,"global contact skin");
    require(selective.dirty_vertex_count()==mesh.vertices.size(),
            "changed collision skin did not refresh all swept bounds");

    // Topological collision mask changes must bypass old caches even
    // when an author neglects to advance the explicit topology epoch.
    mesh.faces[7].mask=0;
    parity(selective,baseline,dense,mesh,"same-epoch mask invalidation");
    require(selective.rebuilds()==2,
            "same-epoch collision mask change failed rebuild");

    mesh.topology_epoch=3;
    std::swap(mesh.faces[5].vertices[1],mesh.faces[5].vertices[2]);
    parity(selective,baseline,dense,mesh,"epoch and connectivity invalidation");
    require(selective.rebuilds()==3,
            "source face connectivity change failed rebuild");

    // Never publish incomplete output if a later vertex contains NaN.
    // A failed call MUST leave the previous successful tree usable.
    auto invalid=mesh;
    invalid.vertices.back().path.control[1].x=
        std::numeric_limits<double>::quiet_NaN();
    bool rejected=false;
    try{(void)selective.query(invalid);}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"invalid motion escaped selective geometry validator");
    parity(selective,baseline,dense,mesh,"transactional recovery after invalid input");
    require(selective.dirty_vertex_count()==0,
            "transactional failure modified previously accepted feature state");

    // Move every vertex: require the method to fall back to complete
    // BVH bounds refitting instead of quadratic ancestor work.
    for(auto& v:mesh.vertices){
        for(auto& p:v.path.control)p.x+=.2;
    }
    parity(selective,baseline,dense,mesh,"all-motion full-refit fallback");
    require(selective.dirty_vertex_count()==mesh.vertices.size(),
            "all-moving scene did not mark all vertices dirty");
    require(dense.dirty_vertex_count()==mesh.vertices.size(),
            "dense source changed the mathematical dirty-vertex set");
    require(dense.dense_motion_warm_frames()>=2,
            "dense feature regeneration branch never exercised");

    std::cout<<"PASS20: selective BVH refit and full-refit reference "
             <<"matched independent VF/EE oracle across static/moving "
             <<"cubic paths, global skin, mask, connectivity, NaN rollback "
             <<"and all-motion fallback.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS20 FAILURE: "<<e.what()<<'\n';
    return 1;
 }
}

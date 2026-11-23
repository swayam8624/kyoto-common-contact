// Pass 14 hierarchy correctness, including broadphase mask/topology
// invalidation and randomized deformation against independent O(E²+VF).
#include "kyoto/refit_feature_hierarchy.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>

namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
static f::Vertex vertex(std::uint64_t id,Vec3 p){
    return {id,{{{p,p,p,p}}}};
}
static void verify(kyoto::contact::atlas::RefitHierarchy& h,
                   const f::Mesh& mesh,const char* label){
    const auto truth=f::exhaustive(mesh).pairs;
    const auto found=h.query(mesh).pairs;
    if(found!=truth)
        throw std::runtime_error(std::string(label)+": hierarchy candidate mismatch");
}
int main(){
 try{
    kyoto::contact::atlas::RefitHierarchy h;
    f::Mesh mesh;
    mesh.thickness=.0005;
    mesh.vertices={vertex(0,{0,0,0}),vertex(1,{1,0,0}),
        vertex(2,{0,1,0}),vertex(3,{.1,.1,.2}),
        vertex(4,{.3,.1,.2}),vertex(5,{.1,.3,.2})};
    mesh.faces={{0,{0,1,2}},{1,{3,4,5}}};
    mesh.edges={{0,{0,1}},{1,{1,2}},{2,{2,0}},
                {3,{3,4}},{4,{4,5}},{5,{5,3}}};
    verify(h,mesh,"initial");
    if(h.rebuilds()!=1||h.refits()!=0)
        throw std::runtime_error("incorrect cold build statistics");
    std::mt19937_64 rng(20261123);
    std::uniform_real_distribution<double> pos(-1.0,1.0);
    for(int trial=0;trial<180;++trial){
        for(auto& v:mesh.vertices){
            const Vec3 p{pos(rng),pos(rng),pos(rng)};
            const Vec3 q{pos(rng),pos(rng),pos(rng)};
            // Full cubic hull, including intermediate mid-step bulges.
            v.path.control={p,{p.x,p.y,p.z+.35},
                {q.x,q.y,q.z-.35},q};
        }
        verify(h,mesh,"randomized warm refit");
    }
    if(h.rebuilds()!=1||h.refits()!=180)
        throw std::runtime_error("cached tree unexpectedly rebuilt on motion");

    mesh.faces.back().mask=0; // Same epoch but changed policy.
    verify(h,mesh,"mask invalidation");
    if(h.rebuilds()!=2)
        throw std::runtime_error("mask invalidation did not rebuild");
    mesh.topology_epoch=2;
    verify(h,mesh,"epoch invalidation");
    if(h.rebuilds()!=3)
        throw std::runtime_error("epoch invalidation did not rebuild");
    // Empty topological families must not access a nonexistent tree.
    mesh.faces.clear();
    mesh.edges.clear();
    verify(h,mesh,"empty features");
    if(h.rebuilds()!=4)
        throw std::runtime_error("feature deletion did not invalidate");
    // Robustness gate for shared-vertex adjacency vs nonlocal pairs.
    std::cout<<"PASS14 hierarchy: 180 randomized warm refits, "
        "all exhaustive VF/EE candidate sets equal; "
        "mask/epoch/feature-deletion rebuilds correct\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS14 FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

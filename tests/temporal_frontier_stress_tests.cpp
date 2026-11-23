// Pass 17: adversarial candidate completeness for retained dual-tree
// node-pair cut. This synthetic FEATURE test deliberately stresses dense
// overlaps, large scene changes, changed masks and force-fallback to full
// hierarchy traversal. It does NOT claim physical simulated dynamics.
#include "kyoto/refit_feature_hierarchy.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
using kyoto::contact::atlas::RefitHierarchy;

static void need(bool ok,const char* why) {
    if(!ok)throw std::runtime_error(why);
}
static f::Mesh make_mesh(){
    f::Mesh m;
    m.thickness=.0001;
    constexpr std::uint64_t triangles=60;
    for(std::uint64_t t=0;t<triangles;++t) {
        const auto v=3*t;
        for(int k=0;k<3;++k) {
            Vec3 p{};
            if(k==1)p.x=.10;
            if(k==2)p.y=.10;
            p.z=double(t)*1e-6;
            f::Vertex item;
            item.id=v+std::uint64_t(k);
            item.path.control={p,p,p,p};
            m.vertices.push_back(item);
        }
        m.faces.push_back({t,{v,v+1,v+2}});
        m.edges.push_back({3*t,{v,v+1}});
        m.edges.push_back({3*t+1,{v+1,v+2}});
        m.edges.push_back({3*t+2,{v+2,v}});
    }
    return m;
}
static void check(RefitHierarchy& frontier,RefitHierarchy& plain,
                  const f::Mesh& mesh,const char* label) {
    const auto truth=f::exhaustive(mesh).pairs;
    const auto found=frontier.query(mesh).pairs;
    const auto baseline=plain.query(mesh).pairs;
    if(found!=truth||baseline!=truth)
        throw std::runtime_error(std::string(label)+
            ": temporal frontier lost or added admissible candidate");
    // The bounded frontier cannot do expensive complete-cut construction
    // and only afterward throw the memory away: its actual retained size
    // and even attempted cut must remain beneath the advertised limit.
    const std::size_t limit=std::max(std::size_t{64},
        std::size_t{4}*(mesh.vertices.size()+mesh.edges.size()+
                        mesh.faces.size()));
    if(frontier.pair_cut_size()>limit ||
       frontier.last_pair_cut_attempted_size()>limit)
        throw std::runtime_error(std::string(label)+
            ": frontier violated preemptive memory cap");
}
int main() {
 try{
    auto m=make_mesh();
    RefitHierarchy frontier(true,true),plain(true,false);
    // Dense overlap produces a very large leaf-pair cut, which must
    // trigger budget fallback instead of retaining unbounded memory.
    check(frontier,plain,m,"dense cold overlap");
    need(frontier.pair_cut_fallbacks()>0,
         "dense source failed to exercise node-pair frontier budget fallback");

    for(int frame=1;frame<=50;++frame) {
        // Alternating near-dense and widely separated regions: even a
        // previously separated pair may overlap on the next frame.
        const bool scattered=(frame%4==1 || frame%4==2);
        for(std::size_t i=0;i<m.vertices.size();++i) {
            auto& v=m.vertices[i];
            const std::size_t t=i/3;
            const int corner=static_cast<int>(i%3);
            const double offset=scattered?double(t)*.8:
                0.002*std::sin(double(frame+t));
            const Vec3 a{offset+(corner==1?.10:0.),
                         offset*.4+(corner==2?.10:0.),
                         scattered?offset*.25:double(t)*1e-6};
            const Vec3 b{a.x+0.0001*frame,
                         a.y-0.0001*frame,a.z};
            // Path control points intentionally bulge at intermediate
            // times, to exercise complete cubic AABB enclosure.
            v.path.control={a,{a.x,a.y,a.z+.01},
                {b.x,b.y,b.z-.01},b};
        }
        check(frontier,plain,m,"rapidly changing 3D frontier");
    }
    need(frontier.pair_cut_reuses()>0,
         "frontier was never reused in a localized regime");
    need(frontier.pair_cut_skipped_frames()>0,
         "dense fallback must charge actual coarse-region traversal");

    m.faces[0].mask=0; // Same epoch, different collision policy
    check(frontier,plain,m,"mask change");
    m.topology_epoch++;
    check(frontier,plain,m,"topology epoch change");
    m.thickness=.02;
    check(frontier,plain,m,"large thickness update");
    std::cout<<"PASS17 complete pair coverage on dense/isolated/transition "
             <<"cubic AABB families, warm cut reuse, cold budget fallback, "
             <<"mask, epoch and skin invalidation; no simulated physics.\n";
    return 0;
 }catch(const std::exception& e) {
    std::cerr<<"PASS17 temporal frontier FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

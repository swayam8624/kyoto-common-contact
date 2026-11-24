// Pass 19: deterministic multi-regime 3D broadphase fuzz controls.
//
// This is FEATURE-LEVEL adversarial verification, not fabricated FEM
// dynamics. It challenges all production candidate paths against the
// independent exhaustive O(E² + VF) oracle with nonlocal cubic paths,
// zero-thickness inclusive touching, masking, topology epochs, huge
// coordinate shifts, cluster overlap and previously invalid inputs.
#include "kyoto/refit_feature_hierarchy.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
using kyoto::contact::atlas::RefitHierarchy;

static void must(bool condition, const std::string& msg){
    if(!condition)throw std::runtime_error(msg);
}
static f::Mesh fixture(std::uint64_t seed){
    f::Mesh m;m.topology_epoch=seed+1;m.thickness=0;
    for(std::uint64_t i=0;i<18;++i) {
        f::Vertex v;
        v.id=1000+seed*30+i;
        v.object=i/6;
        m.vertices.push_back(v);
    }
    for(std::uint64_t i=0;i<6;++i){
        const std::uint64_t a=m.vertices[i*3].id,
                            b=m.vertices[i*3+1].id,
                            c=m.vertices[i*3+2].id;
        m.faces.push_back({4000+seed*30+i,{a,b,c}});
        m.edges.push_back({5000+seed*30+i*3,{a,b}});
        m.edges.push_back({5001+seed*30+i*3,{b,c}});
        m.edges.push_back({5002+seed*30+i*3,{c,a}});
    }
    return m;
}
static void compare(const f::Mesh& m,RefitHierarchy& cached,
                    RefitHierarchy& baseline,RefitHierarchy& frontier,
                    RefitHierarchy& span,
                    RefitHierarchy& prechecked,
                    RefitHierarchy& policy,
                    kyoto::contact::persistent::OrderedSweep& sap,
                    const std::string& scenario){
    const auto truth=f::exhaustive(m).pairs;
    const auto got=cached.query(m).pairs;
    const auto old=baseline.query(m).pairs;
    const auto temporal=frontier.query(m).pairs;
    const auto two_child_span=span.query(m).pairs;
    const auto child_prechecked=prechecked.query(m).pairs;
    const auto policy_pairs=policy.query(m).pairs;
    must(truth==policy_pairs,scenario+": policy certificate missed oracle");
    const auto order=sap.query(m).pairs;
    const auto cold_sweep=f::sweep_and_prune(m).pairs;
    must(truth==got,scenario+": cached BVH candidate mismatch");
    must(truth==old,scenario+": independent rebuilt metadata BVH mismatch");
    must(truth==temporal,scenario+": bounded temporal frontier mismatch");
    must(truth==two_child_span,scenario+
         ": larger-span two-child VF/EE traversal missed oracle");
    must(truth==child_prechecked,scenario+
         ": child-AABB-prechecked VF/EE traversal missed oracle");
    must(truth==order,scenario+": retained-order SAP mismatch");
    must(truth==cold_sweep,scenario+": rebuilt SAP mismatch");
    const std::size_t budget=std::max(std::size_t{64},
        std::size_t{4}*(m.vertices.size()+m.edges.size()+m.faces.size()));
    must(frontier.pair_cut_size()<=budget &&
         frontier.last_pair_cut_attempted_size()<=budget,
         scenario+": maintained pair cache exceeded memory cap");
}
int main(){
 try {
    constexpr int scenes=28,frames=40;
    std::size_t checked=0,invalid_rejections=0;
    for(std::uint64_t seed=0;seed<scenes;++seed){
        auto m=fixture(seed);
        RefitHierarchy cached(true,false),uncached(false,false),
                       frontier(true,true),
                       span_split(true,false,false,4,true,false,true),
                       child_checks(true,false,false,4,true,false,false,true),
                       policy(true,false,false,4,true,false,false,false,true);
        kyoto::contact::persistent::OrderedSweep sap;
        std::mt19937_64 rng(985217+37*seed);
        std::uniform_real_distribution<double> u(-1,1);
        for(int frame=0;frame<frames;++frame){
            const bool dense=(frame%7<3);
            const double scale=(frame%11==0?1e5:(frame%5==0?1e-4:1.0));
            const double offset=seed%3==0?1000.0:0.0;
            m.thickness=(frame%9==0?0.:(frame%9==1?.001:.025));
            if(frame%13==0){
                m.topology_epoch++;
                // Reorder one non-incident face vertex without changing
                // face count. Each invalidation must completely rebuild.
                m.faces[0].vertices[2]=m.vertices[(frame%2)?4:2].id;
            }
            for(std::size_t i=0;i<m.vertices.size();++i){
                auto& v=m.vertices[i];
                const double region=dense?0.:double(i/3)*2.0;
                const Vec3 a{offset+scale*(region+u(rng)*.2),
                    offset+scale*(region*.1+u(rng)*.2),
                    scale*(u(rng)*.2)};
                const Vec3 b{offset+scale*(region+u(rng)*.2),
                    offset+scale*(region*.1+u(rng)*.2),
                    scale*(u(rng)*.2)};
                v.path.control={a,{a.x+scale*u(rng),
                    a.y+scale*u(rng),a.z+scale*u(rng)},
                    {b.x+scale*u(rng),b.y+scale*u(rng),
                     b.z+scale*u(rng)},b};
                if(frame%9==2 && i%3==0){
                    // Equal/zero extents must preserve touching candidates.
                    v.path.control={a,a,a,a};
                }
                v.category=((i+frame)%2==0)?1:2;
                v.mask=(frame%6==0)?0:3;
            }
            for(auto& e:m.edges){
                e.category=frame%4==0?2:1;
                e.mask=(frame%10==0)?0:3;
            }
            for(auto& fce:m.faces){
                fce.category=frame%4==1?2:1;
                fce.mask=(frame%10==0)?0:3;
            }
            // Nontrivial mixed directional policies, including high
            // category bits. Changes do NOT require the caller to bump
            // the epoch: the complete signature detects them.
            if(frame%3==1){
                for(auto& v:m.vertices){
                    v.category=std::uint64_t{1}<<(rng()%8);
                    v.mask=rng()&255u;
                }
                for(auto& e:m.edges){
                    e.category=std::uint64_t{1}<<(rng()%8);
                    e.mask=rng()&255u;
                }
                for(auto& face:m.faces){
                    face.category=std::uint64_t{1}<<(rng()%8);
                    face.mask=rng()&255u;
                }
            }else if(frame%3==2){
                m.vertices[0].category=std::uint64_t{1}<<63;
                m.faces[0].mask=~std::uint64_t{0};
            }
            compare(m,cached,uncached,frontier,span_split,child_checks,policy,sap,
                "seed="+std::to_string(seed)+"/frame="+std::to_string(frame));
            ++checked;
            if(frame==17){
                // An invalid coordinate MUST throw; after restoration a
                // previous partial cache update must not yield stale pairs.
                const auto saved=m.vertices[5].path.control[2].x;
                m.vertices[5].path.control[2].x=
                    std::numeric_limits<double>::quiet_NaN();
                bool rejected=false;
                try{(void)frontier.query(m);}
                catch(const std::invalid_argument&){rejected=true;}
                must(rejected,"invalid numeric path was silently accepted");
                ++invalid_rejections;
                m.vertices[5].path.control[2].x=saved;
                compare(m,cached,uncached,frontier,span_split,child_checks,policy,sap,
                        "recover-after-NaN");
            }
        }
    }
    std::cout<<"PASS19 oracle-parity independent feature regimes="
             <<scenes<<" frames="<<checked
             <<" safe NaN rejections/recoveries="<<invalid_rejections
             <<" conservative inclusive 3D candidate sets across all "
             <<"five algorithm variants; NO claim of simulated physics.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS19 feature fuzz FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

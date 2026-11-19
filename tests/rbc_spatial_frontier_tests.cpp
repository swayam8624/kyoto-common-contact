// RBC accelerated FRONTIER: independent oracle against Stage-0
// brute force across deliberate falsification regimes.
//
// Not a speedup test: the exact oracle is intentionally quadratic.
// Stage-2 implementation is allowed O(D*N) incremental repairs but must
// ALWAYS return EXACT current reach pairs, not a convenient superset.
#include "ccp/rbc/certified_spatial_frontier.hpp"
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::live::Vec3;
using ccp::rbc::Reach;
using ccp::rbc::CertifiedSpatialFrontier;

void demand(bool ok,const char* reason) {
    if(!ok) throw std::runtime_error(reason);
}
template<typename Fn>
void invalid(Fn fn) {
    try {fn();} catch(const std::invalid_argument&){return;}
    throw std::runtime_error("bad input did not throw std::invalid_argument");
}
Bounds make(int id,double x,double y,double z,double radius=.07) {
    Bounds b{};
    b.id=id;b.min={x-radius,y-radius,z-radius};
    b.max={x+radius,y+radius,z+radius};
    b.category=1;b.mask=1;b.movable=true;
    return b;
}
void move(Bounds& b,double x,double y,double z){
    b.min.x+=x;b.max.x+=x;
    b.min.y+=y;b.max.y+=y;
    b.min.z+=z;b.max.z+=z;
}
void matches(const ccp::rbc::SpatialQuery& result,
             const std::vector<Bounds>& boxes,
             const std::vector<Reach>& reach) {
    const auto truth=ccp::rbc::exhaustive(boxes,reach);
    demand(result.pairs==truth,
           "RBC indexed potential set missed or fabricated an exact reach pair");
    demand(std::adjacent_find(result.pairs.begin(),result.pairs.end())==
           result.pairs.end(),"duplicate output pair");
}
void local_dirty_is_real_local_repair() {
    std::vector<Bounds> b;
    std::vector<Reach> r;
    for(int i=0;i<160;++i){
        b.push_back(make(1000+i,5.*i,0.,0.));
        r.push_back({.04,.06});
    }
    CertifiedSpatialFrontier f;f.initialize(b,r);
    demand(f.potential_pairs()==0,
           "sparse initial reference should have no potential edges");
    auto unchanged=f.query(b);
    matches(unchanged,b,r);
    demand(unchanged.dirty_primitives==0 && unchanged.local_repairs==0 &&
           !unchanged.global_rebuilt,
           "quiet frames incorrectly trigger a rebuild");
    // Exactly one object visits a distant unrelated object. O(N) local
    // repair is necessary; a full rebuild is NOT necessary.
    move(b[0],5,0,0);
    auto touched=f.query(b);
    matches(touched,b,r);
    demand(touched.local_repairs==1 && !touched.global_rebuilt,
           "single dirty object forced an unnecessary global rebuild");
    demand(touched.pairs.size()==1,
           "local repair failed to discover new remote object contact");
    // Masks can turn on AFTER a reference was built; potential pair
    // graph cannot omit formerly masked-out pairs.
    b[1].mask=0;
    matches(f.query(b),b,r);
    b[1].mask=1;
    matches(f.query(b),b,r);
    // A population-wide jump must take the bulk rebuild path.
    for(auto& x:b)move(x,-700,200,-45);
    auto all=f.query(b);
    matches(all,b,r);
    demand(all.global_rebuilt && all.dirty_primitives==b.size(),
           "collective teleport did not bulk invalidate");
}
void bound_inclusive_and_zero_width() {
    std::vector<Bounds> b={make(8,0,0,0,0),make(5,.4,0,0,0)};
    std::vector<Reach> r={{.1,.1},{.1,.1}};
    CertifiedSpatialFrontier f; f.initialize(b,r);
    demand(f.potential_pairs()==1,
           "exact touching padded reference envelopes missing");
    move(b[0],.1,0,0);move(b[1],-.1,0,0);
    matches(f.query(b),b,r);
    demand(f.query(b).pairs.size()==1,
           "inclusive reach boundary must be a candidate");
    b[0].max.x=b[0].min.x-.5;
    invalid([&]{(void)f.query(b);});
}
void translation_compensation_fail_safe() {
    std::vector<Bounds> b{make(3,-2,0,0),make(7,2,0,0)};
    std::vector<Reach> r{{0,.01},{0,.01}};
    CertifiedSpatialFrontier f;f.initialize(b,r);
    for(int t=1;t<=100;++t){
        for(auto& x:b)move(x,12,7,-3);
        auto q=f.query_translated(b,{12.*t,7.*t,-3.*t});
        matches(q,b,r);
        demand(q.dirty_primitives==0&&q.local_repairs==0&&
               !q.global_rebuilt,"verified common rigid motion forced repair");
    }
    demand(f.global_builds()==1,
           "coherent translation unexpectedly exhausted motion skin");
    move(b[1],-4,0,0);
    auto hit=f.query_translated(b,{1200,700,-300});
    matches(hit,b,r);
    demand(hit.local_repairs==1,
           "wrong global hint omitted real residual invalidation");
    demand(hit.pairs.size()==1,"relative translation missed collision");
    // Changing the convention to the stationary reference must be safe,
    // even though it can be more expensive.
    matches(f.query(b),b,r);
}
void randomized_dense_and_sparse() {
    std::mt19937_64 rng(0x654321ab);
    std::uniform_real_distribution<double> p(-3.,3.),v(-.07,.07);
    std::uniform_real_distribution<double> reach(0.,.08);
    std::size_t repaired=0,rebuilt=0,exact=0;
    for(int scene=0;scene<32;++scene) {
        const int n=(scene%4==0?96:64);
        std::vector<Bounds> boxes;
        std::vector<Reach> radii;
        for(int i=0;i<n;++i) {
            // Dense scenes, degenerate zero-extent and sparse spatial
            // populations have very different frontier sizes.
            double x=p(rng),y=p(rng),z=p(rng);
            if(scene%3==0){x*=.2;y*=.2;z*=.2;}
            if(scene%3==1){x*=8.;y*=8.;z*=8.;}
            boxes.push_back(make(100+i*3,x,y,z,i%17==0?0.:.11));
            radii.push_back({reach(rng),.12});
        }
        CertifiedSpatialFrontier f;f.initialize(boxes,radii);
        for(int frame=0;frame<80;++frame) {
            for(int i=0;i<n;++i) {
                move(boxes[i],v(rng),v(rng),v(rng));
                if((frame+i+scene)%23==0)
                    boxes[i].mask=(boxes[i].mask==0?1u:0u);
                if((frame+i+scene)%47==0)
                    boxes[i].category=(boxes[i].category==0?1u:0u);
            }
            const auto result=f.query(boxes);
            matches(result,boxes,radii);
            repaired+=result.local_repairs;
            rebuilt+=std::size_t(result.global_rebuilt);
            exact+=result.pairs.size();
        }
    }
    demand(repaired>0 && rebuilt>0 && exact>0,
           "random property tests failed to cover repair/rebuild/candidates");
    std::cout<<"PASS RBC indexed frontier 32x80 randomized audited trajectories, "
             <<"local repairs="<<repaired<<" bulk rebuilds="<<rebuilt
             <<" actual contact candidates="<<exact<<"\n";
}
void large_sparse_sap_oracle() {
    // 2304 real candidates can be checked against INDEPENDENT
    // current SAP at each step without constructing a quadratic
    // brute-force oracle on a large sparse N.
    std::vector<Bounds> boxes;std::vector<Reach> radii;
    for(int i=0;i<2304;++i){
        const double x=(i%48)*1.7;
        const double y=((i/48)%48)*1.7;
        boxes.push_back(make(i,x,y,0,.25));
        radii.push_back({0.,.05});
    }
    CertifiedSpatialFrontier f;f.initialize(boxes,radii);
    for(int frame=0;frame<50;++frame){
        for(int i=0;i<2304;++i){
            if(i%101==0)move(boxes[i],.023,0,0);
        }
        auto result=f.query(boxes);
        const auto sap=ccp::final_broadphase::PersistentSAP{}.query(boxes);
        demand(result.pairs==sap,
               "large 2304-object RBC indexed/current SAP parity violated");
    }
}
void rejects_identity_and_bad_geometry() {
    std::vector<Bounds> b{make(1,0,0,0),make(2,2,0,0)};
    std::vector<Reach> r{{0,.1},{0,.1}};
    CertifiedSpatialFrontier f;
    invalid([&]{f.initialize({b[0],b[0]},r);});
    f.initialize(b,r);
    std::swap(b[0],b[1]);
    invalid([&]{(void)f.query(b);});
    std::swap(b[0],b[1]);
    b[0].min.x=std::numeric_limits<double>::quiet_NaN();
    invalid([&]{(void)f.query(b);});
    b[0]=make(1,0,0,0);
    invalid([&]{(void)f.query_translated(
        b,{std::numeric_limits<double>::infinity(),0,0});});
}
} // namespace
int main() {
    try{
        local_dirty_is_real_local_repair();
        bound_inclusive_and_zero_width();
        translation_compensation_fail_safe();
        randomized_dense_and_sparse();
        large_sparse_sap_oracle();
        rejects_identity_and_bad_geometry();
        return 0;
    } catch(const std::exception& e){
        std::cerr<<"FAIL RBC indexed frontier: "<<e.what()<<"\n";
        return 1;
    }
}

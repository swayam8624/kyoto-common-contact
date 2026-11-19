// RBC Stage 1: conservative linear-swept AABB candidate contract.
//
// This DOES NOT implement CCD time-of-impact or triangle contact.
// Tests prove that a pair whose instantaneous reach-AABBs may become
// active during a linear trajectory is not dropped by the swept set.
// Includes cross-through at midstep, intentionally conservative
// positives, adversarial curved path counterexample and random sweeps.
#include "ccp/rbc/reach_budget.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::rbc::Reach;
using ccp::rbc::Pairs;

void require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}

template <typename F>
void requires_invalid(F&& f, const char* reason) {
    try {f();} catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(reason);
}

Bounds box(int id, double x, double y, double z, double half) {
    Bounds b{};
    b.id=id;
    b.min={x-half,y-half,z-half};
    b.max={x+half,y+half,z+half};
    b.category=1;
    b.mask=~std::uint32_t{0};
    return b;
}

Bounds interpolate_box(const Bounds& a, const Bounds& b, double t) {
    require(a.id==b.id, "interpolation identity changed");
    Bounds r=a;
    auto mix=[t](double x,double y){return x+(y-x)*t;};
    r.min={mix(a.min.x,b.min.x),mix(a.min.y,b.min.y),mix(a.min.z,b.min.z)};
    r.max={mix(a.max.x,b.max.x),mix(a.max.y,b.max.y),mix(a.max.z,b.max.z)};
    return r;
}

void assert_superset(const Pairs& swept,const Pairs& instantaneous) {
    require(std::includes(swept.begin(),swept.end(),
                          instantaneous.begin(),instantaneous.end()),
            "a linear-motion reach-AABB activation was missed");
}

void midstep_crossing() {
    std::vector<Bounds> a={box(8,-4,0,0,.2),box(3,4,0,0,.2)};
    std::vector<Bounds> b={box(8,4,0,0,.2),box(3,-4,0,0,.2)};
    std::vector<Reach> r={{0,0},{0,0}};
    require(ccp::rbc::exhaustive(a,r).empty(),
            "cross-through scene initially contacting");
    require(ccp::rbc::exhaustive(b,r).empty(),
            "cross-through scene finally contacting");
    const auto h=ccp::rbc::linear_swept_reach_candidates(a,b,r);
    require(h.size()==1 && h.front().a==3 && h.front().b==8,
            "swept midstep pass-through not detected");
    const std::vector<Bounds> mid={
        interpolate_box(a[0],b[0],.5),
        interpolate_box(a[1],b[1],.5)
    };
    assert_superset(h,ccp::rbc::exhaustive(mid,r));
}

void conservative_false_positive() {
    std::vector<Bounds> a={box(1,0,0,0,.2),box(2,3,0,0,.2)};
    std::vector<Bounds> b={box(1,12,0,0,.2),box(2,15,0,0,.2)};
    const std::vector<Reach> r={{0,0},{0,0}};
    const auto swept=ccp::rbc::linear_swept_reach_candidates(a,b,r);
    require(swept.size()==1,
            "expected conservative envelope positive absent");
    // Their relative displacement is ZERO. The instantaneous pair is
    // never actually active. Swept-box overlap is a superset, not CCD.
    for(int t=0;t<=20;++t) {
        const double s=double(t)/20.;
        require(ccp::rbc::exhaustive({
                    interpolate_box(a[0],b[0],s),
                    interpolate_box(a[1],b[1],s)},r).empty(),
                "same-speed separated pair became active");
    }
}

void curved_motion_counterexample() {
    // Both end-point boxes are separated and stationary, but a
    // curved midpoint path could bring one into contact. Therefore
    // the current endpoint-hull theorem MUST NOT be applied to
    // unrestricted nonlinear trajectories.
    std::vector<Bounds> a={box(1,-3,0,0,.25),box(2,3,0,0,.25)};
    const std::vector<Reach> r={{0,0},{0,0}};
    const auto endpoints=ccp::rbc::linear_swept_reach_candidates(a,a,r);
    require(endpoints.empty(),
            "static endpoints unexpectedly intersect");
    std::vector<Bounds> curved_mid={box(1,3,0,0,.25),a[1]};
    require(!ccp::rbc::exhaustive(curved_mid,r).empty(),
            "nonlinear midpoint counterexample not constructed");
    // This is a documented limitation, not a passing CCD test.
}

void invalid_input() {
    const auto a=box(1,0,0,0,.2);
    auto b=a;b.id=77;
    const std::vector<Reach> r={{0,0}};
    requires_invalid([&] {
        (void)ccp::rbc::linear_swept_reach_candidates({a},{b},r);
    },"changing stable ID during a swept step accepted");
    b=a;b.mask=0;
    requires_invalid([&] {
        (void)ccp::rbc::linear_swept_reach_candidates({a},{b},r);
    },"changing mask within a swept step accepted");
    requires_invalid([&] {
        (void)ccp::rbc::linear_swept_reach_candidates({a},{a},{});
    },"wrong reach cardinality accepted");
}

void random_linear_sweeps() {
    std::mt19937_64 rng(0x6c206be);
    std::uniform_real_distribution<double> xyz(-3,3);
    std::uniform_real_distribution<double> velocity(-2.5,2.5);
    std::uniform_real_distribution<double> reach(0,.15);
    const int scenes=48, count=32, checkpoints=25;
    std::size_t observed=0;
    for(int scene=0;scene<scenes;++scene) {
        std::vector<Bounds> start,end;
        std::vector<Reach> radii;
        for(int i=0;i<count;++i) {
            auto b=box(1000+i,xyz(rng),xyz(rng),xyz(rng),.1+.07*(i%4));
            auto e=b;
            const double dx=velocity(rng),dy=velocity(rng),dz=velocity(rng);
            e.min.x+=dx;e.max.x+=dx;
            e.min.y+=dy;e.max.y+=dy;
            e.min.z+=dz;e.max.z+=dz;
            if (i%17==0) { b.mask=0;e.mask=0; }
            radii.push_back({reach(rng),.2}); // skin must NOT leak into swept reach
            start.push_back(b);
            end.push_back(e);
        }
        const auto swept=ccp::rbc::linear_swept_reach_candidates(start,end,radii);
        require(std::is_sorted(swept.begin(),swept.end()),
                "swept output not sorted");
        require(std::adjacent_find(swept.begin(),swept.end())==swept.end(),
                "swept output duplicate pairs");
        for(int ti=0;ti<=checkpoints;++ti) {
            std::vector<Bounds> current;
            const double t=double(ti)/checkpoints;
            for(int i=0;i<count;++i)
                current.push_back(interpolate_box(start[i],end[i],t));
            const auto actual=ccp::rbc::exhaustive(current,radii);
            observed+=actual.size();
            assert_superset(swept,actual);
        }
    }
    require(observed>0,"random scenes did not exercise any activation");
    std::cout<<"PASS: linear swept reach AABB contract across "
             <<scenes<<" distinct scenes, "<<checkpoints+1
             <<" checkpoints each; actual pairs observed "<<observed<<"\n";
}
} // namespace

int main() {
    try {
        midstep_crossing();
        conservative_false_positive();
        curved_motion_counterexample();
        invalid_input();
        random_linear_sweeps();
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL RBC swept reach contract: "<<e.what()<<"\n";
        return 1;
    }
}

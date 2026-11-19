// RBC Stage 0: rigorous discrete reach-AABB certificate regression.
//
// This is an O(N^2) CERTIFICATE ORACLE, not an optimized production engine.
// Every state is independently checked against an exhaustive CURRENT
// reach-AABB oracle, including masks, equality touching, many random
// invalidations, zero-thickness geometry, and identity abuse.
#include "ccp/rbc/reach_budget.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::live::Vec3;
using ccp::rbc::Reach;

void require(bool condition, const char* problem) {
    if (!condition) throw std::runtime_error(problem);
}
template<class Fn>
void must_throw(Fn fn, const char* problem) {
    try { fn(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(problem);
}
Bounds box(int id, double x, double y, double z, double half=.5) {
    return {id, {x-half,y-half,z-half},
                {x+half,y+half,z+half},
                std::uint32_t{1}, ~std::uint32_t{0}, true};
}
void shift(Bounds& b, double dx,double dy,double dz) {
    b.min.x+=dx; b.max.x+=dx;
    b.min.y+=dy; b.max.y+=dy;
    b.min.z+=dz; b.max.z+=dz;
}
void check(const std::vector<Bounds>& boxes,
           ccp::rbc::CertifiedReferenceList& ref,
           const std::vector<Reach>& r,
           const char* message) {
    const auto actual = ref.query(boxes);
    const auto truth = ccp::rbc::exhaustive(boxes,r);
    require(actual.pairs==truth,message);
}
void test_touch_and_masks() {
    // Initial faces have unit gap. Two independent 0.5 skins close exactly
    // that unit gap, so inclusive face-touch pair MUST be present.
    std::vector<Bounds> b{box(5,0,0,0),box(2,2,0,0)};
    std::vector<Reach> r{{0,.5},{0,.5}};
    ccp::rbc::CertifiedReferenceList cert;
    cert.initialize(b,r);
    require(cert.potential_pairs()==1,"equality boundary excluded from certificate");
    require(cert.query(b).pairs.empty(),"separated initial pair not filtered");
    shift(b[0],.5,0,0);
    shift(b[1],-.5,0,0);
    auto q=cert.query(b);
    require(!q.rebuilt,"exact allowed skin should not rebuild");
    require(q.pairs.size()==1 && q.pairs[0].a==2 && q.pairs[0].b==5,
            "inclusive touching pair missing or not canonical");
    b[1].mask=0;
    check(b,cert,r,"dynamic category/mask collision exclusion incorrect");
    b[1].mask=~std::uint32_t{0};
    check(b,cert,r,"dynamic collision mask reenable omitted a pair");
    shift(b[0],.001,0,0);
    q=cert.query(b);
    require(q.rebuilt,"crossed skin invalidation did not trigger exact rebuild");
    require(q.pairs==ccp::rbc::exhaustive(b,r),
            "rebuilt exact reference omitted candidate");
    auto flipped=b;
    std::swap(flipped[0],flipped[1]);
    must_throw([&]{(void)cert.query(flipped);},
               "reordered stable IDs must require explicit initialize");
}
void test_budget_ledger() {
    ccp::rbc::BudgetLedger ledger(.2);
    require(ledger.reserve(.1),"first reserved displacement");
    require(ledger.reserve(.1),"exact budget boundary invalid");
    require(!ledger.reserve(.0000001),"budget should reject overspend");
    require(std::abs(ledger.spent()-.2)<1e-12,"budget consumed incorrectly");
    ledger.reset();
    require(ledger.reserve(.2),"reset should restore budget");
    must_throw([&]{(void)ledger.reserve(-.1);},
               "negative displacement invalid");
    must_throw([&]{(void)ledger.reserve(
        std::numeric_limits<double>::infinity());},
        "infinite displacement invalid");
}
void test_invalid_inputs_and_linear_sweep() {
    auto a=box(10,0,0,0);
    auto b=box(10,2,0,0);
    auto swept=ccp::rbc::linear_endpoint_hull(a,b);
    require(swept.min.x==a.min.x && swept.max.x==b.max.x,
            "linear swept hull does not include endpoints");
    // Another axis's target shape could be different.
    shift(b,0,3,-1);
    swept=ccp::rbc::linear_endpoint_hull(a,b);
    require(swept.min.z==b.min.z && swept.max.y==b.max.y,
            "linear swept hull incorrectly used one axis");
    auto wrongId=b;wrongId.id=11;
    must_throw([&]{(void)ccp::rbc::linear_endpoint_hull(a,wrongId);},
               "changed swept primitive id accepted");
    ccp::rbc::CertifiedReferenceList cert;
    must_throw([&]{cert.initialize({a,a},{{},{} });},
               "duplicate primitive ID accepted");
    a.min.x=std::numeric_limits<double>::quiet_NaN();
    must_throw([&]{cert.initialize({a},{{}});},"NaN AABB accepted");
    a=box(0,0,0,0);
    a.max.y=-10;
    must_throw([&]{cert.initialize({a},{{}});},"inverted AABB accepted");
    a=box(0,0,0,0);
    must_throw([&]{cert.initialize({a},{{0,-1}});},"negative skin accepted");
    must_throw([&]{cert.initialize({a},{{-1,0}});},"negative reach accepted");
}
void test_randomized_reference_and_pair_oracle() {
    std::mt19937_64 rng(709293813);
    std::uniform_real_distribution<double> coord(-6.0,6.0);
    std::uniform_real_distribution<double> drift(-.11,.11);
    std::uniform_real_distribution<double> radii(0.,.1);
    for (int scene=0;scene<48;++scene) {
        std::vector<Bounds> boxes;
        std::vector<Reach> reaches;
        for(int i=0;i<36;++i) {
            boxes.push_back(box(1000+i,coord(rng),coord(rng),coord(rng),
                                .08+.12*double(i%4)));
            reaches.push_back({radii(rng),.18});
        }
        ccp::rbc::CertifiedReferenceList cert;
        cert.initialize(boxes,reaches);
        for (int frame=0;frame<85;++frame) {
            for (std::size_t i=0;i<boxes.size();++i) {
                shift(boxes[i],drift(rng),drift(rng),drift(rng));
                if ((frame+i)%31==0)
                    boxes[i].mask = boxes[i].mask==0?
                        ~std::uint32_t{0}:std::uint32_t{0};
                if ((frame+i)%53==0)
                    boxes[i].category = boxes[i].category==0?
                        std::uint32_t{1}:std::uint32_t{0};
            }
            check(boxes,cert,reaches,"RBC random exact candidate mismatch");
        }
        require(cert.rebuilds()>1,
                "randomized tests failed to exercise actual invalidations");
    }
}
void test_lipschitz_bound() {
    std::mt19937 rng(1999);
    std::uniform_real_distribution<double> coord(-7.,7.);
    std::uniform_real_distribution<double> drift(-.4,.4);
    for (int i=0;i<20000;++i) {
        auto a=box(1,coord(rng),coord(rng),coord(rng));
        auto b=box(2,coord(rng),coord(rng),coord(rng));
        const double old=ccp::rbc::linf_box_gap(a,b);
        auto c=a,d=b;
        shift(c,drift(rng),drift(rng),drift(rng));
        shift(d,drift(rng),drift(rng),drift(rng));
        const double neu=ccp::rbc::linf_box_gap(c,d);
        const double bound=ccp::rbc::face_motion(a,c)+
                           ccp::rbc::face_motion(b,d);
        require(std::abs(old-neu)<=bound+1e-11,
                "l_infinity gap Lipschitz lemma violated");
    }
}
} // namespace

int main() {
    try {
        test_touch_and_masks();
        test_budget_ledger();
        test_invalid_inputs_and_linear_sweep();
        test_randomized_reference_and_pair_oracle();
        test_lipschitz_bound();
        std::cout << "PASS: RBC Stage 0 exact reach oracle; "
                  << "172800 randomized frame-pair equality decisions; "
                  << "20000 Lipschitz trials; safe invalidation, "
                  << "mask changes, linear sweeps and strict input validation\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: RBC Stage 0: " << e.what() << "\n";
        return 1;
    }
}

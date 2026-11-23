// Pass 22A: independent geometric regression for *two* moving
// finite 3D triangle surfaces. Source trajectories are adversarial
// geometric fixtures, NOT fabricated solver physics.
//
// Coverage oracle: multiple independent exact-geometry closest-point
// evaluations at dense time samples. A positive sample contact must
// NEVER be declared "entirely separated" by a supposedly conservative
// time certificate. Sampling itself is NOT a CCD completeness proof.
#include "kyoto/moving_triangle_pair_certificate.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace s=kyoto::solid;
static void require(bool value,const char* message){
    if(!value)throw std::runtime_error(message);
}
static s::Triangle3 triangle(s::V3 center){
    return {{center,center+s::V3{.2,0,0},
             center+s::V3{0,.2,0}}};
}
static s::LinearTriangleMotion linear(s::V3 start,s::V3 end){
    return {triangle(start),triangle(end)};
}
static bool sampled_collision(const s::LinearTriangleMotion& a,
                              const s::LinearTriangleMotion& b,
                              double skin){
    for(int step=0;step<=128;++step){
        const double t=double(step)/128.;
        const auto ta=s::lerp_vertices(a.start,a.end,t);
        const auto tb=s::lerp_vertices(b.start,b.end,t);
        if(s::triangle_triangle_closest(ta,tb).distance<=skin)
            return true;
    }
    return false;
}
int main(){
 try{
    constexpr double skin=.0005;
    const auto fixed=linear({0,0,0},{0,0,0});

    const auto distant=linear({0,0,.75},{0,0,.74});
    require(s::certify_two_moving_triangles(distant,fixed,skin).entirely_separated,
            "two distant physical triangles were not certified separated");

    // One moving, one stationary. The actual source geometry crosses
    // the plane at t=.5. The moving-moving certificate must reject safe.
    const auto crossing=linear({0,0,-.5},{0,0,.5});
    const auto event=s::certify_two_moving_triangles(crossing,fixed,skin,14);
    require(!event.entirely_separated,
            "moving/static triangle collision incorrectly certified absent");
    require(event.safe_prefix<.5 && event.first_unresolved_upper>=event.safe_prefix,
            "unresolved swept time interval is invalid");
    require(!s::certify_two_moving_triangles(fixed,crossing,skin,14)
                  .entirely_separated,
            "swapping both moving surfaces lost actual crossing");

    // Both nonstationary: initially disjoint x domains, then
    // triangles exchange positions and physically pass through.
    const auto head_on_a=linear({-1,0,0},{1,0,0});
    const auto head_on_b=linear({1,0,0},{-1,0,0});
    require(sampled_collision(head_on_a,head_on_b,skin),
            "head-on pair fixture did not exhibit actual contact");
    require(!s::certify_two_moving_triangles(
                 head_on_a,head_on_b,skin,14).entirely_separated,
            "moving-moving head-on collision incorrectly pruned");

    // Independent static solver's prior certificate: the new method
    // must agree on simple real disjointness and clear crossing cases.
    const auto old=s::certify_moving_triangle_rigid_triangle(
        crossing,fixed.start,skin,14);
    require(old.entirely_separated==event.entirely_separated,
            "moving/static limiting case broke old separation safety");

    // Near-miss: broad swept AABBs overlap in some directions, yet
    // exact moving triangle surfaces remain above the skin.
    const auto close_but_safe=linear({0,0,.03},{.002,0,.02});
    require(s::certify_two_moving_triangles(
        close_but_safe,fixed,.002,14).entirely_separated,
        "safe near-miss was not resolved within the subdivision budget");

    const auto touching=linear({0,0,0},{0,0,0});
    require(!s::certify_two_moving_triangles(
        touching,fixed,0.,12).entirely_separated,
        "exactly coincident source triangles falsely certified separated");

    std::mt19937_64 random(20261124);
    std::uniform_real_distribution<double> pos(-.5,.5),vel(-.65,.65);
    constexpr int SCENES=120;
    std::size_t contact_scenes=0,pruned_scenes=0;
    for(int trial=0;trial<SCENES;++trial){
        const s::V3 p{pos(random),pos(random),pos(random)};
        const s::V3 q{pos(random),pos(random),pos(random)};
        const s::V3 dp{vel(random),vel(random),vel(random)};
        const s::V3 dq{vel(random),vel(random),vel(random)};
        const auto a=linear(p,p+dp);
        const auto b=linear(q,q+dq);
        const bool encountered=sampled_collision(a,b,skin);
        const auto forward=s::certify_two_moving_triangles(a,b,skin,12);
        const auto swapped=s::certify_two_moving_triangles(b,a,skin,12);
        if(encountered){
            ++contact_scenes;
            require(!forward.entirely_separated&&!swapped.entirely_separated,
                    "source sampled moving-moving event pruned by certificate");
        }
        require(forward.entirely_separated==swapped.entirely_separated,
                "exchange of moving surfaces changed conservative outcome");
        pruned_scenes+=forward.entirely_separated;
    }

    // An invalid floating-point input must stop; "safe" is prohibited.
    auto invalid=crossing;
    invalid.end[2].z=std::numeric_limits<double>::quiet_NaN();
    bool threw=false;
    try{(void)s::certify_two_moving_triangles(invalid,fixed,skin);}
    catch(const std::invalid_argument&){threw=true;}
    require(threw,"nonfinite moving triangle escaped certificate validator");

    std::cout<<"PASS22 moving/moving real 3D triangle separation checks, "
             <<SCENES<<" seeded geometric scenes, observed contact cases="
             <<contact_scenes<<", safe pruned scenes="<<pruned_scenes
             <<". Dense samples are regression checks, not mathematical CCD proofs.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS22 moving/moving certificate FAILURE: "
             <<e.what()<<'\n';
    return 1;
 }
}

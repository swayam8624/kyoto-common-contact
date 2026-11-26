#include "kyoto/initial_tet_separation.hpp"
#include <iostream>
#include <random>
using namespace kyoto::geometry;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Tetrahedron shift(Tetrahedron t,Point d){for(auto& p:t)for(int i=0;i<3;++i)p[i]+=d[i];return t;}
int main(){
    const Tetrahedron a{{{0,0,0},{1,0,0},{0,1,0},{0,0,1}}};
    need(certify_initial_tet_separation(a,shift(a,{2,0,0})).separated,"axis separation");
    // AABBs overlap, but the plane x+y+z=1 separates these convex solids.
    auto diagonal=shift(a,{.5,.5,.5});
    need(certify_initial_tet_separation(a,diagonal).separated,"oblique separation");
    Tetrahedron inner=a;for(auto& p:inner)for(auto& x:p)x=.1+.1*x;
    need(!certify_initial_tet_separation(a,inner).separated,"nested volume incorrectly cleared");
    need(!certify_initial_tet_separation(a,shift(a,{1,0,0})).separated,"touch incorrectly cleared");
    need(!certify_initial_tet_separation(a,a).separated,"identical volume incorrectly cleared");
    std::mt19937_64 rng(3802026);std::uniform_real_distribution<double> u(.01,.2);
    std::size_t overlapping=0,separated=0;
    for(int trial=0;trial<1000;++trial){
        // Exact contained construction (each coordinate .01.. .2,
        // then a scale < .2). Every vertex is strictly inside a.
        Tetrahedron b=a;Point d{u(rng),u(rng),u(rng)};const auto s=u(rng);
        for(auto& p:b)for(int i=0;i<3;++i)p[i]=d[i]+s*p[i];
        need(!certify_initial_tet_separation(a,b).separated,"random containment false clear");
        need(!certify_initial_tet_separation(b,a).separated,"swapped containment false clear");
        overlapping+=2;
        b=shift(a,{.4+u(rng),.4+u(rng),.4+u(rng)});
        need(certify_initial_tet_separation(a,b).separated,"random oblique separation unresolved");
        need(certify_initial_tet_separation(b,a).separated,"swapped oblique separation unresolved");
        separated+=2;
    }
    const auto far=shift(a,{1e12,-1e12,1e12});
    need(certify_initial_tet_separation(far,shift(far,{.5,.5,.5})).separated,"large translation separation");
    std::cout<<"PASS38A contained_cases="<<overlapping<<" separated_cases="<<separated<<" plus analytic controls\n";
}

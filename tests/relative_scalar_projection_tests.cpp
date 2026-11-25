#include "kyoto/rounded_relative_triangle_certificate.hpp"
#include <bit>
#include <cstdint>
#include <iostream>
#include <random>
using namespace kyoto::solid;
using namespace relative_hull_detail;
static void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){
    std::mt19937_64 rng(39391126);
    auto num=[&](){double x;do{x=std::bit_cast<double>(rng());}while(!std::isfinite(x));return x;};
    for(int i=0;i<200000;++i){const double x=num(),y=num(),z=num();const Interval a{std::min(x,y),std::max(x,y)};
        const auto r=product(a,exact(z)),s=scalar_product(a,z);need(r.lo==s.lo&&r.hi==s.hi,"scalar interval mismatch");}
    const auto tiny=std::numeric_limits<double>::denorm_min();for(double x:{-tiny,-0.,0.,tiny,1.,-1.})for(double y:{-tiny,-0.,0.,tiny,1.,-1.}){const auto a=exact(x);const auto r=product(a,exact(y)),s=scalar_product(a,y);need(r.lo==s.lo&&r.hi==s.hi,"subnormal interval mismatch");}
    std::uniform_int_distribution<int>d(-16,16);auto point=[&](){return V3{d(rng)/16.,d(rng)/16.,d(rng)/16.};};
    std::size_t clears=0,unknown=0;
    for(int i=0;i<2000;++i){LinearTriangleMotion a,b;for(auto* m:{&a,&b})for(auto* t:{&m->start,&m->end})for(auto& p:*t)p=point();
        if(i%4==0){a.start[1]=a.start[2]=a.start[0];a.end[1]=a.end[2]=a.end[0];}
        if(i%4==1){a.start[2]=a.start[1];a.end[2]=a.end[1];b.start[2]=b.start[1];b.end[2]=b.end[1];}
        if(i%3==0)for(auto* t:{&b.start,&b.end})for(auto& p:*t)p.x+=3.;
        const auto r=certify_rounded_relative_triangle_pair(a,b,.01,4,false),s=certify_rounded_relative_triangle_pair(a,b,.01,4,true);
        need(r.entirely_separated==s.entirely_separated&&r.nodes==s.nodes&&r.directions==s.directions&&r.interval_projections==s.interval_projections&&r.max_depth==s.max_depth&&r.lower_clearance==s.lower_clearance,"full certificate mismatch");clears+=r.entirely_separated;unknown+=!r.entirely_separated;
    }
    need(clears>0&&unknown>0,"vacuous path validation");std::cout<<"PASS 200000 scalar interval pairs, 2000 full certificates: clear="<<clears<<" unresolved="<<unknown<<'\n';
}

#include "kyoto/exact_swept_tet_certificate.hpp"
#include <iostream>
#include <random>
using namespace kyoto::geometry;
using namespace kyoto::geometry::swept_tet_detail;
int main(){
    std::mt19937_64 rng(3902028);std::uniform_int_distribution<int> d(-1000,1000);
    auto point=[&](int shift){Q p;Z factor=0;boost::multiprecision::bit_set(factor,unsigned(shift));for(auto& v:p){v=d(rng);v*=factor;}return p;};
    for(int i=0;i<10000;++i){
        const int shift=i%3==0?0:i%3==1?64:1074;
        const Line a{point(shift),point(shift)},b{point(shift),point(shift)},c{point(shift),point(shift)};
        if(AxisCoefficients(a,b).project(c)!=det_coeff(a,b,c))throw std::runtime_error("exact coefficient mismatch");
    }
    const Tetrahedron a{{{0,0,0},{1,0,0},{0,1,0},{0,0,1}}};
    for(int i=0;i<1000;++i){
        auto b=a,c=a;
        for(auto* t:{&b,&c})for(auto& p:*t)for(auto& x:p)x+=double(d(rng))/512.;
        const ExactLinearTetMotion x{a,a,{0,1,2,3}},y{b,c,{4,5,6,7}};
        const auto r=certify_exact_swept_tet_pair(x,y,2,false),s=certify_exact_swept_tet_pair(x,y,2,true);
        if(r.certified!=s.certified||r.nodes!=s.nodes||r.axes!=s.axes||r.polynomials!=s.polynomials||r.max_depth!=s.max_depth)
            throw std::runtime_error("factored pair receipt mismatch");
    }
    std::cout<<"PASS 10000 exact coefficient comparisons and 1000 full swept pair receipt comparisons\n";
}

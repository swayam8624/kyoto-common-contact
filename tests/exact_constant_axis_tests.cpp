#include "kyoto/exact_axis_polynomial.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
using Z=boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>,boost::multiprecision::et_off>;
using Q=std::array<Z,3>;
struct Line{Q a,b;};
using Axis=kyoto::geometry::exact_axis::AxisPolynomial<Z,Line>;
using Coeff=Axis::Coeff;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
// Independent eight-determinant expansion, not the factored helper.
static Z det(const Q& a,const Q& b,const Q& c){return a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0]);}
static Coeff truth(const Line& e,const Line& f,const Line& r){return {3*det(e.a,f.a,r.a),det(e.b,f.a,r.a)+det(e.a,f.b,r.a)+det(e.a,f.a,r.b),det(e.a,f.b,r.b)+det(e.b,f.a,r.b)+det(e.b,f.b,r.a),3*det(e.b,f.b,r.b)};}
struct Scene{Line e,f;std::array<Line,8> rays;};
int main(int argc,char** argv){
    std::mt19937_64 rng(3912026);std::uniform_int_distribution<int> d(-1024,1024);
    auto point=[&](int shift){Q p;Z factor=0;boost::multiprecision::bit_set(factor,unsigned(shift));for(auto& x:p){x=d(rng);x*=factor;}return p;};
    std::array<std::vector<Scene>,3> groups;
    std::size_t checks=0;
    for(int group=0;group<3;++group)for(int i=0;i<1000;++i){
        const int shift=i%3==0?0:i%3==1?64:1074;
        Scene s{};s.e={point(shift),point(shift)};s.f={point(shift),point(shift)};
        if(group==0){s.e.b=s.e.a;s.f.b=s.f.a;}
        if(group==1){s.e.b=s.e.a;for(int k=0;k<3;++k)s.f.b[k]=s.f.a[k]+s.e.a[k];}
        for(auto& r:s.rays)r={point(shift),point(shift)};
        const Axis a(s.e,s.f,false),b(s.e,s.f,true);
        if(group<2)need(b.constant,"constant normal not detected");
        for(const auto& r:s.rays){const auto c=truth(s.e,s.f,r);need(a.project(r)==c&&b.project(r)==c,"exact coefficient mismatch");++checks;}
        groups[group].push_back(std::move(s));
    }
    // Include zero and sign-changing normal controls.
    const Line a{{1,0,0},{-1,0,0}},b{{0,1,0},{0,1,0}},r{{0,0,1},{0,0,-1}};
    need(!Axis(a,b,true).constant&&Axis(a,b,true).project(r)==truth(a,b,r),"nonconstant sign-changing normal misclassified");
    const Line zero{};need(Axis(zero,b,true).project(r)==truth(zero,b,r),"zero axis mismatch");
    std::cerr<<"PASS exact coefficient comparisons="<<checks<<"; static, moving-constant, general axes and controls\n";
    if(argc!=2||std::string(argv[1])!="--timing")return 0;
    using Clock=std::chrono::steady_clock;
    std::cout<<"group,block,slot,arm,scenes,projections,elapsed_ns,checksum\n";
    for(int group=0;group<3;++group){
        Z reference_checksum;bool first=true;
        for(int block=-1;block<5;++block)for(int slot=0;slot<4;++slot){
            const bool fast=(slot==0||slot==3)==(block%2==0);
            Z checksum=0;
            const auto begin=Clock::now();
            for(const auto& s:groups[group]){const Axis axis(s.e,s.f,fast);for(const auto& ray:s.rays){const auto c=axis.project(ray);for(const auto& x:c)checksum+=x;}}
            const auto end=Clock::now();
            if(first){reference_checksum=checksum;first=false;}else need(checksum==reference_checksum,"timed checksum diverged");
            const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count();need(ns>0,"invalid duration");
            if(block>=0)std::cout<<group<<','<<block<<','<<slot<<','<<(fast?"constant_fast":"factored")<<','<<groups[group].size()<<','<<groups[group].size()*8<<','<<ns<<','<<(checksum%1000003)<<'\n';
        }
    }
}

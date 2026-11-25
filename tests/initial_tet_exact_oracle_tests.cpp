// Independent bounded-integer SAT oracle. No rounded arithmetic or
// production projection routines are used in this reference.
#include "kyoto/initial_tet_separation.hpp"
#include <cstdint>
#include <iostream>
#include <random>
using namespace kyoto::geometry;
using Q=std::array<std::int64_t,3>;
using QT=std::array<Q,4>;
static Q sub(Q a,Q b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
static Q cross(Q a,Q b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
static std::int64_t dot(Q a,Q b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static bool valid(const QT& t){return dot(sub(t[1],t[0]),cross(sub(t[2],t[0]),sub(t[3],t[0])))!=0;}
// Input coordinates are bounded by 128. Cross products, dot products,
// and determinants therefore fit comfortably inside signed int64.
static bool exact_separated(const QT& a,const QT& b){
    auto axis=[&](Q n){
        auto alo=dot(n,a[0]),ahi=alo,blo=dot(n,b[0]),bhi=blo;
        for(auto p:a){const auto v=dot(n,p);alo=std::min(alo,v);ahi=std::max(ahi,v);}
        for(auto p:b){const auto v=dot(n,p);blo=std::min(blo,v);bhi=std::max(bhi,v);}
        return ahi<blo||bhi<alo;
    };
    constexpr int faces[4][3]={{0,1,2},{0,1,3},{0,2,3},{1,2,3}};
    for(const auto* t:{&a,&b})for(const auto& f:faces)
        if(axis(cross(sub((*t)[f[1]],(*t)[f[0]]),sub((*t)[f[2]],(*t)[f[0]]))))return true;
    for(int i=0;i<4;++i)for(int j=i+1;j<4;++j)for(int k=0;k<4;++k)for(int l=k+1;l<4;++l)
        if(axis(cross(sub(a[j],a[i]),sub(b[l],b[k]))))return true;
    return false;
}
static Tetrahedron convert(const QT& t,double scale,Point offset){
    Tetrahedron d;
    for(int i=0;i<4;++i)for(int j=0;j<3;++j)d[i][j]=offset[j]+scale*double(t[i][j]);
    return d;
}
int main(){
    std::mt19937_64 rng(3802027);std::uniform_int_distribution<int> coord(-128,128);
    auto sample=[&](){QT t;do{for(auto& p:t)for(auto& x:p)x=coord(rng);}while(!valid(t));return t;};
    std::size_t separated=0,overlap=0,unknown=0;
    for(int trial=0;trial<10000;++trial){
        const auto a=sample(),b=sample();const bool truth=exact_separated(a,b);
        for(int scale_id=0;scale_id<3;++scale_id){
            // All coordinate transforms below are exactly representable
            // binary64 dyadics. The integer oracle remains applicable.
            const double scale=scale_id==0?1.:scale_id==1?0x1p-20:0x1p+20;
            const Point offset=scale_id==1?Point{1024.,-1024.,2048.}:
                               scale_id==2?Point{0x1p+45,-0x1p+45,0x1p+45}:Point{};
            const auto x=convert(a,scale,offset),y=convert(b,scale,offset);
            const auto r=certify_initial_tet_separation(x,y);
            if(r.separated&&!truth)throw std::runtime_error("false clear versus exact integer SAT");
            if(truth){++separated;unknown+=!r.separated;}else ++overlap;
        }
    }
    if(!separated||!overlap)throw std::runtime_error("oracle campaign vacuous");
    std::cout<<"PASS38 exact SAT comparisons="<<separated+overlap
             <<" exact_separated="<<separated<<" exact_nonseparated="<<overlap
             <<" conservative_unknown="<<unknown<<" false_clear=0\n";
}

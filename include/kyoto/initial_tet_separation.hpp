#pragma once
// Pass 38A: initial convex tetrahedron separation, independent of FEM.
// A separating direction is merely a proposal. Only outward-rounded
// projections of ALL sixteen vertex differences authorize separation.
// No witness => unresolved (possibly intersecting), not a collision proof.
#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace kyoto::geometry {
using Point=std::array<double,3>;
using Tetrahedron=std::array<Point,4>;
struct InitialTetSeparation {
    bool separated{};
    std::size_t directions{},projections{};
};
namespace tet_sat_detail {
struct I {double lo{},hi{};};
inline double down(double x){return std::nextafter(x,-std::numeric_limits<double>::infinity());}
inline double up(double x){return std::nextafter(x,std::numeric_limits<double>::infinity());}
inline bool good(I x){return std::isfinite(x.lo)&&std::isfinite(x.hi)&&x.lo<=x.hi;}
inline I bad(){const auto q=std::numeric_limits<double>::infinity();return {-q,q};}
inline I add(I a,I b){if(!good(a)||!good(b))return bad();return {down(a.lo+b.lo),up(a.hi+b.hi)};}
inline I diff(double a,double b){const auto q=a-b;return {down(q),up(q)};}
inline I mul(I a,double b){
    if(!good(a)||!std::isfinite(b))return bad();
    const double x=a.lo*b,y=a.hi*b;
    if(!std::isfinite(x)||!std::isfinite(y))return bad();
    return {down(std::min(x,y)),up(std::max(x,y))};
}
inline Point sub(Point a,Point b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline Point cross(Point a,Point b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline bool supported(){
#if defined(__FAST_MATH__)
    return false;
#else
    volatile double d=std::numeric_limits<double>::denorm_min();
    return std::numeric_limits<double>::is_iec559&&std::fegetround()==FE_TONEAREST&&d>0&&d+d>d;
#endif
}
}
inline InitialTetSeparation certify_initial_tet_separation(const Tetrahedron& a,const Tetrahedron& b){
    using namespace tet_sat_detail;
    if(!supported())throw std::runtime_error("unsupported tetrahedral separation FP model");
    for(const auto* t:{&a,&b})for(const auto& p:*t)for(double x:p)
        if(!std::isfinite(x))throw std::invalid_argument("nonfinite initial tet");
    InitialTetSeparation r;
    auto prove=[&](Point n){
        const double scale=std::max({std::abs(n[0]),std::abs(n[1]),std::abs(n[2])});
        if(!(std::isfinite(scale)&&scale>0.))return false;
        // Scaling is only witness selection; no claim relies on exact axes.
        for(auto& x:n)x/=scale;
        ++r.directions;
        bool positive=true,negative=true;
        for(const auto& x:a)for(const auto& y:b){
            I p{0.,0.};
            for(int k=0;k<3;++k)p=add(p,mul(diff(x[k],y[k]),n[k]));
            ++r.projections;
            if(!good(p))return false;
            positive=positive&&p.lo>0.;negative=negative&&p.hi<0.;
            if(!positive&&!negative)return false;
        }
        return positive||negative;
    };
    for(Point n:{Point{1,0,0},Point{0,1,0},Point{0,0,1}})
        if(prove(n)){r.separated=true;return r;}
    constexpr int faces[4][3]={{0,1,2},{0,1,3},{0,2,3},{1,2,3}};
    for(const auto* t:{&a,&b})for(const auto& f:faces)
        if(prove(cross(sub((*t)[f[1]],(*t)[f[0]]),sub((*t)[f[2]],(*t)[f[0]])))){
            r.separated=true;return r;
        }
    for(int i=0;i<4;++i)for(int j=i+1;j<4;++j)
        for(int k=0;k<4;++k)for(int l=k+1;l<4;++l)
            if(prove(cross(sub(a[j],a[i]),sub(b[l],b[k])))){
                r.separated=true;return r;
            }
    return r;
}
} // namespace kyoto::geometry

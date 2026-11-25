#pragma once
// Pass 30A: outward interval Bernstein orientation guard for linear
// tetrahedral trajectories. A positive endpoint determinant is not
// sufficient: det(F(t)) is a cubic and can change sign twice.
// Let the columns of F(0),F(1) be A_i,B_i. Three times the cubic
// Bernstein coefficients are 3 det(A), the sum of the three determinants
// with one B column, the sum with two B columns, and 3 det(B).
// A convex combination of coefficients all strictly above 3*J_floor
// proves orientation throughout the interval. Dyadic de Casteljau
// subdivisions retain outward enclosures. Unknown never authorizes a step.
// Requires the same IEEE model as rounded_prism_certificate.hpp,
// including disabled fast-math, contraction and gradual underflow.
#include "kyoto/rounded_prism_certificate.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>

namespace kyoto::solid {
using TetPositions=std::array<V3,4>;
struct TetOrientationCertificate {
    bool certified{};
    std::size_t nodes{};
    unsigned max_depth{};
    // A lower bound on J over all certified time intervals, not a sample.
    // Meaningful only when certified==true.
    double lower_j{std::numeric_limits<double>::infinity()};
};
namespace tet_orientation_detail {
using namespace rounded;
using Column=std::array<Interval,3>;
using Matrix=std::array<Column,3>;
using Coefficients=std::array<Interval,4>;
inline Matrix deformation(const TetPositions& p,const M3& inv){
    Matrix out;
    for(int c=0;c<3;++c)for(int r=0;r<3;++r){
        Interval value=exact(0.);
        for(int k=0;k<3;++k)
            value=plus(value,product(minus(exact(component(p[k+1],r)),
                                          exact(component(p[0],r))),
                                     exact(inv(k,c))));
        out[c][r]=value;
    }
    return out;
}
inline Interval det(const Column& a,const Column& b,const Column& c){
    return plus(minus(
        product(a[0],minus(product(b[1],c[2]),product(b[2],c[1]))),
        product(a[1],minus(product(b[0],c[2]),product(b[2],c[0])))),
        product(a[2],minus(product(b[0],c[1]),product(b[1],c[0]))));
}
inline Coefficients coefficients(const Matrix& a,const Matrix& b){
    return {product(exact(3.),det(a[0],a[1],a[2])),
        plus(plus(det(b[0],a[1],a[2]),det(a[0],b[1],a[2])),det(a[0],a[1],b[2])),
        plus(plus(det(a[0],b[1],b[2]),det(b[0],a[1],b[2])),det(b[0],b[1],a[2])),
        product(exact(3.),det(b[0],b[1],b[2]))};
}
inline Interval half(Interval a,Interval b){
    return plus(product(a,exact(.5)),product(b,exact(.5)));
}
} // namespace tet_orientation_detail
inline TetOrientationCertificate certify_linear_tet_orientation(
    const TetPositions& start,const TetPositions& end,const M3& inverse_rest,
    double j_floor=1e-6,unsigned depth_limit=12){
    using namespace rounded;
    using namespace tet_orientation_detail;
    if(!prerequisites())throw std::runtime_error("unsupported orientation FP model");
    if(!std::isfinite(j_floor)||j_floor<0.||depth_limit>20)
        throw std::invalid_argument("invalid orientation guard settings");
    for(int i=0;i<4;++i)
        if(!finite(start[i])||!finite(end[i]))
            throw std::invalid_argument("nonfinite orientation trajectory");
    const auto threshold=product(exact(3.),exact(j_floor));
    TetOrientationCertificate receipt;
    if(!good(threshold))return receipt;
    const auto initial=coefficients(deformation(start,inverse_rest),
                                    deformation(end,inverse_rest));
    const auto check=[&](auto&& self,const Coefficients& p,unsigned depth)->bool{
        ++receipt.nodes;
        receipt.max_depth=std::max(receipt.max_depth,depth);
        double lower=std::numeric_limits<double>::infinity();
        for(const auto x:p){
            if(!good(x))return false;
            lower=std::min(lower,x.lo);
        }
        if(lower>threshold.hi){
            const double j=down(lower/3.);
            if(j>j_floor){
                receipt.lower_j=std::min(receipt.lower_j,j);
                return true;
            }
        }
        // Exact endpoints of this interval already cannot be proven
        // above the floor: no subdivision can certify the closed interval.
        if(p.front().lo<=threshold.hi||p.back().lo<=threshold.hi||
           depth==depth_limit)return false;
        const auto a=half(p[0],p[1]),b=half(p[1],p[2]),c=half(p[2],p[3]);
        const auto d=half(a,b),e=half(b,c),f=half(d,e);
        return self(self,Coefficients{p[0],a,d,f},depth+1)&&
               self(self,Coefficients{f,e,c,p[3]},depth+1);
    };
    receipt.certified=check(check,initial,0);
    if(!receipt.certified)receipt.lower_j=0.;
    return receipt;
}
} // namespace kyoto::solid

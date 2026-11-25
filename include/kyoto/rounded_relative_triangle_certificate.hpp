#pragma once
// Pass 31A: synchronized relative swept hull separation for two linear
// finite triangles, with outward binary64 projection arithmetic.
//
// For arbitrary barycentric weights alpha,beta, A(t)-B(t) belongs to
// conv{a_i(t)-b_j(t)}. On a common slab, each of the nine differences
// is affine in time and contained in its two endpoint convex hull.
// Hence all synchronized differences lie inside the hull of 18 points.
// Any direction n for which every support dot is > skin*||n|| proves
// separation throughout the slab, even when the individual world-space
// swept hulls overlap due to common translation.
// Approximate nearest features only PROPOSE directions. They never
// certify separation. Unknown is conservative, not an impact verdict.
// Same conditional IEEE model as rounded_prism_certificate.hpp.
#include "kyoto/rounded_prism_certificate.hpp"
#include "kyoto/triangle_mesh_contact.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace kyoto::solid {
struct RoundedPairCertificate {
    bool entirely_separated{};
    std::size_t nodes{},directions{},interval_projections{};
    unsigned max_depth{};
    double lower_clearance{std::numeric_limits<double>::infinity()};
};
namespace relative_hull_detail {
using namespace rounded;
using Differences=std::array<I3,9>;
inline I3 subtract(V3 a,V3 b){
    return {minus(exact(a.x),exact(b.x)),minus(exact(a.y),exact(b.y)),
            minus(exact(a.z),exact(b.z))};
}
inline Differences differences(const Triangle3& a,const Triangle3& b){
    Differences d;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)d[3*i+j]=subtract(a[i],b[j]);
    return d;
}
inline I3 lerp(I3 a,I3 b,double t){
    if(t==0.)return a;
    if(t==1.)return b;
    const auto f=[&](Interval x,Interval y){
        return plus(product(exact(1.-t),x),product(exact(t),y));
    };
    return {f(a.x,b.x),f(a.y,b.y),f(a.z,b.z)};
}
inline Interval projection(I3 p,V3 n){
    return plus(plus(product(p.x,exact(n.x)),product(p.y,exact(n.y))),
                product(p.z,exact(n.z)));
}
inline bool prove_direction(const Differences& a,const Differences& b,
                            V3 n,double skin,RoundedPairCertificate& out,
                            double& lower){
    if(!finite(n))return false;
    // Scaling directions is not needed for correctness. Reject tiny,
    // nonfinite or overflowing directions without claiming separation.
    const auto len=root(plus(plus(square(exact(n.x)),square(exact(n.y))),
                             square(exact(n.z))));
    if(!good(len)||!(len.lo>0.))return false;
    const auto target=product(exact(skin),{len.hi,len.hi});
    if(!good(target))return false;
    ++out.directions;
    double pmin=std::numeric_limits<double>::infinity();
    for(const auto* set:{&a,&b})for(const auto& p:*set){
        ++out.interval_projections;
        const auto dot=projection(p,n);
        if(!good(dot)||!(dot.lo>target.hi))return false;
        pmin=std::min(pmin,dot.lo);
    }
    lower=down(pmin/len.hi);
    return std::isfinite(lower)&&lower>skin;
}
} // namespace relative_hull_detail
inline RoundedPairCertificate certify_rounded_relative_triangle_pair(
    const LinearTriangleMotion& a,const LinearTriangleMotion& b,
    double skin=0.,unsigned depth_limit=12){
    using namespace relative_hull_detail;
    if(!rounded::prerequisites())throw std::runtime_error("unsupported relative hull FP model");
    if(!std::isfinite(skin)||skin<0.||depth_limit>20)
        throw std::invalid_argument("invalid relative hull parameters");
    for(int i=0;i<3;++i)
        if(!finite(a.start[i])||!finite(a.end[i])||
           !finite(b.start[i])||!finite(b.end[i]))
            throw std::invalid_argument("nonfinite relative hull motion");
    const auto start=differences(a.start,b.start),end=differences(a.end,b.end);
    RoundedPairCertificate out;
    const auto recurse=[&](auto&& self,double t0,double t1,unsigned depth)->bool{
        ++out.nodes;out.max_depth=std::max(out.max_depth,depth);
        Differences lo,hi;
        for(int i=0;i<9;++i){
            lo[i]=lerp(start[i],end[i],t0);
            hi[i]=lerp(start[i],end[i],t1);
        }
        double lower=0.;
        auto test=[&](V3 n){return prove_direction(lo,hi,n,skin,out,lower);};
        bool proven=false;
        for(V3 n:{V3{1,0,0},V3{-1,0,0},V3{0,1,0},V3{0,-1,0},V3{0,0,1},V3{0,0,-1}})
            if(test(n)){proven=true;break;}
        if(!proven){
            const double mid=(t0+t1)*.5;
            const auto am=lerp_vertices(a.start,a.end,mid);
            const auto bm=lerp_vertices(b.start,b.end,mid);
            // Approximate closest geometry is only a witness generator.
            // A failed proposal or nonfinite intermediate never proves safety.
            const auto near=triangle_triangle_closest(am,bm);
            proven=test(near.on_deformable-near.on_rigid);
            if(!proven){
                const auto na=cross(am[1]-am[0],am[2]-am[0]);
                const auto nb=cross(bm[1]-bm[0],bm[2]-bm[0]);
                proven=test(na)||test(-na)||test(nb)||test(-nb);
            }
            if(!proven)for(int i=0;i<3&&!proven;++i)
                for(int j=0;j<3&&!proven;++j){
                    const auto n=cross(am[(i+1)%3]-am[i],bm[(j+1)%3]-bm[j]);
                    proven=test(n)||test(-n);
                }
            // At a sampled possible contact the closed interval cannot
            // be certified by this path. Returning unknown is safe even
            // when the approximate distance underestimated the true one.
            if(!proven&&near.distance<=skin)return false;
        }
        if(proven){
            out.lower_clearance=std::min(out.lower_clearance,lower);
            return true;
        }
        if(depth==depth_limit)return false;
        const double mid=(t0+t1)*.5;
        return self(self,t0,mid,depth+1)&&self(self,mid,t1,depth+1);
    };
    out.entirely_separated=recurse(recurse,0.,1.,0);
    if(!out.entirely_separated)out.lower_clearance=0.;
    return out;
}
} // namespace kyoto::solid

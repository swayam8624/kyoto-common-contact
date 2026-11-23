#pragma once
// Kyoto Pass 10: IEEE-754 outward-enclosed swept triangle/sphere separation.
//
// Unlike the Pass-9 heuristic (whose candidate inequality was evaluated
// directly in double), this verifier does not TRUST a rounded dot product.
// Each elementary subtraction, multiplication, addition and square-root
// is conservatively enlarged by one representable binary64 number on
// either side, assuming correctly rounded IEEE operations in FE_TONEAREST.
//
// A direction is just a *candidate*. It is accepted only if the
// outward interval LOWER bound for every one of the six swept-endpoint
// support projections is STRICTLY above the UPPER bound r*||n||.
//
// To subdivide time without silently introducing gaps due to rounded
// midpoint positions, dyadic t endpoints are represented exactly and
// all six endpoint positions are enclosed from the ORIGINAL two
// world-position vectors, using interval arithmetic:
//
//      P_i(t) = (1-t)*P_i(0) + t*P_i(1)
//
// All interior interpolations therefore remain contained. This tests
// LINEAR source-vertex motion only; it is not general mesh-mesh CCD.
//
// ASSUMPTIONS: IEC559 double, hardware/subnormal gradual underflow,
// correctly rounded +-*/sqrt/nextafter, FE_TONEAREST and no fast-math,
// FTZ/DAZ or reassociation. We check supported assumptions where
// possible, but hardware FTZ and C++ math implementation compliance
// are not formally verified by this header. This is a stronger
// mathematically explicit FP envelope, NOT an MPFR/exact-predicate
// formal proof across all compilers/runtimes.
//
// Failed candidate -> UNKNOWN (never "clear"). Nonfinite intermediate
// ranges -> UNKNOWN (never certified). No solver position or velocity
// is modified by this independent auditor.

#include "kyoto/independent_prism_audit.hpp"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace kyoto::solid::rounded {

struct Interval { double lo{},hi{}; };
struct I3 { Interval x{},y{},z{}; };

inline double up(double a){
    return std::nextafter(a,std::numeric_limits<double>::infinity());
}
inline double down(double a){
    return std::nextafter(a,-std::numeric_limits<double>::infinity());
}
inline bool good(Interval a){
    return std::isfinite(a.lo)&&std::isfinite(a.hi)&&a.lo<=a.hi;
}
inline Interval exact(double a){
    if(!std::isfinite(a))throw std::invalid_argument("nonfinite interval point");
    return {a,a};
}
inline Interval plus(Interval a,Interval b){
    if(!good(a)||!good(b))return {-INFINITY,INFINITY};
    return {down(a.lo+b.lo),up(a.hi+b.hi)};
}
inline Interval minus(Interval a,Interval b){
    if(!good(a)||!good(b))return {-INFINITY,INFINITY};
    return {down(a.lo-b.hi),up(a.hi-b.lo)};
}
inline Interval product(Interval a,Interval b){
    if(!good(a)||!good(b))return {-INFINITY,INFINITY};
    const double x[4]={a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
    for(const double y:x)
        if(!std::isfinite(y))return {-INFINITY,INFINITY};
    return {down(*std::min_element(x,x+4)),
            up(*std::max_element(x,x+4))};
}
inline Interval square(Interval a){
    if(!good(a))return {-INFINITY,INFINITY};
    if(a.lo>=0)return {down(a.lo*a.lo),up(a.hi*a.hi)};
    if(a.hi<=0)return {down(a.hi*a.hi),up(a.lo*a.lo)};
    const double m=std::max(a.lo*a.lo,a.hi*a.hi);
    return {0.,up(m)};
}
inline Interval root(Interval a){
    if(!good(a)||a.hi<0)return {-INFINITY,INFINITY};
    const double hi=std::sqrt(a.hi);
    const double lo=std::sqrt(std::max(0.,a.lo));
    return {std::max(0.,down(lo)),up(hi)};
}
inline Interval coordinate(I3 p,int i){
    return i==0?p.x:i==1?p.y:p.z;
}
inline double component(V3 p,int i){
    return i==0?p.x:i==1?p.y:p.z;
}
inline I3 interpolated(V3 p0,V3 p1,double t){
    // Binary dyadic t is exactly representable for depth<=22.
    // Use p0 + t*(p1-p0); mathematical interval encloses that
    // exact affine interpolation on the REAL input vertices.
    if(t==0)return {exact(p0.x),exact(p0.y),exact(p0.z)};
    if(t==1)return {exact(p1.x),exact(p1.y),exact(p1.z)};
    const auto f=[&](double a,double b)->Interval{
        return plus(exact(a),product(exact(t),minus(exact(b),exact(a))));
    };
    return {f(p0.x,p1.x),f(p0.y,p1.y),f(p0.z,p1.z)};
}
inline bool prerequisites(){
#if defined(__FAST_MATH__)
    return false;
#else
    if(!std::numeric_limits<double>::is_iec559 ||
       FLT_RADIX!=2 || std::fegetround()!=FE_TONEAREST)
        return false;
    // This also fails obvious FTZ/DAZ configurations on supported
    // targets without pretending to detect every nonstandard mode.
    const double denorm=std::numeric_limits<double>::denorm_min();
    return denorm>0 && denorm*.5==0 &&
           denorm+denorm>denorm;
#endif
}

struct RoundedWitness {
    bool proven{};
    double strictly_positive_lower_margin_m{};
    std::size_t endpoint_interval_dot_products{};
};

// All coordinates in the 6 prisms are INTERVALS (not merely
// nominally rounded binary64 endpoint locations).
inline RoundedWitness verify_plane(
    const std::array<I3,6>& corners,V3 n,
    const RigidSphere& sphere) {
    RoundedWitness result;
    if(!finite(n))return result;
    const Interval length_squared=plus(
        plus(square(exact(n.x)),square(exact(n.y))),
        square(exact(n.z)));
    const Interval len=root(length_squared);
    if(!good(len)||!(len.hi>0))return result;
    const Interval threshold=product(exact(sphere.radius),
                                     {len.hi,len.hi});
    if(!good(threshold))return result;
    double smallest=std::numeric_limits<double>::infinity();
    for(const auto& corner:corners){
        Interval projection=exact(0);
        for(int i=0;i<3;i++){
            const Interval displacement=minus(
                coordinate(corner,i),
                exact(component(sphere.center,i)));
            projection=plus(projection,product(
                exact(component(n,i)),displacement));
        }
        result.endpoint_interval_dot_products++;
        if(!good(projection)||!(projection.lo>threshold.hi))
            return result;
        const double lower=down(projection.lo-threshold.hi);
        // Divide by upper ||n|| only for a reported lower margin.
        // The strict *boolean* proof requires only dot LOWER > rhs UPPER.
        if(!std::isfinite(lower)||lower<=0)return result;
        // The reported metric must ALSO be a genuine lower bound:
        // a round-to-nearest division might otherwise exaggerate
        // the clearance by a final ULP, even though the proof bool
        // itself was already one-sided and conservative.
        const double normalized_lower=down(lower/len.hi);
        if(!(std::isfinite(normalized_lower)&&normalized_lower>0))
            return result;
        smallest=std::min(smallest,normalized_lower);
    }
    result.proven=true;
    result.strictly_positive_lower_margin_m=smallest;
    return result;
}
struct CertifiedPrism {
    bool fully_certified{};
    std::size_t tested_time_nodes{};
    std::size_t certified_time_nodes{};
    std::size_t unresolved_time_nodes{};
    std::size_t proposed_planes{};
    std::size_t interval_dot_products{};
    unsigned max_depth_reached{};
    double minimum_verified_margin_m{
        std::numeric_limits<double>::infinity()};
};

inline CertifiedPrism certify_rounded_prism(
    const LinearTriangleMotion& motion,const RigidSphere& sphere,
    unsigned max_depth=12){
    validate(sphere);
    if(!prerequisites())
        throw std::runtime_error(
            "outward binary64 verifier requires supported IEEE round-to-nearest");
    if(max_depth>22)throw std::invalid_argument("too deep dyadic interval");
    for(int i=0;i<3;i++)
        if(!finite(motion.start[i])||!finite(motion.end[i]))
            throw std::invalid_argument("nonfinite rounded prism input");
    CertifiedPrism out;
    out.fully_certified=true;
    const auto scan=[&](auto&& self,double t0,double t1,
                        unsigned depth)->void{
        out.tested_time_nodes++;
        out.max_depth_reached=std::max(depth,out.max_depth_reached);
        const std::array<I3,6> points={
            interpolated(motion.start[0],motion.end[0],t0),
            interpolated(motion.start[1],motion.end[1],t0),
            interpolated(motion.start[2],motion.end[2],t0),
            interpolated(motion.start[0],motion.end[0],t1),
            interpolated(motion.start[1],motion.end[1],t1),
            interpolated(motion.start[2],motion.end[2],t1)};
        const auto positions=[&](double t)->std::array<V3,3>{
            return lerp_vertices(motion.start,motion.end,t);
        };
        const auto a=positions(t0),b=positions(t1);
        std::array<V3,6> v;
        for(int i=0;i<3;i++){
            v[i]=a[i]-sphere.center;
            v[i+3]=b[i]-sphere.center;
        }
        bool separated=false;
        double witness_margin=0.;
        const auto test=[&](V3 direction){
            if(separated||!finite(direction))return;
            const double len=norm(direction);
            if(!(std::isfinite(len)&&len>1e-14))return;
            out.proposed_planes++;
            const auto cert=verify_plane(points,direction,sphere);
            out.interval_dot_products+=cert.endpoint_interval_dot_products;
            if(cert.proven){
                separated=true;
                witness_margin=cert.strictly_positive_lower_margin_m;
            }
        };
        for(const auto& n:std::array<V3,6>{
            V3{1,0,0},V3{-1,0,0},V3{0,1,0},
            V3{0,-1,0},V3{0,0,1},V3{0,0,-1}})test(n);
        for(const auto& q:v)test(q);
        for(int side=0;side<2;side++){
            const int i=side*3;
            const V3 n=cross(v[i+1]-v[i],v[i+2]-v[i]);
            test(n);test(-n);
        }
        if(!separated){
            int nearest=0;
            for(int j=1;j<6;j++)
                if(dot(v[j],v[j])<dot(v[nearest],v[nearest]))
                    nearest=j;
            V3 q=v[nearest];
            for(int k=0;k<64;k++){
                test(q);
                if(separated)break;
                int extreme=0;
                for(int j=1;j<6;j++)
                    if(dot(q,v[j])<dot(q,v[extreme]))
                        extreme=j;
                const V3 delta=q-v[extreme];
                const double denom=dot(delta,delta);
                if(!(std::isfinite(denom)&&denom>1e-28))break;
                const double rate=std::clamp(dot(q,delta)/denom,0.,1.);
                if(rate<1e-16)break;
                q=q-delta*rate;
            }
        }
        if(separated){
            out.certified_time_nodes++;
            out.minimum_verified_margin_m=std::min(
                out.minimum_verified_margin_m,witness_margin);
            return;
        }
        if(depth==max_depth){
            out.fully_certified=false;
            out.unresolved_time_nodes++;
            return;
        }
        const double tm=(t0+t1)*.5;
        self(self,t0,tm,depth+1);
        self(self,tm,t1,depth+1);
    };
    scan(scan,0.,1.,0);
    return out;
}
} // namespace kyoto::solid::rounded

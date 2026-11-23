#pragma once
// Pass 9: independent point/FINITE-triangle distance implementation.
// Different numerical structure from sphere_surface_audit.hpp, which
// explicitly tests all edges plus planar projection. Here we classify
// the seven Voronoi regions (3 vertices, 3 edges, 1 interior), using
// the Ericson barycentric determinants. Degenerate triangles fall
// back to three independently clamped segments.
//
// This is NOT exact predicate/interval geometry: conventional binary64
// dot products and comparisons are used. Difference to the production
// nearest-point routine is measured, not assumed to be zero.

#include "kyoto/solid_dynamics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kyoto::solid {
struct IndependentTriangleNearest {
    V3 point{};
    double distance{};
    std::array<double,3> barycentric{};
};
inline IndependentTriangleNearest independent_triangle_distance(
    V3 p,V3 a,V3 b,V3 c) {
    if(!finite(p)||!finite(a)||!finite(b)||!finite(c))
        throw std::invalid_argument("independent point-triangle input nonfinite");
    const V3 ab=b-a,ac=c-a,ap=p-a;
    const double aa=dot(ab,ab),bb=dot(ac,ac),abac=dot(ab,ac);
    // Explicit separate degeneracy policy: finite triangle collapses
    // to the union of its three closed segments, not a plane.
    if(aa*bb-abac*abac <=
       1e-25*std::max({1.,aa*bb,abac*abac})) {
        IndependentTriangleNearest best;
        best.distance=std::numeric_limits<double>::infinity();
        const V3 vertex[3]={a,b,c};
        for(int i=0;i<3;i++){
            const int j=(i+1)%3;
            const V3 delta=vertex[j]-vertex[i];
            const double d2=dot(delta,delta);
            const double t=d2>0?
               std::clamp(dot(p-vertex[i],delta)/d2,0.,1.):0.;
            const V3 point=vertex[i]+delta*t;
            const double distance=norm(p-point);
            if(distance<best.distance){
                best.distance=distance;
                best.point=point;
                best.barycentric={};
                best.barycentric[i]=1-t;
                best.barycentric[j]=t;
            }
        }
        return best;
    }
    const double d1=dot(ab,ap),d2=dot(ac,ap);
    V3 q;
    std::array<double,3> w{};
    if(d1<=0&&d2<=0){q=a;w={1,0,0};}
    else {
        const V3 bp=p-b;
        const double d3=dot(ab,bp),d4=dot(ac,bp);
        if(d3>=0&&d4<=d3){q=b;w={0,1,0};}
        else {
            const double vc=d1*d4-d3*d2;
            if(vc<=0&&d1>=0&&d3<=0){
                const double v=d1/(d1-d3);
                q=a+ab*v;w={1-v,v,0};
            }else{
                const V3 cp=p-c;
                const double d5=dot(ab,cp),d6=dot(ac,cp);
                if(d6>=0&&d5<=d6){q=c;w={0,0,1};}
                else {
                    const double vb=d5*d2-d1*d6;
                    if(vb<=0&&d2>=0&&d6<=0){
                        const double v=d2/(d2-d6);
                        q=a+ac*v;w={1-v,0,v};
                    }else{
                        const double va=d3*d6-d5*d4;
                        if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){
                            const double v=(d4-d3)/
                                ((d4-d3)+(d5-d6));
                            q=b+(c-b)*v;w={0,1-v,v};
                        }else{
                            const double sum=va+vb+vc;
                            if(!(std::isfinite(sum)&&sum>0))
                                throw std::runtime_error(
                                    "independent Voronoi triangle degeneracy");
                            const double v=vb/sum,u=vc/sum;
                            q=a+ab*v+ac*u;
                            w={1-v-u,v,u};
                        }
                    }
                }
            }
        }
    }
    const double distance=norm(q-p);
    if(!(std::isfinite(distance)&&finite(q)))
        throw std::runtime_error("independent triangle distance invalid");
    return {q,distance,w};
}
} // namespace kyoto::solid

#pragma once
// Pass 6: independently inspect curved-obstacle clearance of the *entire
// triangular boundary surface*, not just its 180 moving vertices.
//
// Static geometric audit only. This does NOT implement continuous
// triangle-sphere CCD between accepted states or physical contact response.
// A triangle can pass through a sphere although all 3 vertices are outside.
// Report the failure mode rather than calling vertex-safe == surface-safe.
#include "kyoto/curved_contact.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace kyoto::solid {

struct TriangleDistance {
    V3 nearest{};
    std::array<double,3> barycentric{};
    double distance{};
};
// Squared-distance minimization on the closed triangle: evaluate vertices,
// all 3 edge segments, and the plane projection when inside the triangle.
// Degenerate faces are handled through their edges rather than NaN/inf.
inline TriangleDistance closest_triangle_point(
    V3 p,V3 a,V3 b,V3 c) {
    if(!finite(p)||!finite(a)||!finite(b)||!finite(c))
        throw std::invalid_argument("nonfinite triangle distance input");
    TriangleDistance best;
    double best_d2=std::numeric_limits<double>::infinity();
    const auto candidate=[&](V3 q,std::array<double,3> w) {
        const double d2=dot(p-q,p-q);
        if(!std::isfinite(d2))
            throw std::overflow_error("triangle distance overflow");
        if(d2<best_d2) {
            best_d2=d2;
            best.nearest=q;
            best.barycentric=w;
        }
    };
    candidate(a,{1,0,0});candidate(b,{0,1,0});candidate(c,{0,0,1});
    const V3 v[3]{a,b,c};
    for(int edge=0;edge<3;edge++){
        const int i=edge,j=(edge+1)%3;
        const auto d=v[j]-v[i];
        const double length2=dot(d,d);
        if(!(length2>0))continue;
        const double t=std::clamp(dot(p-v[i],d)/length2,0.,1.);
        std::array<double,3> w{};
        w[i]=1-t;w[j]=t;
        candidate(v[i]+d*t,w);
    }
    const auto ab=b-a,ac=c-a;
    const auto n=cross(ab,ac);
    const double area2=dot(n,n);
    if(area2>1e-24) {
        // Orthogonal plane projection and two-by-two barycentric solve.
        const auto q=p-n*(dot(p-a,n)/area2);
        const auto aq=q-a;
        const double ab2=dot(ab,ab),ac2=dot(ac,ac),mix=dot(ab,ac);
        const double determinant2=ab2*ac2-mix*mix;
        if(determinant2>1e-25) {
            const double t=(dot(aq,ab)*ac2-dot(aq,ac)*mix)/determinant2;
            const double u=(dot(aq,ac)*ab2-dot(aq,ab)*mix)/determinant2;
            // Include geometric boundary with floating tolerance.
            if(t>=-1e-12&&u>=-1e-12&&t+u<=1+1e-12) {
                const double tt=std::clamp(t,0.,1.);
                const double uu=std::clamp(u,0.,1.-tt);
                candidate(a+ab*tt+ac*uu,{1.-tt-uu,tt,uu});
            }
        }
    }
    best.distance=std::sqrt(best_d2);
    return best;
}

struct SurfaceSphereAudit {
    double minimum_surface_clearance_m{std::numeric_limits<double>::infinity()};
    double minimum_vertex_clearance_m{std::numeric_limits<double>::infinity()};
    std::size_t penetrating_faces{},tested_faces{};
    std::size_t outside_vertex_count{},inside_vertex_count{};
    std::size_t face_nearest_vertex{};
    V3 surface_nearest_point{};
};
// For an opaque solid and a rigid sphere, a negative triangle clearance
// diagnoses an overlapping *boundary surface*. It does not independently
// determine inside/outside winding of a closed volume.
inline SurfaceSphereAudit audit_surface_sphere(
    const Body& body,const RigidSphere& obstacle,double eps=1e-9) {
    validate(obstacle);
    if(!std::isfinite(eps)||eps<0)
        throw std::invalid_argument("invalid audit tolerance");
    SurfaceSphereAudit result;
    for(const auto& node:body.nodes){
        const double gap=norm(node.x-obstacle.center)-obstacle.radius;
        result.minimum_vertex_clearance_m=
            std::min(result.minimum_vertex_clearance_m,gap);
        if(gap< -eps)++result.inside_vertex_count;
        else ++result.outside_vertex_count;
    }
    for(std::size_t i=0;i<body.surface_faces.size();i++){
        const auto& f=body.surface_faces[i];
        for(auto id:f)if(id>=body.nodes.size())
            throw std::invalid_argument("invalid surface face connectivity");
        const auto closest=closest_triangle_point(
            obstacle.center,body.nodes[f[0]].x,
            body.nodes[f[1]].x,body.nodes[f[2]].x);
        const double gap=closest.distance-obstacle.radius;
        if(gap<result.minimum_surface_clearance_m){
            result.minimum_surface_clearance_m=gap;
            result.face_nearest_vertex=i;
            result.surface_nearest_point=closest.nearest;
        }
        if(gap<-eps)++result.penetrating_faces;
        result.tested_faces++;
    }
    return result;
}
} // namespace kyoto::solid

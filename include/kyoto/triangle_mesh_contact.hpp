#pragma once
// Kyoto Pass 11: first actual rigid triangular-mesh narrow phase.
//
// This file is deliberately NOT the old sphere model disguised as a mesh:
// both objects are connected finite THREE-DIMENSIONAL TRIANGLE SURFACES.
// Pair separation uses all vertex/face and edge/edge closest features,
// PLUS segment/triangle piercing tests. The latter are essential: a
// moving edge can go through a triangle interior even when every
// endpoint-to-triangle and edge-to-edge distance is strictly positive.
//
// The continuous test bounds a LINEARLY MOVING deformable triangle
// against a static finite rigid triangle. The distance between compact
// sets is 1-Lipschitz in the maximum vertex displacement, hence
//
//   distance(T(t),S) >= d(T(mid),S) - .5 max_i ||end_i-start_i||.
//
// A positive lower bound above a requested clearance proves the entire
// interval separated IN EXACT REAL ARITHMETIC. Binary64 code uses a
// deliberate scale-aware conservative guard, not formally directed-rounded
// interval predicates. Unresolved is never relabeled "no contact".
//
// The scope is geometry and contact candidates. A mesh-mesh response
// requires physically coupled contact manifolds, sides/inside semantics,
// mass, friction, time stepping, and a separate validated solver.

#include "kyoto/independent_triangle_distance.hpp"
#include "kyoto/triangle_sphere_sweep.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::solid {
using Triangle3=std::array<V3,3>;

struct StaticTriangleMesh {
    std::vector<V3> vertices;
    std::vector<std::array<std::size_t,3>> faces;
};
inline void validate(const StaticTriangleMesh& mesh){
    if(mesh.vertices.empty()||mesh.faces.empty()||
       mesh.vertices.size()>1000000||mesh.faces.size()>2000000)
        throw std::invalid_argument("empty/oversized rigid triangle mesh");
    for(const auto& p:mesh.vertices)
        if(!finite(p))throw std::invalid_argument("nonfinite rigid vertex");
    for(const auto& f:mesh.faces){
        if(f[0]>=mesh.vertices.size()||f[1]>=mesh.vertices.size()||
           f[2]>=mesh.vertices.size()||
           f[0]==f[1]||f[0]==f[2]||f[1]==f[2])
            throw std::invalid_argument("invalid rigid triangle connectivity");
        const V3 a=mesh.vertices[f[0]];
        const V3 b=mesh.vertices[f[1]];
        const V3 c=mesh.vertices[f[2]];
        const double u=norm(b-a),v=norm(c-a);
        if(!(u>1e-12&&v>1e-12&&
             norm(cross(b-a,c-a))>1e-12*u*v))
            throw std::invalid_argument("degenerate rigid triangle");
    }
}
inline Triangle3 face_positions(const StaticTriangleMesh& mesh,
                                std::size_t index){
    const auto f=mesh.faces.at(index);
    return {mesh.vertices[f[0]],mesh.vertices[f[1]],
            mesh.vertices[f[2]]};
}
struct SegmentClosest{
    V3 a{},b{};
    double s{},t{};
    double squared_distance{};
};
inline SegmentClosest closest_segments(V3 p,V3 q,V3 r,V3 s){
    const V3 u=q-p,v=s-r,w=p-r;
    const double a=dot(u,u),e=dot(v,v),
                 b=dot(u,v),c=dot(u,w),f=dot(v,w);
    double alpha=0,beta=0;
    const double scale=std::max({1.,a,e});
    if(a<=1e-26*scale&&e<=1e-26*scale){
        alpha=0;beta=0;
    }else if(a<=1e-26*scale){
        alpha=0;beta=std::clamp(f/e,0.,1.);
    }else if(e<=1e-26*scale){
        beta=0;alpha=std::clamp(-c/a,0.,1.);
    }else{
        const double denom=a*e-b*b;
        if(denom>1e-26*scale*scale)
            alpha=std::clamp((b*f-c*e)/denom,0.,1.);
        beta=(b*alpha+f)/e;
        if(beta<0){
            beta=0;alpha=std::clamp(-c/a,0.,1.);
        }else if(beta>1){
            beta=1;alpha=std::clamp((b-c)/a,0.,1.);
        }
    }
    const V3 a0=p+u*alpha,b0=r+v*beta;
    return {a0,b0,alpha,beta,dot(a0-b0,a0-b0)};
}

struct RigidFaceContact {
    V3 on_deformable{},on_rigid{};
    std::array<double,3> deformable_weights{};
    std::array<double,3> rigid_weights{};
    double distance{std::numeric_limits<double>::infinity()};
    enum class Feature{vertex_face,face_vertex,edge_edge,face_piercing}
        feature{Feature::vertex_face};
};

inline bool segment_triangle_piercing(V3 p,V3 q,
    const Triangle3& tri,double& t,
    std::array<double,3>& w){
    // Moller-Trumbore, TWO-SIDED, including internal intersection.
    // Returns false on coplanar configurations; the vertex and
    // edge/edge cases handle those in the finite-triangle audit.
    const V3 d=q-p,e0=tri[1]-tri[0],e1=tri[2]-tri[0];
    const V3 h=cross(d,e1);
    const double determinant=dot(e0,h);
    const double scale=std::max({1.,norm(d),norm(e0),norm(e1)});
    if(!(std::abs(determinant)>1e-13*scale*scale*scale))
        return false;
    const double inv=1/determinant;
    const V3 s=p-tri[0];
    const double u=dot(s,h)*inv;
    if(u<-1e-11||u>1+1e-11)return false;
    const V3 k=cross(s,e0);
    const double v=dot(d,k)*inv;
    if(v<-1e-11||u+v>1+1e-11)return false;
    const double along=dot(e1,k)*inv;
    if(along<-1e-11||along>1+1e-11)return false;
    t=std::clamp(along,0.,1.);
    w={1-u-v,u,v};
    return true;
}
inline RigidFaceContact triangle_triangle_closest(
    const Triangle3& a,const Triangle3& b){
    for(const auto& q:a)if(!finite(q))
        throw std::invalid_argument("nonfinite deformable triangle");
    for(const auto& q:b)if(!finite(q))
        throw std::invalid_argument("nonfinite rigid triangle");
    RigidFaceContact best;
    const auto emit=[&](V3 p,V3 q,
             std::array<double,3> w0,std::array<double,3> w1,
             RigidFaceContact::Feature feature){
        const double d=norm(p-q);
        if(d<best.distance)best={p,q,w0,w1,d,feature};
    };
    for(int i=0;i<3;i++){
        const auto near=independent_triangle_distance(
            a[i],b[0],b[1],b[2]);
        std::array<double,3> own{};own[i]=1;
        emit(a[i],near.point,own,near.barycentric,
             RigidFaceContact::Feature::vertex_face);
    }
    for(int j=0;j<3;j++){
        const auto near=independent_triangle_distance(
            b[j],a[0],a[1],a[2]);
        std::array<double,3> own{};own[j]=1;
        emit(near.point,b[j],near.barycentric,own,
             RigidFaceContact::Feature::face_vertex);
    }
    for(int i=0;i<3;i++)for(int j=0;j<3;j++){
        const int ii=(i+1)%3,jj=(j+1)%3;
        const auto near=closest_segments(a[i],a[ii],b[j],b[jj]);
        std::array<double,3> wa{},wb{};
        wa[i]=1-near.s;wa[ii]=near.s;
        wb[j]=1-near.t;wb[jj]=near.t;
        emit(near.a,near.b,wa,wb,
             RigidFaceContact::Feature::edge_edge);
    }
    // Essential intersection cases. Without these, a thin
    // rod-like deformable triangle can pierce another face
    // entirely through an edge *interior* without sharing any
    // closest endpoint, or any pair of mutually touching edges.
    for(int i=0;i<3;i++){
        const int j=(i+1)%3;
        double t=0;std::array<double,3> w{};
        if(segment_triangle_piercing(a[i],a[j],b,t,w)){
            std::array<double,3> own{};own[i]=1-t;own[j]=t;
            const V3 p=a[i]+(a[j]-a[i])*t;
            emit(p,p,own,w,RigidFaceContact::Feature::face_piercing);
        }
        if(segment_triangle_piercing(b[i],b[j],a,t,w)){
            std::array<double,3> own{};own[i]=1-t;own[j]=t;
            const V3 p=b[i]+(b[j]-b[i])*t;
            emit(p,p,w,own,RigidFaceContact::Feature::face_piercing);
        }
    }
    if(!std::isfinite(best.distance))
        throw std::runtime_error("invalid mesh-mesh finite triangle distance");
    return best;
}

struct TriangleBox{
    V3 lo{},hi{};
};
inline TriangleBox bounding(const Triangle3& p){
    TriangleBox r{p[0],p[0]};
    for(const auto& x:p){
        r.lo={std::min(r.lo.x,x.x),std::min(r.lo.y,x.y),
              std::min(r.lo.z,x.z)};
        r.hi={std::max(r.hi.x,x.x),std::max(r.hi.y,x.y),
              std::max(r.hi.z,x.z)};
    }
    return r;
}
inline TriangleBox swept_bounding(const LinearTriangleMotion& path){
    auto r=bounding(path.start);
    for(const auto& x:path.end){
        r.lo={std::min(r.lo.x,x.x),std::min(r.lo.y,x.y),
              std::min(r.lo.z,x.z)};
        r.hi={std::max(r.hi.x,x.x),std::max(r.hi.y,x.y),
              std::max(r.hi.z,x.z)};
    }
    return r;
}
inline bool maybe_near(TriangleBox a,TriangleBox b,double gap){
    if(!(std::isfinite(gap)&&gap>=0))
        throw std::invalid_argument("invalid triangle box expansion");
    return a.lo.x<=b.hi.x+gap&&b.lo.x<=a.hi.x+gap&&
           a.lo.y<=b.hi.y+gap&&b.lo.y<=a.hi.y+gap&&
           a.lo.z<=b.hi.z+gap&&b.lo.z<=a.hi.z+gap;
}
struct MeshSweepCertificate{
    bool entirely_separated{true};
    double safe_prefix{1.};
    double first_unresolved_upper{1.};
    std::size_t nodes{},pruned{},sampled_piercings{};
};
inline MeshSweepCertificate certify_moving_triangle_rigid_triangle(
    const LinearTriangleMotion& motion,const Triangle3& rigid,
    double thickness=0.,unsigned max_depth=12){
    if(!(std::isfinite(thickness)&&thickness>=0)||max_depth>20)
        throw std::invalid_argument("invalid mesh CCD parameters");
    MeshSweepCertificate cert;
    const auto recurse=[&](auto&& self,
          double t0,double t1,unsigned depth)->bool{
        ++cert.nodes;
        const auto a=lerp_vertices(motion.start,motion.end,t0);
        const auto b=lerp_vertices(motion.start,motion.end,t1);
        // Culling the WHOLE swept AABB is a sufficient separation
        // certificate against the static face even at depth zero.
        if(!maybe_near(swept_bounding({a,b}),bounding(rigid),
                       thickness+1e-12)){
            ++cert.pruned;return true;
        }
        const auto midpoint=lerp_vertices(motion.start,motion.end,
                                           .5*(t0+t1));
        const auto near=triangle_triangle_closest(midpoint,rigid);
        double max_motion=0;
        for(int j=0;j<3;j++)
            max_motion=std::max(max_motion,norm(b[j]-a[j]));
        const double guard=4e-11*std::max({1.,thickness,
                  near.distance,max_motion});
        if(near.distance-.5*max_motion-guard>thickness){
            ++cert.pruned;return true;
        }
        if(near.distance<thickness-guard)cert.sampled_piercings++;
        if(depth==max_depth){
            cert.entirely_separated=false;
            cert.safe_prefix=t0;
            cert.first_unresolved_upper=t1;
            return false;
        }
        const double mid=(t0+t1)*.5;
        return self(self,t0,mid,depth+1) &&
               self(self,mid,t1,depth+1);
    };
    recurse(recurse,0.,1.,0);
    return cert;
}

struct MeshSurfaceAudit {
    double minimum_distance_m{std::numeric_limits<double>::infinity()};
    std::size_t overlapping_face_pairs{};
    std::size_t face_pair_tests{};
    std::size_t broadphase_culled{};
};
inline MeshSurfaceAudit audit_deformable_rigid_mesh(
    const Body& deformable,const StaticTriangleMesh& rigid,
    double thickness=0.){
    validate(rigid);
    if(!(std::isfinite(thickness)&&thickness>=0))
        throw std::invalid_argument("negative mesh thickness");
    MeshSurfaceAudit out;
    for(const auto& f:deformable.surface_faces){
        const Triangle3 triangle={
            deformable.nodes.at(f[0]).x,deformable.nodes.at(f[1]).x,
            deformable.nodes.at(f[2]).x};
        for(std::size_t i=0;i<rigid.faces.size();i++){
            const auto other=face_positions(rigid,i);
            // AABB disjointness gives a LOWER bound on distance; it
            // cannot give the exact global minimum without a BVH.
            const auto a=bounding(triangle),b=bounding(other);
            double dx=std::max({a.lo.x-b.hi.x,b.lo.x-a.hi.x,0.});
            double dy=std::max({a.lo.y-b.hi.y,b.lo.y-a.hi.y,0.});
            double dz=std::max({a.lo.z-b.hi.z,b.lo.z-a.hi.z,0.});
            const double lower=std::sqrt(dx*dx+dy*dy+dz*dz);
            if(lower>out.minimum_distance_m){
                out.broadphase_culled++;
                continue;
            }
            const double distance=triangle_triangle_closest(
                triangle,other).distance;
            out.face_pair_tests++;
            out.minimum_distance_m=std::min(out.minimum_distance_m,distance);
            out.overlapping_face_pairs+=distance<=thickness;
        }
    }
    return out;
}
} // namespace kyoto::solid

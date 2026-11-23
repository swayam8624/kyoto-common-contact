#include "kyoto/triangle_sphere_sweep.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
using namespace kyoto::solid;
static void must(bool v,const std::string& s){
    if(!v)throw std::runtime_error(s);
}
static LinearTriangleMotion vertical(double z0,double z1){
    LinearTriangleMotion t;
    t.start={V3{-0.3,-0.3,z0},V3{0.5,-0.2,z0},
             V3{-0.2,0.5,z0}};
    t.end={V3{-0.3,-0.3,z1},V3{0.5,-0.2,z1},
           V3{-0.2,0.5,z1}};
    return t;
}
int main(){
 try {
    const RigidSphere unit{{0,0,0},1.,.1,.2};
    {
        const auto cert=certify_triangle_sphere_prefix(vertical(2,-2),unit,16);
        must(!cert.entirely_separated,"swept surface tunnelling missed");
        must(cert.certified_safe_prefix<=.25,
            "returned a prefix beyond analytically first sphere contact");
        must(cert.certified_safe_prefix>.24,
            "sweep certificate unreasonably loose at depth16");
        must(cert.first_unresolved_upper>=cert.certified_safe_prefix,
            "invalid unresolved leaf interval");
    }
    {
        const auto cert=certify_triangle_sphere_prefix(vertical(2,2),unit,12);
        must(cert.entirely_separated&&cert.certified_safe_prefix==1,
            "stationary distant triangle not certified");
    }
    {
        // All three initial vertices outside, but triangle INTERIOR
        // intersects sphere. A vertex-based test cannot see this.
        LinearTriangleMotion t;
        t.start={V3{2,0,0},V3{-1,1.7320508075688772,0},
                 V3{-1,-1.7320508075688772,0}};
        t.end=t.start;
        const auto cert=certify_triangle_sphere_prefix(t,unit,9);
        must(!cert.entirely_separated&&cert.certified_safe_prefix==0,
             "initial triangle/sphere overlap must invalidate safe motion");
        must(cert.sampled_overlap,"true static triangle contact was not witnessed");
    }
    std::mt19937_64 rng(2272026);
    std::uniform_real_distribution<double> coord(-2.5,2.5);
    std::uniform_real_distribution<double> move(-1.2,1.2);
    std::size_t fully_certified=0,unresolved=0;
    for(int k=0;k<640;k++){
        LinearTriangleMotion t;
        for(int j=0;j<3;j++){
            t.start[j]={coord(rng),coord(rng),coord(rng)};
            t.end[j]=t.start[j]+V3{move(rng),move(rng),move(rng)};
        }
        const auto cert=certify_triangle_sphere_prefix(t,unit,12);
        if(cert.entirely_separated)fully_certified++;else unresolved++;
        // Independent temporal sampling is ONLY a regression check;
        // the no-false-negative argument is the Lipschitz theorem.
        for(int j=0;j<=200;j++){
            const double time=cert.certified_safe_prefix*j/200.;
            const auto triangle=lerp_vertices(t.start,t.end,time);
            const double d=triangle_distance_to_center(triangle,unit.center);
            if(cert.certified_safe_prefix>0)
                must(d>=unit.radius-2e-8,
                     "certified safe interval contains sampled penetration");
            if(cert.entirely_separated)
                must(d>unit.radius-1e-8,
                     "certified clear sweep contains sampled overlap");
        }
    }
    {
        // Real solver-generated counterexample: find the FIRST step
        // in which its vertex-only response permits a triangle overlap.
        // Confirm a full-face conservative sweep flags that transition.
        auto b=toroidal_solid();
        const RigidSphere sphere{};
        bool observed=false;std::size_t flagged=0;
        double safe=1;
        for(int step=1;step<=900;step++){
            const auto original=b;
            const auto start=audit_surface_sphere(b,sphere);
            step_rigid_sphere(b,sphere,.00075);
            const auto end=audit_surface_sphere(b,sphere);
            if(start.penetrating_faces!=0||end.penetrating_faces==0)
                continue;
            for(const auto& face:b.surface_faces){
                LinearTriangleMotion motion;
                for(int i=0;i<3;i++){
                    motion.start[i]=original.nodes[face[i]].x;
                    motion.end[i]=b.nodes[face[i]].x;
                }
                const auto cert=certify_triangle_sphere_prefix(
                    motion,sphere,12);
                if(!cert.entirely_separated){
                    flagged++;
                    safe=std::min(safe,cert.certified_safe_prefix);
                }
            }
            must(flagged>0&&safe<1,
                 "independent whole-face sweep missed observed FEM triangle intrusion");
            std::cout<<"SURFACE_SWEEP_FIRST_FEM_OVERLAP step="<<step
                     <<" flagged_faces="<<flagged
                     <<" earliest_safe_fraction="<<safe<<'\n';
            observed=true;break;
        }
        must(observed,
            "expected reproducible vertex-only FEM surface failure fixture absent");
    }
    std::cout<<"PASS: continuous triangle sphere Lipschitz certificate, "
                "analytic tunnelling, 640 random 3D path proofs with "
                "128640 sample checks, real FEM first-face failure detection; "
             <<"fully_certified="<<fully_certified
             <<" unresolved="<<unresolved<<'\n';
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

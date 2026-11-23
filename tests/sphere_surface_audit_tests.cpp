#include "kyoto/sphere_surface_audit.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace kyoto::solid;
static void check(bool ok,const std::string& m){
    if(!ok)throw std::runtime_error(m);
}
int main(){
 try{
    const V3 a{2,0,0},b{-1,1.7320508075688772,0},
             c{-1,-1.7320508075688772,0};
    {
        const auto d=closest_triangle_point({0,0,0},a,b,c);
        check(d.distance<1e-12,
            "triangle interior must reach sphere although every vertex is outside");
        check(d.barycentric[0]>.3&&d.barycentric[0]<.34,
              "interior barycentric closest point wrong");
        const double sum=d.barycentric[0]+d.barycentric[1]+d.barycentric[2];
        check(std::abs(sum-1)<1e-12,"barycentric normalization failure");
    }
    {
        const auto d=closest_triangle_point({0,0,1.1},a,b,c);
        check(std::abs(d.distance-1.1)<1e-12,
              "triangle-plane near miss distance wrong");
        const auto edge=closest_triangle_point({3,0,0},a,b,c);
        check(std::abs(edge.distance-1)<1e-12,
              "triangle vertex Voronoi case wrong");
    }
    {
        const auto d=closest_triangle_point({.25,.2,1.3},
             {0,0,0},{1,0,0},{0,1,0});
        check(std::abs(d.distance-1.3)<1e-12,
              "projection on interior triangle wrong");
        check(std::abs(d.barycentric[1]-.25)<1e-12,
              "projected barycentrics wrong");
    }
    {
        const auto d=closest_triangle_point({0,2,0},
             {0,0,0},{1,0,0},{2,0,0});
        check(std::abs(d.distance-2)<1e-12,
              "degenerate triangle should use line segments");
    }
    {
        Body b;
        b.nodes={{a,a,{},0,0},{b,b,{},0,0},{c,c,{},0,0}};
        b.surface_faces={{0,1,2}};
        const RigidSphere s{{0,0,0},1,.1,.2};
        const auto audit=audit_surface_sphere(b,s);
        check(audit.outside_vertex_count==3 && audit.inside_vertex_count==0,
              "fixture vertices must be outside");
        check(audit.minimum_vertex_clearance_m>.9,
              "vertices do not clear unit sphere");
        check(audit.penetrating_faces==1&&audit.minimum_surface_clearance_m<-.99,
              "interior triangle-sphere collision must be diagnosed");
        check(audit.tested_faces==1,"face count incorrect");
    }
    {
        auto b=toroidal_solid();
        const RigidSphere s{};
        const auto start=audit_surface_sphere(b,s);
        check(start.tested_faces==b.surface_faces.size(),
              "auditor skipped connected solid boundary faces");
        check(start.minimum_surface_clearance_m>0,
              "rest specimen unexpectedly intersects rigid sphere");
        check(start.inside_vertex_count==0,"rest specimen has penetrating vertex");
        // Diagnostic only: measured real motion, never silently assert
        // that vertex-safe means triangle-safe or call this a CCD proof.
        std::size_t penetrated_states=0,vertex_errors=0;
        double worst_face_gap=1e9;
        for(int step=1;step<=900;step++){
            step_rigid_sphere(b,s,.00075);
            const auto audit=audit_surface_sphere(b,s);
            if(audit.penetrating_faces>0)penetrated_states++;
            if(audit.inside_vertex_count>0)vertex_errors++;
            worst_face_gap=std::min(worst_face_gap,audit.minimum_surface_clearance_m);
        }
        check(vertex_errors==0,"vertex solver failed its declared guarantee");
        check(std::isfinite(worst_face_gap),"invalid surface diagnostic");
        std::cout<<"TRIANGLE_SPHERE_AUDIT real_FEM_900_steps "
                 <<"states_with_face_overlap="<<penetrated_states
                 <<" worst_surface_clearance_m="<<worst_face_gap
                 <<" (not suppressed; must be resolved separately)\n";
    }
    std::cout<<"PASS: independent closest-point surface audit, triangle "
                "interior collision despite outside vertices, 900 step diagnostics\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}

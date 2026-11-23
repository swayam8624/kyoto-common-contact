// Pass 11B: physically integrated finite rigid triangle patch contact.
//
// The obstacle is 49 original connected rigid vertices and 72 sloped,
// finite triangles, not a plane and not a sphere. Source FEM is the
// same 180-node, 480-tet neo-Hookean freely deformable torus.
//
// Each accepted step re-integrates its REAL physical time, rejects any
// uncertified triangle/triangle swept interval, and checks the entire
// 320x72 finite-face separation plus positive tetrahedral Jacobians.
//
// A two-sided open surface has no intrinsic signed interior and does
// not support closed-shell "inside/outside" assertions. Zero measured
// triangle intersection is NOT self-contact or implicit IPC solver.

#include "kyoto/rigid_mesh_fem_contact.hpp"
#include "kyoto/whole_face_contact_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace kyoto::solid;
static void need(bool ok,const char* why){
    if(!ok)throw std::runtime_error(why);
}
static void equal_body(const Body& a,const Body& b,double tol){
    need(a.nodes.size()==b.nodes.size(),"different source FEM topology");
    need(std::abs(a.time_seconds-b.time_seconds)<=tol,
         "FEM physical clocks differ");
    for(std::size_t i=0;i<a.nodes.size();i++){
        need(norm(a.nodes[i].x-b.nodes[i].x)<=tol,
             "FEM physical positions unexpectedly changed");
        need(norm(a.nodes[i].v-b.nodes[i].v)<=tol,
             "FEM physical velocities unexpectedly changed");
    }
}
int main(int argc,char**argv){
 try{
    if(argc>2)throw std::invalid_argument(
      "usage: kyoto_rigid_mesh_fem_tests [optional evidence directory]");
    const StaticTriangleMesh patch=corrugated_contact_patch();
    need(patch.vertices.size()==49&&patch.faces.size()==72,
         "source curved rigid triangular surface topology mismatch");
    // Regression: absent contact, exactly reproduce the uncut
    // physical FEM integrator; no invented geometry or velocity.
    {
        Body newbody=toroidal_solid(),old=newbody;
        auto far=corrugated_contact_patch();
        for(auto& p:far.vertices)p.z-=100;
        const RigidSphere far_sphere{{0,0,-200},.7,0,.2};
        for(int i=0;i<14;i++){
            const auto a=step_time_consistent_rigid_triangle_mesh(
                newbody,far,.00075);
            const auto b=step_whole_face_sphere(old,far_sphere,.00075);
            need(!b.time_clipped&&a.accepted_leaves==1&&
                 a.face_impulse_updates==0,
                 "distant rigid patch unexpectedly applies contact");
            equal_body(newbody,old,1e-11);
        }
        std::cout<<"PASS11_TRIANGLE_MESH_FREEFLIGHT_REFERENCE\n";
    }
    // Explicit rollback across the ENTIRE requested physical step.
    {
        Body original=toroidal_solid();
        auto raised=corrugated_contact_patch();
        const auto initial=audit_deformable_rigid_mesh(original,raised,0.);
        const double raise=initial.minimum_distance_m-.015;
        for(auto& p:raised.vertices)p.z+=raise;
        need(std::abs(audit_deformable_rigid_mesh(
           original,raised,0.).minimum_distance_m-.015)<.004,
           "fast finite-mesh fixture not sufficiently close");
        for(auto& node:original.nodes)node.v={0,0,-50};
        Body candidate=original;
        RigidMeshContactSettings cfg;
        cfg.maximum_bisection_depth=0;
        std::vector<MeshPhysicalLeaf> receipts{{-1,0,.1}};
        bool failed=false;
        try{
            (void)step_time_consistent_rigid_triangle_mesh(
               candidate,raised,.00075,cfg,&receipts);
        }catch(const std::runtime_error&){failed=true;}
        need(failed,"underbudget tunnelling mesh trial was silently accepted");
        equal_body(original,candidate,0);
        need(receipts.size()==1&&receipts[0].start_time_s==-1,
             "failed finite mesh physical step leaked receipts");
        std::cout<<"PASS11_TRIANGLE_MESH_ATOMIC_ROLLBACK\n";
    }

    Body body=toroidal_solid();
    const int steps=argc==2?900:450;
    std::ofstream history,vertices,face_csv,obstacle_vertices,obstacle_faces;
    if(argc==2){
        const std::filesystem::path dir=argv[1];
        std::filesystem::create_directories(dir);
        history.open(dir/"rigid_triangle_physics_900.csv");
        vertices.open(dir/"rigid_triangle_physical_vertices.csv");
        face_csv.open(dir/"deformable_surface_triangles.csv");
        obstacle_vertices.open(dir/"rigid_patch_original_vertices.csv");
        obstacle_faces.open(dir/"rigid_patch_original_triangles.csv");
        history<<std::setprecision(17);
        vertices<<std::setprecision(17);
        obstacle_vertices<<std::setprecision(17);
        history<<"step,physical_time_s,accepted_leaves,rejected_geom,"
                 "rejected_tetra,actual_accepted_time_s,"
                 "active_contact_pairs,face_impulse_updates,"
                 "normal_impulse_ns,external_impulse_norm_ns,"
                 "finite_triangle_min_distance_m,"
                 "min_tetra_j,kinetic_j,elastic_j,gravity_j,"
                 "com_x,com_y,com_z,paid_pair_nearest_feature_tests,"
                 "aabb_lower_bound_prunes\n";
        vertices<<"step,physical_time_s,vertex_id,"
                  "rest_x,rest_y,rest_z,x,y,z,vx,vy,vz,"
                  "von_mises_pa,displacement_m\n";
        face_csv<<"face_id,a,b,c\n";
        for(std::size_t i=0;i<body.surface_faces.size();i++){
            const auto f=body.surface_faces[i];
            face_csv<<i<<','<<f[0]<<','<<f[1]<<','<<f[2]<<'\n';
        }
        obstacle_vertices<<"vertex_id,x,y,z\n";
        for(std::size_t i=0;i<patch.vertices.size();i++){
            const V3 p=patch.vertices[i];
            obstacle_vertices<<i<<','<<p.x<<','<<p.y<<','<<p.z<<'\n';
        }
        obstacle_faces<<"face_id,a,b,c\n";
        for(std::size_t i=0;i<patch.faces.size();i++){
            const auto f=patch.faces[i];
            obstacle_faces<<i<<','<<f[0]<<','<<f[1]<<','<<f[2]<<'\n';
        }
    }
    std::size_t contact_steps=0,face_impulses=0,accepted=0,
                rejected_geometry=0,rejected_tetra=0,paid_pair_tests=0;
    double smallest_gap=std::numeric_limits<double>::infinity();
    double smallest_J=std::numeric_limits<double>::infinity();
    double largest_stress=0;
    double first_contact=-1;
    MeshAdaptiveStats last;
    for(int step=0;step<=steps;step++){
        MeshAdaptiveStats metric;
        if(step>0){
            metric=step_time_consistent_rigid_triangle_mesh(
                body,patch,.00075);
            double nodal_sum=0;
            for(const auto& node:body.nodes)
                nodal_sum+=node.last_normal_impulse;
            need(std::abs(nodal_sum-metric.normal_impulse_ns)<
                 1e-8*std::max(1.,metric.normal_impulse_ns),
                 "routine mesh contact nodal impulses disagree with accepted macro-step total");
            accepted+=metric.accepted_leaves;
            rejected_geometry+=metric.rejected_geom_trials;
            rejected_tetra+=metric.rejected_tetra_trials;
            contact_steps+=metric.face_impulse_updates>0;
            face_impulses+=metric.face_impulse_updates;
            if(first_contact<0&&metric.face_impulse_updates>0)
                first_contact=body.time_seconds;
        }
        const auto surface=audit_deformable_rigid_mesh(
            body,patch,0.);
        paid_pair_tests+=surface.face_pair_tests;
        const auto fem=body.diagnostics(0);
        need(surface.overlapping_face_pairs==0 &&
             surface.minimum_distance_m>0,
             "accepted REAL triangular rigid-mesh FEM has intersecting faces");
        need(std::isfinite(fem.elastic_joule)&&
             std::isfinite(fem.kinetic_joule)&&
             fem.min_jacobian>1e-6,
             "true finite mesh contact inverted FEM or lost finite energy");
        need(std::abs(body.time_seconds-.00075*step)<1e-9,
             "true rigid triangular contact advanced incorrect physical clock");
        smallest_gap=std::min(smallest_gap,surface.minimum_distance_m);
        smallest_J=std::min(smallest_J,fem.min_jacobian);
        largest_stress=std::max(largest_stress,fem.max_von_mises_pa);
        if(history.is_open()){
            history<<step<<','<<body.time_seconds<<','
                   <<metric.accepted_leaves<<','
                   <<metric.rejected_geom_trials<<','
                   <<metric.rejected_tetra_trials<<','
                   <<metric.integrated_dt_s<<','
                   <<metric.active_face_pairs<<','
                   <<metric.face_impulse_updates<<','
                   <<metric.normal_impulse_ns<<','
                   <<norm(metric.external_contact_impulse)<<','
                   <<surface.minimum_distance_m<<','
                   <<fem.min_jacobian<<','
                   <<fem.kinetic_joule<<','
                   <<fem.elastic_joule<<','
                   <<fem.gravity_joule<<','
                   <<fem.center_of_mass.x<<','
                   <<fem.center_of_mass.y<<','
                   <<fem.center_of_mass.z<<','
                   <<surface.face_pair_tests<<','
                   <<surface.broadphase_culled<<'\n';
            if(step%10==0){
                const auto field=body.evaluate();
                for(std::size_t i=0;i<body.nodes.size();i++){
                    const auto& n=body.nodes[i];
                    vertices<<step<<','<<body.time_seconds<<','
                      <<i<<','<<n.rest.x<<','<<n.rest.y<<','
                      <<n.rest.z<<','<<n.x.x<<','<<n.x.y<<','
                      <<n.x.z<<','<<n.v.x<<','<<n.v.y<<','
                      <<n.v.z<<','<<field.nodal_von_mises_pa[i]<<','
                      <<norm(n.x-n.rest)<<'\n';
                }
            }
        }
        last=metric;
    }
    need(contact_steps>0&&face_impulses>0,
         "finite rigid triangular obstacle never produced REAL physical impulses");
    need(accepted>=std::size_t(steps) && first_contact>0,
         "physical elapsed mesh-contact time incomplete");
    if(argc==2){
        const std::filesystem::path dir=argv[1];
        std::ofstream manifest(dir/"rigid_mesh_manifest.json");
        manifest<<std::setprecision(17)
          <<"{\n"
          <<" \"original_solver\":\"neoHookean_3D_480tet_Cpp23\",\n"
          <<" \"actual_rigid_triangle_contact_response\":true,\n"
          <<" \"sphere_used_to_resolve_mesh_contact\":false,\n"
          <<" \"rigid_mesh_static_and_two_sided_open_surface\":true,\n"
          <<" \"self_contact_solved\":false,\n"
          <<" \"general_rigid_obstacle_motion_solved\":false,\n"
          <<" \"rounding_formally_certified\":false,\n"
          <<" \"implicit_KKT_or_IPC_barrier_physics\":false,\n"
          <<" \"deformable_vertices\":"<<body.nodes.size()<<",\n"
          <<" \"deformable_tetrahedra\":"<<body.tets.size()<<",\n"
          <<" \"deformable_surface_triangles\":"<<body.surface_faces.size()<<",\n"
          <<" \"rigid_vertices\":"<<patch.vertices.size()<<",\n"
          <<" \"rigid_triangles\":"<<patch.faces.size()<<",\n"
          <<" \"physical_steps\":"<<steps<<",\n"
          <<" \"final_physical_time_s\":"<<body.time_seconds<<",\n"
          <<" \"first_contact_time_s\":"<<first_contact<<",\n"
          <<" \"actual_contact_steps\":"<<contact_steps<<",\n"
          <<" \"face_impulse_updates\":"<<face_impulses<<",\n"
          <<" \"accepted_actual_physical_leaves\":"<<accepted<<",\n"
          <<" \"rejected_geometric_trials\":"<<rejected_geometry<<",\n"
          <<" \"rejected_inverted_tet_trials\":"<<rejected_tetra<<",\n"
          <<" \"smallest_full_face_mesh_clearance_m\":"<<smallest_gap<<",\n"
          <<" \"smallest_tetra_jacobian\":"<<smallest_J<<",\n"
          <<" \"maximum_von_mises_pa\":"<<largest_stress<<",\n"
          <<" \"static_face_pair_exact_feature_tests\":"<<paid_pair_tests<<"\n"
          <<"}\n";
    }
    std::cout<<"PASS11_REAL_TRIANGLE_MESH_FEM"
             <<" steps="<<steps
             <<" real_contact_steps="<<contact_steps
             <<" impulse_updates="<<face_impulses
             <<" accepted_leaves="<<accepted
             <<" rejected_geo="<<rejected_geometry
             <<" rejected_tet="<<rejected_tetra
             <<" first_contact_time_s="<<first_contact
             <<" min_full_face_distance_m="<<smallest_gap
             <<" min_tetra_J="<<smallest_J
             <<" max_stress_pa="<<largest_stress
             <<" (real finite mesh contact, not a sphere)\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}

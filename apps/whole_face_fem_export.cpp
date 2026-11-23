// Pass 7A: REAL 3D volumetric FEM with globally synchronized, whole-triangle
// certified safe prefixes and mass-weighted barycentric sphere impulses.
// 900 integrated physical steps, no scripted vertex motion.
// No general IPC mesh-mesh response or proven energy-consistent full-step integration.
#include "kyoto/whole_face_contact_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace kyoto::solid;
int main(int argc,char**argv){
 try {
    if(argc!=2)throw std::invalid_argument(
       "usage: kyoto_whole_face_fem_export OUTPUT_DIR");
    const std::filesystem::path dir=argv[1];
    std::filesystem::create_directories(dir);
    auto b=toroidal_solid();
    const RigidSphere sphere{};
    const WholeFaceContactSettings contact_settings{};
    constexpr double dt=.00075;
    constexpr int steps=900,stride=5,frames=steps/stride+1;

    std::ofstream states(dir/"curved_solid_states.csv");
    states<<std::setprecision(17);
    states<<"frame,time_s,vertex_id,rest_x,rest_y,rest_z,"
            "x,y,z,vx,vy,vz,displacement_m,stress_von_mises_pa,"
            "strain_energy_density_pa,clearance_m\n";
    std::ofstream metrics(dir/"curved_solid_metrics.csv");
    metrics<<std::setprecision(17);
    metrics<<"frame,time_s,kinetic_j,elastic_j,gravitational_j,"
             "mechanical_total_j,min_tet_jacobian,max_stress_pa,"
             "center_x,center_y,center_z,min_vertex_clearance_m,"
             "swept_contacts_interval,normal_impulses_interval,"
             "normal_impulse_ns_interval,total_mass_kg\n";
    // Independent STATIC full-surface audit on every accepted native step.
    // Vertex-only CCD cannot establish triangle/sphere safety.
    std::ofstream surface_audit(dir/"surface_sphere_clearance.csv");
    surface_audit<<std::setprecision(17);
    surface_audit<<"step,time_s,minimum_vertex_clearance_m,"
                    "minimum_triangle_clearance_m,penetrating_triangles,"
                    "tested_triangles,vertex_penetrations,closest_triangle_id\n";
    std::ofstream impacts(dir/"curved_contact_impulses.csv");
    impacts<<std::setprecision(17);
    impacts<<"step,time_s,vertex_id,x,y,z,impulse_ns\n";
    std::ofstream faces(dir/"surface_faces.csv");
    faces<<"id,a,b,c\n";
    for(std::size_t i=0;i<b.surface_faces.size();i++){
        const auto f=b.surface_faces[i];
        faces<<i<<','<<f[0]<<','<<f[1]<<','<<f[2]<<'\n';
    }
    std::ofstream tets(dir/"tetrahedra.csv");
    tets<<std::setprecision(17);
    tets<<"id,a,b,c,d,reference_volume_m3\n";
    for(std::size_t i=0;i<b.tets.size();i++){
        const auto& tet=b.tets[i];
        tets<<i<<','<<tet.n[0]<<','<<tet.n[1]<<','
            <<tet.n[2]<<','<<tet.n[3]<<','<<tet.rest_volume<<'\n';
    }
    std::size_t impulses_total=0,swept_total=0,contact_frames=0;
    std::size_t clipped_steps=0;
    double minimum_accepted_fraction=1;
    std::size_t surface_overlap_states=0,overlap_faces_total=0;
    std::size_t vertex_inside_states=0;
    double worst_surface_gap=std::numeric_limits<double>::infinity();
    std::size_t interval_impulses=0,interval_sweeps=0;
    double interval_normal_ns=0;
    double max_stress=0,min_j=std::numeric_limits<double>::infinity();
    double first_contact_t=-1;
    for(int step=0;step<=steps;step++){
        if(step>0){
            const auto c=step_whole_face_sphere(b,sphere,dt,contact_settings);
            clipped_steps+=c.time_clipped;
            minimum_accepted_fraction=std::min(minimum_accepted_fraction,c.accepted_global_fraction);
            swept_total+=c.whole_face_unresolved;
            impulses_total+=c.normal_impulses;
            interval_sweeps+=c.whole_face_unresolved;
            interval_impulses+=c.normal_impulses;
            interval_normal_ns+=c.total_normal_impulse_ns;
            if(c.normal_impulses>0&&first_contact_t<0)
                first_contact_t=b.time_seconds;
            for(std::size_t i=0;i<b.nodes.size();i++){
                const auto& node=b.nodes[i];
                if(node.last_normal_impulse<=0)continue;
                impacts<<step<<','<<b.time_seconds<<','<<i<<','
                       <<node.x.x<<','<<node.x.y<<','<<node.x.z<<','
                       <<node.last_normal_impulse<<'\n';
            }
        }
        const auto surface=audit_surface_sphere(b,sphere);
        surface_audit<<step<<','<<b.time_seconds<<','
            <<surface.minimum_vertex_clearance_m<<','
            <<surface.minimum_surface_clearance_m<<','
            <<surface.penetrating_faces<<','<<surface.tested_faces<<','
            <<surface.inside_vertex_count<<','
            <<surface.face_nearest_vertex<<'\n';
        if(surface.penetrating_faces>0){
            surface_overlap_states++;
            overlap_faces_total+=surface.penetrating_faces;
        }
        if(surface.inside_vertex_count>0)vertex_inside_states++;
        if(surface.penetrating_faces>0)
            throw std::runtime_error("full-surface contact regression: penetrating triangle");

        worst_surface_gap=std::min(worst_surface_gap,
                                   surface.minimum_surface_clearance_m);
        if(step%stride)continue;
        const int frame=step/stride;
        const auto diag=b.diagnostics(-100);
        const auto field=b.evaluate();
        double clearance=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<b.nodes.size();i++){
            const auto& n=b.nodes[i];
            const V3 dx=n.x-n.rest;
            const double gap=norm(n.x-sphere.center)-sphere.radius;
            if(!(std::isfinite(gap)&&gap>=-2e-9))
                throw std::runtime_error("accepted sphere-vertex interpenetration");
            clearance=std::min(clearance,gap);
            states<<frame<<','<<b.time_seconds<<','<<i<<','
                  <<n.rest.x<<','<<n.rest.y<<','<<n.rest.z<<','
                  <<n.x.x<<','<<n.x.y<<','<<n.x.z<<','
                  <<n.v.x<<','<<n.v.y<<','<<n.v.z<<','
                  <<norm(dx)<<','<<field.nodal_von_mises_pa[i]<<','
                  <<field.nodal_energy_density[i]<<','<<gap<<'\n';
        }
        max_stress=std::max(max_stress,diag.max_von_mises_pa);
        min_j=std::min(min_j,diag.min_jacobian);
        contact_frames+=interval_impulses>0;
        metrics<<frame<<','<<b.time_seconds<<','<<diag.kinetic_joule<<','
               <<diag.elastic_joule<<','<<diag.gravity_joule<<','
               <<diag.kinetic_joule+diag.elastic_joule+diag.gravity_joule<<','
               <<diag.min_jacobian<<','<<diag.max_von_mises_pa<<','
               <<diag.center_of_mass.x<<','<<diag.center_of_mass.y<<','
               <<diag.center_of_mass.z<<','<<clearance<<','
               <<interval_sweeps<<','<<interval_impulses<<','
               <<interval_normal_ns<<','<<diag.total_mass_kg<<'\n';
        interval_impulses=0;interval_sweeps=0;interval_normal_ns=0;
    }
    states.close();metrics.close();impacts.close();faces.close();
    tets.close();surface_audit.close();
    if(!states.good()||!metrics.good()||!impacts.good()||
       !faces.good()||!tets.good()||!surface_audit.good())
        throw std::runtime_error("error writing actual FEM curved scene");
    if(!(first_contact_t>0&&impulses_total>0&&
         max_stress>1&&min_j>0))
        throw std::runtime_error("no valid curved FEM collision");
    std::ofstream manifest(dir/"curved_manifest.json");
    manifest<<std::setprecision(17);
    manifest<<"{\n"
        <<" \"scene\":\"free_3D_hyperelastic_toroid_on_analytic_rigid_sphere\",\n"
        <<" \"simulation_kind\":\"ACTUAL_integrated_neohookean_tetrahedral_FEM\",\n"
        <<" \"physical_motion\":true,\n"
        <<" \"linear_vertex_sphere_analytic_CCD\":false,\n"
        <<" \"whole_triangle_lipschitz_CCD\":true,\n"
        <<" \"machine_certified_rounded_intervals\":false,\n"
        <<" \"full_triangle_sphere_safety\":true,\n"
        <<" \"accepted_static_surface_no_penetration_verified\":true,\n"
        <<" \"general_mesh_mesh_IPC_response\":false,\n"
        <<" \"face_self_collision_resolved\":false,\n"
        <<" \"contact_policy\":\"globally_synchronized_whole_face_prefix_plus_barycentric_effective_mass_impulse\",\n"
        <<" \"accepted_contact_displaces_full_timestep\":false,\n"
        <<" \"sphere_center_xyz_m\":["<<sphere.center.x<<','
               <<sphere.center.y<<','<<sphere.center.z<<"],\n"
        <<" \"sphere_radius_m\":"<<sphere.radius<<",\n"
        <<" \"density_kg_m3\":"<<b.material.density<<",\n"
        <<" \"young_pa\":"<<b.material.young<<",\n"
        <<" \"poisson\":"<<b.material.poisson<<",\n"
        <<" \"gravity_m_s2\":"<<b.material.gravity<<",\n"
        <<" \"dt_s\":"<<dt<<",\n"
        <<" \"steps\":"<<steps<<",\n"
        <<" \"stride\":"<<stride<<",\n"
        <<" \"frames\":"<<frames<<",\n"
        <<" \"vertices\":"<<b.nodes.size()<<",\n"
        <<" \"surface_triangles\":"<<b.surface_faces.size()<<",\n"
        <<" \"tetrahedra\":"<<b.tets.size()<<",\n"
        <<" \"total_mass_kg\":"<<b.diagnostics(-100).total_mass_kg<<",\n"
        <<" \"first_curved_contact_time_s\":"<<first_contact_t<<",\n"
        <<" \"swept_vertex_sphere_hits\":"<<swept_total<<",\n"
        <<" \"normal_impulse_vertex_steps\":"<<impulses_total<<",\n"
        <<" \"contact_output_intervals\":"<<contact_frames<<",\n"
        <<" \"peak_von_mises_pa\":"<<max_stress<<",\n"
        <<" \"minimum_element_jacobian\":"<<min_j<<",\n"
        <<" \"time_clipped_steps\":"<<clipped_steps<<",\n"
        <<" \"minimum_global_accepted_fraction\":"<<minimum_accepted_fraction<<",\n"
        <<" \"contact_skin_m\":"<<contact_settings.clearance_skin_m<<",\n"
        <<" \"surface_audit_states\":"<<(steps+1)<<",\n"
        <<" \"states_with_static_triangle_sphere_overlap\":"<<surface_overlap_states<<",\n"
        <<" \"overlap_face_count_summed_across_states\":"<<overlap_faces_total<<",\n"
        <<" \"states_with_vertex_inside_sphere\":"<<vertex_inside_states<<",\n"
        <<" \"minimum_static_triangle_clearance_m\":"<<worst_surface_gap<<"\n"
        <<"}\n";
    if(!manifest.good())throw std::runtime_error("manifest failed");
    std::cout<<"PASS: synchronized whole-face 3D hyperelastic FEM, "
             <<steps<<" integration steps, "<<frames<<" real frames, "
             <<swept_total<<" whole-face unresolved interval tests, "
             <<impulses_total<<" vertex-step normal impulses; "
             <<"first t="<<first_contact_t<<"s, max Cauchy von Mises "
             <<max_stress<<" Pa, min J="<<min_j
             <<", states with triangle overlap="<<surface_overlap_states
             <<", worst triangle clearance="<<worst_surface_gap<<'\n';
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

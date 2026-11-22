// Kyoto Pass 5: native source provenance for the first actually integrated
// 3D hyperelastic deformable volume. No scripted per-frame motion.
// The only collision constraint is a static infinite plane at Z=0,
// solved analytically by unilateral projection and discrete vertex impulses.
// This is NOT IPC constrained FEM, nor general mesh collision response.

#include "kyoto/solid_dynamics.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace kyoto::solid;
int main(int argc,char** argv){
    try {
        if(argc!=2)throw std::invalid_argument(
            "usage: kyoto_solid_simulation OUTPUT_DIR");
        const std::filesystem::path out=argv[1];
        std::filesystem::create_directories(out);
        Body b=toroidal_solid();
        constexpr double dt=.00075,plane=0.0;
        constexpr int steps=900,output_stride=5;
        constexpr int frames=steps/output_stride+1;

        std::ofstream nodes(out/"solid_states.csv");
        nodes<<std::setprecision(17);
        nodes<<"frame,time_s,vertex_id,rest_x,rest_y,rest_z,"
                "x,y,z,vx,vy,vz,mass_kg,displacement_m,"
                "strain_energy_density_pa,von_mises_pa\n";

        std::ofstream faces(out/"solid_surface_faces.csv");
        faces<<"face_id,a,b,c\n";
        for(std::size_t i=0;i<b.surface_faces.size();i++){
            const auto t=b.surface_faces[i];
            faces<<i<<','<<t[0]<<','<<t[1]<<','<<t[2]<<'\n';
        }
        std::ofstream tets(out/"solid_tetrahedra.csv");
        tets<<std::setprecision(17);
        tets<<"tetra_id,a,b,c,d,reference_volume_m3\n";
        for(std::size_t i=0;i<b.tets.size();i++){
            const auto& t=b.tets[i];
            tets<<i<<','<<t.n[0]<<','<<t.n[1]<<','<<t.n[2]<<','
                <<t.n[3]<<','<<t.rest_volume<<'\n';
        }
        std::ofstream metrics(out/"solid_metrics.csv");
        metrics<<std::setprecision(17);
        metrics<<"frame,time_s,kinetic_j,elastic_j,gravitational_j,"
                 "mechanical_total_j,min_tetra_jacobian,"
                 "max_von_mises_pa,max_displacement_m,"
                 "center_x,center_y,center_z,min_height_m,"
                 "impact_events_this_frame_interval,"
                 "normal_contact_impulse_ns_this_interval,"
                 "total_body_mass_kg\n";
        std::ofstream events(out/"floor_impacts.csv");
        events<<std::setprecision(17);
        events<<"step,time_s,vertex_id,x_m,y_m,z_m,normal_impulse_ns\n";

        std::size_t impacts=0,active_frames=0;
        double peak_stress=0,global_min_J=std::numeric_limits<double>::infinity();
        std::size_t interval_hits=0;
        double interval_impulse=0;
        for(int step=0;step<=steps;step++){
            if(step>0){
                b.step(dt,plane);
                interval_hits+=b.contact_count_step;
                interval_impulse+=b.normal_impulse_step;
                for(std::size_t i=0;i<b.nodes.size();i++){
                    const auto& n=b.nodes[i];
                    if(n.last_normal_impulse>0){
                        events<<step<<','<<b.time_seconds<<','<<i<<','
                              <<n.x.x<<','<<n.x.y<<','<<n.x.z<<','
                              <<n.last_normal_impulse<<'\n';
                    }
                }
            }
            if(step%output_stride!=0)continue;
            const int frame=step/output_stride;
            const auto field=b.evaluate();
            const auto d=b.diagnostics(plane);
            if(d.min_height<-1e-11||d.min_jacobian<=1e-6)
                throw std::runtime_error("invalid integrated physical mesh");
            impacts+=interval_hits;
            active_frames+=interval_hits>0;
            peak_stress=std::max(peak_stress,d.max_von_mises_pa);
            global_min_J=std::min(global_min_J,d.min_jacobian);
            for(std::size_t i=0;i<b.nodes.size();i++){
                const auto& n=b.nodes[i];
                const V3 rest=n.rest,x=n.x,v=n.v;
                nodes<<frame<<','<<d.time_seconds<<','<<i<<','
                     <<rest.x<<','<<rest.y<<','<<rest.z<<','
                     <<x.x<<','<<x.y<<','<<x.z<<','
                     <<v.x<<','<<v.y<<','<<v.z<<','<<n.mass<<','
                     <<norm(x-rest)<<','
                     <<field.nodal_energy_density[i]<<','
                     <<field.nodal_von_mises_pa[i]<<'\n';
            }
            metrics<<frame<<','<<d.time_seconds<<','<<d.kinetic_joule<<','
                   <<d.elastic_joule<<','<<d.gravity_joule<<','
                   <<(d.kinetic_joule+d.elastic_joule+d.gravity_joule)<<','
                   <<d.min_jacobian<<','<<d.max_von_mises_pa<<','
                   <<d.max_displacement<<','<<d.center_of_mass.x<<','
                   <<d.center_of_mass.y<<','<<d.center_of_mass.z<<','
                   <<d.min_height<<','<<interval_hits<<','<<interval_impulse
                   <<','<<d.total_mass_kg<<'\n';
            interval_hits=0;interval_impulse=0;
        }
        nodes.close();faces.close();tets.close();metrics.close();events.close();
        if(!nodes.good()||!faces.good()||!tets.good()||
           !metrics.good()||!events.good())
            throw std::runtime_error("failed writing native simulation source");
        if(!(impacts>0&&peak_stress>1&&global_min_J>0))
            throw std::runtime_error("no physically meaningful contact/deformation");
        std::ofstream manifest(out/"solid_manifest.json");
        manifest<<std::setprecision(17);
        manifest<<"{\n"
            <<" \"scene\": \"free_tetrahedral_solid_toroidal_impact\",\n"
            <<" \"scope\": \"physically_integrated_3D_compressible_neohookean_FEM\",\n"
            <<" \"contact\": \"unilateral_analytic_rigid_plane_vertex_impulses\",\n"
            <<" \"general_mesh_ccd\": false,\n"
            <<" \"upstream_IPC_integration_for_solver\": false,\n"
            <<" \"friction_law\": \"lumped_per_vertex_coulomb_like\",\n"
            <<" \"time_integrator\": \"semi_implicit_euler_explicit_forces\",\n"
            <<" \"fully_implicit_elasticity\": false,\n"
            <<" \"material_model\": \"compressible_neo_hookean_log_J\",\n"
            <<" \"rendered_states_are_solver_generated\": true,\n"
            <<" \"topology\": \"connected_closed_volumetric_torus_rectangular_section\",\n"
            <<" \"frames\": "<<frames<<",\n"
            <<" \"steps\": "<<steps<<",\n"
            <<" \"output_stride\": "<<output_stride<<",\n"
            <<" \"dt_seconds\": "<<dt<<",\n"
            <<" \"floor_z_m\": "<<plane<<",\n"
            <<" \"vertices\": "<<b.nodes.size()<<",\n"
            <<" \"tetrahedra\": "<<b.tets.size()<<",\n"
            <<" \"surface_triangles\": "<<b.surface_faces.size()<<",\n"
            <<" \"rest_volume_m3\": "<<b.rest_volume<<",\n"
            <<" \"density_kg_m3\": "<<b.material.density<<",\n"
            <<" \"young_pa\": "<<b.material.young<<",\n"
            <<" \"poisson\": "<<b.material.poisson<<",\n"
            <<" \"gravity_m_s2\": "<<b.material.gravity<<",\n"
            <<" \"restitution\": "<<b.material.restitution<<",\n"
            <<" \"friction\": "<<b.material.friction<<",\n"
            <<" \"viscous_damping_inv_s\": "<<b.material.damping<<",\n"
            <<" \"impact_events\": "<<impacts<<",\n"
            <<" \"frames_with_impacts\": "<<active_frames<<",\n"
            <<" \"maximum_von_mises_pa\": "<<peak_stress<<",\n"
            <<" \"minimum_tetra_jacobian\": "<<global_min_J<<"\n"
            <<"}\n";
        if(!manifest.good())throw std::runtime_error("manifest failed");
        std::cout<<"PASS: 3D hyperelastic FEM source export: "
                 <<frames<<" frames, "<<b.nodes.size()<<" moving 3D nodes, "
                 <<b.tets.size()<<" tetrahedral elements, "
                 <<impacts<<" physical ground impulses, peak stress "
                 <<peak_stress<<" Pa, min J "<<global_min_J
                 <<"; NO scripted vertex trajectories\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
    }
}

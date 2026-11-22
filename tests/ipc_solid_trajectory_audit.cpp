// Pass-5 external audit of **actually integrated** FEM trajectories.
// The native solid solver handles its infinite plane analytically.
// This opt-in executable uses real official IPC Toolkit v1.6.1 LBVH and
// continuous CCD to inspect selected *accepted* linear integration steps.
//
// Passing a candidate+CCD check is not equivalent to a general solver
// that resolves self-contact. Self-contact is reported, NEVER hidden.
// A rigid floor triangle patch is used for independent geometric review;
// it approximates the analytic infinite plane only within the specimen
// footprint. This audit samples a subset of 900 actual solver steps.

#include "kyoto/solid_dynamics.hpp"
#include <ipc/broad_phase/lbvh.hpp>
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using kyoto::solid::Body;
using kyoto::solid::V3;
using kyoto::solid::toroidal_solid;
struct Probe {
    int vf{},ee{},ccd_vf{},ccd_ee{};
    double earliest_toi{1};
    double build_and_candidates_ms{},ccd_ms{};
};
static Eigen::MatrixXd positions(const std::vector<V3>& points){
    Eigen::MatrixXd A(points.size(),3);
    for(std::size_t i=0;i<points.size();i++)
        A.row(static_cast<Eigen::Index>(i))<<points[i].x,points[i].y,points[i].z;
    return A;
}
static std::vector<V3> positions(const Body& b){
    std::vector<V3> result;result.reserve(b.nodes.size());
    for(const auto& n:b.nodes)result.push_back(n.x);
    return result;
}
static void connectivity(const Body& body,
                         Eigen::MatrixXi& E,Eigen::MatrixXi& F,
                         bool with_floor){
    const int en=body.surface_edges.size();
    const int fn=body.surface_faces.size();
    E.resize(en+(with_floor?5:0),2);
    F.resize(fn+(with_floor?2:0),3);
    for(int i=0;i<en;i++)
        for(int k=0;k<2;k++)E(i,k)=body.surface_edges[i][k];
    for(int i=0;i<fn;i++)
        for(int k=0;k<3;k++)F(i,k)=body.surface_faces[i][k];
    if(with_floor){
        const int n=body.nodes.size();
        const std::array<std::array<int,2>,5> edges={
            std::array<int,2>{0,1},{1,2},{2,3},{3,0},{0,2}};
        const std::array<std::array<int,3>,2> faces={
            std::array<int,3>{0,1,2},{0,2,3}};
        for(int j=0;j<5;j++)for(int k=0;k<2;k++)
            E(en+j,k)=n+edges[j][k];
        for(int j=0;j<2;j++)for(int k=0;k<3;k++)
            F(fn+j,k)=n+faces[j][k];
    }
}
static const std::array<V3,4> floor_patch{{
    {-1.35,-1.35,0},{1.35,-1.35,0},
    {1.35,1.35,0},{-1.35,1.35,0}
}};
static Probe inspect(const Eigen::MatrixXd& X0,const Eigen::MatrixXd& X1,
                     const Eigen::MatrixXi& edges,const Eigen::MatrixXi& faces,
                     int nbody,bool floor_cross_only) {
    ipc::LBVH official;
    if(floor_cross_only)
        official.can_vertices_collide=[nbody](std::size_t i,std::size_t j){
            return (i<std::size_t(nbody)) != (j<std::size_t(nbody));
        };
    else
        official.can_vertices_collide=[](auto,auto){return true;};
    const auto start=std::chrono::steady_clock::now();
    official.build(X0,X1,edges,faces,0.);
    std::vector<ipc::FaceVertexCandidate> vf;
    std::vector<ipc::EdgeEdgeCandidate> ee;
    official.detect_face_vertex_candidates(vf);
    official.detect_edge_edge_candidates(ee);
    const auto split=std::chrono::steady_clock::now();
    Probe result;
    result.vf=vf.size();result.ee=ee.size();
    for(const auto& c:vf) {
        double toi=1;
        if(c.ccd(c.dof(X0,edges,faces),c.dof(X1,edges,faces),toi)) {
            result.ccd_vf++;
            result.earliest_toi=std::min(result.earliest_toi,toi);
        }
    }
    for(const auto& c:ee) {
        double toi=1;
        if(c.ccd(c.dof(X0,edges,faces),c.dof(X1,edges,faces),toi)) {
            result.ccd_ee++;
            result.earliest_toi=std::min(result.earliest_toi,toi);
        }
    }
    const auto end=std::chrono::steady_clock::now();
    result.build_and_candidates_ms=
      std::chrono::duration<double,std::milli>(split-start).count();
    result.ccd_ms=std::chrono::duration<double,std::milli>(end-split).count();
    return result;
}
int main(int argc,char**argv){
    try{
        if(argc!=2)throw std::invalid_argument(
            "usage: kyoto_ipc_solid_audit OUTPUT_DIR");
        const auto dir=std::filesystem::path(argv[1]);
        std::filesystem::create_directories(dir);
        Body body=toroidal_solid();
        Eigen::MatrixXi Efloor,Ffloor,Eself,Fself;
        connectivity(body,Efloor,Ffloor,true);
        connectivity(body,Eself,Fself,false);
        const int nb=body.nodes.size();
        std::ofstream trace(dir/"official_ipc_on_real_fem.csv");
        trace<<std::setprecision(17);
        trace<<"step,time_s,analytic_min_height_m,solver_plane_impulse_events,"
            "ipc_cross_vf_candidates,ipc_cross_ee_candidates,"
            "ipc_cross_vf_ccd_events,ipc_cross_ee_ccd_events,"
            "ipc_cross_earliest_toi,ipc_build_query_ms,"
            "ipc_ccd_paid_ms,self_audit_performed,"
            "ipc_self_vf_candidates,ipc_self_ee_candidates,"
            "ipc_self_vf_ccd_events,ipc_self_ee_ccd_events\n";
        std::size_t sampled=0,total_hit=0,total_self_hit=0;
        double max_paid=0;
        for(int step=1;step<=900;step++){
            const auto previous=positions(body);
            body.step(.00075,0);
            // Observe all selected physical integration steps, including
            // a denser window at the first actual ground impact.
            const bool sample=step%20==0||
                (step>=335 && step<=425 && step%5==0);
            if(!sample)continue;
            const auto after=positions(body);
            for(const auto& p:after)
                if(p.z < -1e-12 || !kyoto::solid::finite(p))
                    throw std::runtime_error("invalid physically accepted floor state");
            std::vector<V3> start=previous,final=after;
            start.insert(start.end(),floor_patch.begin(),floor_patch.end());
            final.insert(final.end(),floor_patch.begin(),floor_patch.end());
            const Probe cross=inspect(positions(start),positions(final),
                                     Efloor,Ffloor,nb,true);
            const bool self=step%150==0;
            Probe interior;
            if(self){
                interior=inspect(positions(previous),positions(after),
                                 Eself,Fself,nb,false);
                total_self_hit+=interior.ccd_vf+interior.ccd_ee;
            }
            double min_z=std::numeric_limits<double>::infinity();
            for(const auto& p:after)min_z=std::min(min_z,p.z);
            total_hit+=cross.ccd_vf+cross.ccd_ee;
            max_paid=std::max(max_paid,
                              cross.build_and_candidates_ms+cross.ccd_ms);
            trace<<step<<','<<body.time_seconds<<','<<min_z<<','
                 <<body.contact_count_step<<','<<cross.vf<<','<<cross.ee<<','
                 <<cross.ccd_vf<<','<<cross.ccd_ee<<','
                 <<cross.earliest_toi<<','<<cross.build_and_candidates_ms
                 <<','<<cross.ccd_ms<<','<<int(self)<<','
                 <<interior.vf<<','<<interior.ee<<','
                 <<interior.ccd_vf<<','<<interior.ccd_ee<<'\n';
            sampled++;
        }
        if(!trace.good())throw std::runtime_error("source export failed");
        trace.close();
        std::ofstream manifest(dir/"manifest.json");
        manifest<<"{\n"
            <<" \"external_reference\": \"ACTUAL_PINNED_IPC_TOOLKIT_1.6.1\",\n"
            <<" \"tested_source\": \"native_hyperelastic_FEM_integrated_3D_torus\",\n"
            <<" \"trajectory_assumption\": \"piecewise_linear_per_native_integrator_step\",\n"
            <<" \"all_900_steps_independently_CCD_audited\": false,\n"
            <<" \"self_contact_is_resolved_by_solver\": false,\n"
            <<" \"analytic_infinite_plane_response\": true,\n"
            <<" \"independent_finite_triangulated_floor_audit\": true,\n"
            <<" \"total_solver_steps\": 900,\n"
            <<" \"sampled_steps\": "<<sampled<<",\n"
            <<" \"upstream_cross_CCD_events_on_sampled_steps\": "<<total_hit<<",\n"
            <<" \"upstream_self_CCD_events_on_sampled_steps\": "<<total_self_hit<<",\n"
            <<" \"max_upstream_paid_ms_single_sample\": "<<max_paid<<"\n"
            <<"}\n";
        if(sampled<50)throw std::runtime_error("insufficient sampled solver steps");
        std::cout<<"PASS: actual IPC Toolkit geometric audit on "
                 <<sampled<<" *physically integrated* 3D FEM steps; "
                 <<"cross-surface CCD events="<<total_hit
                 <<" unhandled sampled self-event signals="<<total_self_hit
                 <<" (reported, not hidden); max IPC cost per sample "
                 <<max_paid<<" ms\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
    }
}

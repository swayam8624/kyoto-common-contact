// Pass 9 independent swept-contact audit.
//
// Unlike the solver's midpoint-Lipschitz certificate, this audit:
///  1. computes point/triangle distance using independent Voronoi regions;
///  2. certifies continuous intervals from six-endpoint convex hull
///     separating-plane witnesses;
///  3. never treats an unresolved interval as "no collision";
///  4. checks REAL accepted FEM paths, replayed at their physical leaves.
/// Ordinary binary64 is used: this is NOT an exact-predicate CCD proof.

#include "kyoto/independent_prism_audit.hpp"
#include "kyoto/independent_triangle_distance.hpp"
#include "kyoto/time_consistent_contact.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

using namespace kyoto::solid;
static void must(bool yes,const char* why){
    if(!yes)throw std::runtime_error(why);
}
static bool close(double a,double b,double tol=1e-10){
    return std::abs(a-b)<=tol;
}

struct AuditedPath {
    std::size_t certified{},unresolved{},support_tests{},time_nodes{};
    double nearest_static_gap{std::numeric_limits<double>::infinity()};
    double largest_static_distance_disagreement{};
};

// The independent oracle NEVER asks the native collision checker whether
// its proposed path is clear; native states supply ONLY the true motion.
static AuditedPath audit_native_leaf(
    const Body& from,const Body& to,const RigidSphere& sphere,
    unsigned max_depth=12,std::ostream* face_receipts=nullptr,
    std::size_t leaf_index=0) {
    must(from.surface_faces.size()==to.surface_faces.size() &&
         from.nodes.size()==to.nodes.size(),"inconsistent geometry topology");
    AuditedPath r;
    for(std::size_t face_id=0;face_id<from.surface_faces.size();face_id++){
        const auto& face=from.surface_faces[face_id];
        LinearTriangleMotion motion;
        for(int i=0;i<3;i++){
            motion.start[i]=from.nodes[face[i]].x;
            motion.end[i]=to.nodes[face[i]].x;
        }
        // Independent static Voronoi oracle at BOTH leaf endpoints.
        for(const auto& state:std::array<const Body*,2>{&from,&to}){
            const V3 a=state->nodes[face[0]].x;
            const V3 b=state->nodes[face[1]].x;
            const V3 c=state->nodes[face[2]].x;
            const double independent=
                independent_triangle_distance(sphere.center,a,b,c).distance;
            const double native=
                closest_triangle_point(sphere.center,a,b,c).distance;
            r.largest_static_distance_disagreement=std::max(
                r.largest_static_distance_disagreement,
                std::abs(independent-native));
            r.nearest_static_gap=std::min(
                r.nearest_static_gap,independent-sphere.radius);
            must(independent>=sphere.radius-3e-9,
                 "independent point/triangle audit finds penetrating solid");
            must(close(independent,native,1e-8),
                 "independent distance oracle disagrees with native geometry");
        }
        const auto prism=audit_triangle_sphere_prism(
            motion,sphere,max_depth);
        r.support_tests+=prism.support_planes_tested;
        r.time_nodes+=prism.time_nodes;
        if(face_receipts){
            const V3 center=(motion.end[0]+motion.end[1]+motion.end[2])/3.;
            const auto observed=independent_triangle_distance(
                sphere.center,motion.end[0],motion.end[1],motion.end[2]);
            (*face_receipts)<<leaf_index<<','<<face_id<<','
                <<center.x<<','<<center.y<<','<<center.z<<','
                <<(observed.distance-sphere.radius)<<','
                <<int(prism.separated)<<','
                <<prism.support_planes_tested<<','
                <<prism.time_nodes<<'\n';
        }
        if(prism.separated)r.certified++;
        else {
            r.unresolved++;
            // Finite sampling detects regressions in unresolved intervals.
            // It is NOT an independent continuous safety proof.
            for(int sample=0;sample<=32;sample++){
                const double alpha=double(sample)/32.;
                const auto state=lerp_vertices(
                    motion.start,motion.end,alpha);
                const double d=independent_triangle_distance(
                    sphere.center,state[0],state[1],state[2]).distance;
                must(d>=sphere.radius-3e-9,
                     "unresolved FEM path has sampled real penetration");
            }
        }
    }
    return r;
}

static void compare_state(const Body& a,const Body& b){
    must(a.nodes.size()==b.nodes.size(),"state topology mismatch");
    must(close(a.time_seconds,b.time_seconds,1e-10),
         "replayed physical leaf clock differs");
    for(std::size_t i=0;i<a.nodes.size();i++){
        must(norm(a.nodes[i].x-b.nodes[i].x)<1e-9,
             "actual FEM replay positions differ");
        must(norm(a.nodes[i].v-b.nodes[i].v)<1e-9,
             "actual FEM replay velocities differ");
    }
}
int main(int argc,char**argv){
 try {
    if(argc>2)throw std::invalid_argument(
        "usage: kyoto_independent_contact_tests [optional evidence directory]");
    std::ofstream stream;
    std::ofstream fast;
    std::ofstream face_csv;
    if(argc==2) {
        std::filesystem::create_directories(argv[1]);
        stream.open(std::filesystem::path(argv[1])/"real_fem_independent_sweeps.csv");
        fast.open(std::filesystem::path(argv[1])/"fast_impact_independent_sweeps.csv");
        face_csv.open(std::filesystem::path(argv[1])/
                      "fast_impact_face_witnesses.csv");
        face_csv<<std::setprecision(17);
        face_csv<<"leaf_id,face_id,center_x,center_y,center_z,"
                  "independent_end_clearance_m,continuously_certified,"
                  "support_plane_tests,temporal_nodes\n";
        stream<<std::setprecision(17);
        fast<<std::setprecision(17);
        stream<<"macro_step,time_s,accepted_physical_leaves,"
                "faces_independently_certified,faces_unresolved,"
                "support_plane_evaluations,swept_time_nodes,"
                "independent_min_static_clearance_m,"
                "max_static_oracle_disagreement_m\n";
        fast<<"leaf_id,physical_end_time_s,physical_dt_s,"
              "faces_independently_certified,faces_unresolved,"
              "support_plane_evaluations,swept_time_nodes,"
              "independent_min_static_clearance_m,"
              "max_static_oracle_disagreement_m\n";
    }
    const RigidSphere unit{{0,0,0},1.,0,.2};
    {
        // Grounded geometry check: a very large triangle spans the
        // origin although every vertex is OUTSIDE the unit sphere.
        const V3 a{2,0,0},b{-1,1.7320508075688772,0},
                 c{-1,-1.7320508075688772,0};
        const auto oracle=independent_triangle_distance({0,0,0},a,b,c);
        must(oracle.distance<1e-11,
             "independent Voronoi oracle missed interior penetration");
        LinearTriangleMotion m{{a,b,c},{a,b,c}};
        const auto prism=audit_triangle_sphere_prism(m,unit,4);
        must(!prism.separated&&prism.unresolved_terminal_nodes>0,
             "static interior collision falsely certified as separated");
    }
    {
        // Two separated terminal configurations with an undeniable
        // collision at their common temporal midpoint.
        LinearTriangleMotion m;
        m.start={V3{-0.4,-0.3,2},V3{0.6,-0.3,2},
                 V3{-0.2,0.6,2}};
        m.end={V3{-0.4,-0.3,-2},V3{0.6,-0.3,-2},
               V3{-0.2,0.6,-2}};
        for(int i=0;i<3;i++) {
            must(norm(m.start[i])>unit.radius &&
                 norm(m.end[i])>unit.radius,
                 "test vertices should be outside both endpoints");
        }
        const auto witness=lerp_vertices(m.start,m.end,.5);
        must(independent_triangle_distance(unit.center,witness[0],
                  witness[1],witness[2]).distance<unit.radius,
             "midstep penetrating triangle fixture broken");
        must(!audit_triangle_sphere_prism(m,unit,6).separated,
             "independent continuous prism certificate missed collision");
    }
    {
        // Exactly tangent / almost tangent paths must remain
        // unresolved unless a STRICT separating witness exists.
        LinearTriangleMotion m;
        m.start={V3{-1,-1,1},V3{1,-1,1},V3{0,2,1}};
        m.end=m.start;
        must(!audit_triangle_sphere_prism(m,unit,0).separated,
             "grazing tangent incorrectly called strictly separated");
        for(auto& v:m.start)v.z=1+3e-13;
        m.end=m.start;
        must(!audit_triangle_sphere_prism(m,unit,0).separated,
             "near machine-ulp tangency incorrectly certified");
    }
    std::mt19937_64 rng(20261123);
    std::uniform_real_distribution<double> xy(-1.8,1.8),
        z(2.25,3.6),delta(-.12,.12);
    double largest_error=0;
    for(int n=0;n<1000;n++){
        LinearTriangleMotion m;
        for(int j=0;j<3;j++){
            m.start[j]={xy(rng),xy(rng),z(rng)};
            m.end[j]=m.start[j]+V3{
                delta(rng),delta(rng),delta(rng)};
        }
        const auto proof=audit_triangle_sphere_prism(m,unit,9);
        must(proof.separated,
             "far and genuinely separated moving triangle uncertified");
        const auto mid=lerp_vertices(m.start,m.end,.35);
        const auto distinct=independent_triangle_distance(
            unit.center,mid[0],mid[1],mid[2]);
        const auto original=closest_triangle_point(
            unit.center,mid[0],mid[1],mid[2]);
        largest_error=std::max(largest_error,
                               std::abs(distinct.distance-original.distance));
        must(close(distinct.distance,original.distance,1e-8),
             "independent static Voronoi oracle randomized mismatch");
    }
    {
        // Degenerate triangles are a documented separate fallback.
        const auto a=independent_triangle_distance(
          {0,2,0},{0,0,0},{1,0,0},{2,0,0});
        must(close(a.distance,2),"degenerate line oracle incorrect");
    }
    Body body=toroidal_solid();
    const RigidSphere real_sphere{};
    AuditedPath total;
    std::size_t accepted_leaves=0;
    double min_j=std::numeric_limits<double>::infinity();
    for(int step=1;step<=900;step++){
        const Body before=body;
        std::vector<AcceptedPhysicalSubstep> receipts;
        const auto result=step_time_consistent_whole_face_sphere(
            body,real_sphere,.00075,{},&receipts);
        must(receipts.size()==result.accepted_substeps,
             "original physical accepted receipts missing");
        Body replay=before;
        AuditedPath local;
        for(const auto& leaf:receipts){
            Body next=replay;
            const auto old=step_whole_face_sphere(
                next,real_sphere,leaf.dt_s);
            must(!old.time_clipped,
                "accepted physical leaf cannot be geometrically clipped");
            const auto audit=audit_native_leaf(
                replay,next,real_sphere,12);
            local.certified+=audit.certified;
            local.unresolved+=audit.unresolved;
            local.support_tests+=audit.support_tests;
            local.time_nodes+=audit.time_nodes;
            local.nearest_static_gap=std::min(
                local.nearest_static_gap,audit.nearest_static_gap);
            local.largest_static_distance_disagreement=std::max(
                local.largest_static_distance_disagreement,
                audit.largest_static_distance_disagreement);
            min_j=std::min(min_j,next.evaluate().min_jacobian);
            replay=std::move(next);
        }
        compare_state(replay,body);
        accepted_leaves+=receipts.size();
        total.certified+=local.certified;
        total.unresolved+=local.unresolved;
        total.support_tests+=local.support_tests;
        total.time_nodes+=local.time_nodes;
        total.nearest_static_gap=std::min(
            total.nearest_static_gap,local.nearest_static_gap);
        total.largest_static_distance_disagreement=std::max(
            total.largest_static_distance_disagreement,
            local.largest_static_distance_disagreement);
        if(stream.is_open())
            stream<<step<<','<<body.time_seconds<<','<<receipts.size()<<','
                  <<local.certified<<','<<local.unresolved<<','
                  <<local.support_tests<<','<<local.time_nodes<<','
                  <<local.nearest_static_gap<<','
                  <<local.largest_static_distance_disagreement<<'\n';
    }
    must(close(body.time_seconds,.675,1e-10),"900-step clock invalid");
    must(total.certified+total.unresolved==
         accepted_leaves*body.surface_faces.size(),
         "independent auditor omitted a physical face interval");
    must(min_j>1e-6&&total.nearest_static_gap>0,
         "independent geometry or tetrahedral physical safety failed");
    // The FAST physical event used to falsify Pass-7 clock charging.
    Body fast_body=toroidal_solid();
    RigidSphere fast_sphere{};
    fast_sphere.radius+=audit_surface_sphere(
          fast_body,fast_sphere).minimum_surface_clearance_m-.015;
    for(auto& node:fast_body.nodes)node.v={0,0,-50};
    Body fast_replay=fast_body;
    std::vector<AcceptedPhysicalSubstep> fast_receipts;
    const auto fast_result=step_time_consistent_whole_face_sphere(
        fast_body,fast_sphere,.00075,{},&fast_receipts);
    must(fast_result.rejected_clipped_trials>0&&
         fast_receipts.size()>1,
         "fast physical event lacks meaningful time subdivision");
    AuditedPath total_fast;
    for(std::size_t k=0;k<fast_receipts.size();k++){
        Body next=fast_replay;
        const auto r=step_whole_face_sphere(
            next,fast_sphere,fast_receipts[k].dt_s);
        must(!r.time_clipped,"fast accepted physical leaf clipped");
        const auto audit=audit_native_leaf(
            fast_replay,next,fast_sphere,15,
            face_csv.is_open()?&face_csv:nullptr,k);
        total_fast.certified+=audit.certified;
        total_fast.unresolved+=audit.unresolved;
        total_fast.support_tests+=audit.support_tests;
        total_fast.time_nodes+=audit.time_nodes;
        total_fast.nearest_static_gap=std::min(
            total_fast.nearest_static_gap,audit.nearest_static_gap);
        total_fast.largest_static_distance_disagreement=std::max(
            total_fast.largest_static_distance_disagreement,
            audit.largest_static_distance_disagreement);
        if(fast.is_open())
            fast<<k<<','<<next.time_seconds<<','
                <<fast_receipts[k].dt_s<<','<<audit.certified<<','
                <<audit.unresolved<<','<<audit.support_tests<<','
                <<audit.time_nodes<<','<<audit.nearest_static_gap<<','
                <<audit.largest_static_distance_disagreement<<'\n';
        fast_replay=std::move(next);
    }
    compare_state(fast_replay,fast_body);
    must(total_fast.certified+total_fast.unresolved==
         fast_receipts.size()*fast_body.surface_faces.size(),
         "fast sweep auditor omitted connected face");
    if(argc==2){
        std::ofstream manifest(std::filesystem::path(argv[1])/
                               "independent_manifest.json");
        manifest<<std::setprecision(17);
        manifest<<"{\n"
                <<" \"source\":\"independent_six_endpoint_convex_hull_and_Voronoi_CCD_audit\",\n"
                <<" \"independent_of_midpoint_lipschitz_narrowphase\":true,\n"
                <<" \"machine_directed_rounding_proven\":false,\n"
                <<" \"undecided_means_collision\":false,\n"
                <<" \"ordinary_physical_macro_steps\":900,\n"
                <<" \"ordinary_accepted_substeps\":"<<accepted_leaves<<",\n"
                <<" \"ordinary_face_intervals\":"<<(total.certified+total.unresolved)<<",\n"
                <<" \"ordinary_certified_intervals\":"<<total.certified<<",\n"
                <<" \"ordinary_unresolved_intervals\":"<<total.unresolved<<",\n"
                <<" \"ordinary_support_plane_evaluations\":"<<total.support_tests<<",\n"
                <<" \"ordinary_interval_nodes\":"<<total.time_nodes<<",\n"
                <<" \"ordinary_min_static_clearance_m\":"<<total.nearest_static_gap<<",\n"
                <<" \"ordinary_max_static_oracle_distance_disagreement_m\":"<<total.largest_static_distance_disagreement<<",\n"
                <<" \"ordinary_min_tetra_jacobian\":"<<min_j<<",\n"
                <<" \"fast_accepted_physical_substeps\":"<<fast_receipts.size()<<",\n"
                <<" \"fast_face_intervals\":"<<(total_fast.certified+total_fast.unresolved)<<",\n"
                <<" \"fast_certified_intervals\":"<<total_fast.certified<<",\n"
                <<" \"fast_unresolved_intervals\":"<<total_fast.unresolved<<",\n"
                <<" \"fast_min_static_clearance_m\":"<<total_fast.nearest_static_gap<<",\n"
                <<" \"fast_max_static_oracle_distance_disagreement_m\":"<<total_fast.largest_static_distance_disagreement<<",\n"
                <<" \"randomized_separated_triangle_cases\":1000,\n"
                <<" \"max_random_static_distance_disagreement_m\":"<<largest_error<<"\n}\n";
    }
    std::cout<<"PASS: independent 3D six-endpoint swept convex-hull auditor"
             <<" ordinary_certified="<<total.certified
             <<" ordinary_unresolved="<<total.unresolved
             <<" fast_certified="<<total_fast.certified
             <<" fast_unresolved="<<total_fast.unresolved
             <<" max_static_disagreement="<<total.largest_static_distance_disagreement
             <<" all_accepted_static_states_separated=true"
             <<" binary64_interval_not_formally_certified=true\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}

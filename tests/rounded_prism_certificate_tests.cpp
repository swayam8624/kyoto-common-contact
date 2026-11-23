// Pass 10: outward IEEE interval swept-geometry falsification and
// native 3D FEM source audit. This does NOT modify the production solver.
//
// An ordinary rounded-dot-product plane is not enough for a claim of
// machine-sound geometric separation. We verify outward bounded
// endpoint projections and radius*norm using interval arithmetic.
// Time-subdivision vertices are intervals computed from ORIGINAL
// endpoints at exact dyadic time fractions, not rounded midpoints.
//
// This is an explicit IEC559/FE_TONEAREST machine-model validation,
// not an independent formal proof of compiler/hardware conformance.

#include "kyoto/rounded_prism_certificate.hpp"
#include "kyoto/time_consistent_contact.hpp"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace kyoto::solid;
using namespace kyoto::solid::rounded;

static void check(bool yes,const char* msg){
    if(!yes)throw std::runtime_error(msg);
}
struct Counters {
    std::size_t leaves{},certified{},unresolved{},time_nodes{},
                planes{},interval_dots{};
    double min_margin_m{std::numeric_limits<double>::infinity()};
    unsigned max_depth{};
    void add(const CertifiedPrism& p){
        leaves++;
        certified+=p.fully_certified;
        unresolved+=!p.fully_certified;
        time_nodes+=p.tested_time_nodes;
        planes+=p.proposed_planes;
        interval_dots+=p.interval_dot_products;
        max_depth=std::max(max_depth,p.max_depth_reached);
        if(p.fully_certified)min_margin_m=std::min(
            min_margin_m,p.minimum_verified_margin_m);
    }
};
static void audit_leaf(const Body& before,const Body& after,
                       const RigidSphere& sphere,Counters& global,
                       std::ostream* per_face=nullptr,
                       std::size_t leaf=0){
    check(before.surface_faces.size()==after.surface_faces.size(),
          "rounded geometry audit topology mismatch");
    for(std::size_t i=0;i<before.surface_faces.size();i++){
        const auto& face=before.surface_faces[i];
        LinearTriangleMotion motion;
        for(int k=0;k<3;k++){
            motion.start[k]=before.nodes[face[k]].x;
            motion.end[k]=after.nodes[face[k]].x;
        }
        const auto r=certify_rounded_prism(motion,sphere,12);
        global.add(r);
        if(per_face){
            (*per_face)<<leaf<<','<<i<<','<<int(r.fully_certified)
                <<','<<r.tested_time_nodes<<','<<r.proposed_planes
                <<','<<r.interval_dot_products<<','
                <<(r.fully_certified?r.minimum_verified_margin_m:0.)
                <<'\n';
        }
    }
}
int main(int argc,char**argv) {
 try{
    if(argc>2)throw std::invalid_argument(
        "usage: kyoto_rounded_prism_tests [optional evidence directory]");
    check(prerequisites(),
          "IEC559 binary64/round-to-nearest prerequisite did not hold");
    {
        // The original numerical candidate search can NEVER authorize
        // an unsafe transition: a full triangle tunnels through a sphere
        // even though its endpoints and vertices are outside.
        const RigidSphere sphere{{0,0,0},1,0,.2};
        LinearTriangleMotion tunnel{
            {V3{-1,-1,2},V3{1,-1,2},V3{0,2,2}},
            {V3{-1,-1,-2},V3{1,-1,-2},V3{0,2,-2}}};
        const auto tr=certify_rounded_prism(tunnel,sphere,8);
        check(!tr.fully_certified && tr.unresolved_time_nodes>0,
              "outward geometry falsely certified swept tunnelling");
        LinearTriangleMotion tangent{
            {V3{-2,-1,1},V3{2,-1,1},V3{0,2,1}},
            {V3{-2,-1,1},V3{2,-1,1},V3{0,2,1}}};
        check(!certify_rounded_prism(tangent,sphere,4).fully_certified,
              "outward interval incorrectly certified real tangency");
        // A separation of one ulp may be too small to prove.
        // Unresolved is acceptable; incorrect "clear" is not.
        for(auto& p:tangent.start)
            p.z=std::nextafter(1.,2.);
        tangent.end=tangent.start;
        const auto near=certify_rounded_prism(tangent,sphere,2);
        if(near.fully_certified)
            check(near.minimum_verified_margin_m>0,
                  "strict near-grazing bound nonpositive");
        LinearTriangleMotion distant{
            {V3{-1,-1,2},V3{1,-1,2},V3{0,2,2}},
            {V3{-1,-1,2},V3{1,-1,2},V3{0,2,2}}};
        check(certify_rounded_prism(distant,sphere,0).fully_certified,
              "clear rigid-sphere fixture unresolved");
        const int original=std::fegetround();
        check(original==FE_TONEAREST,"unexpected initial FP rounding");
        check(std::fesetround(FE_UPWARD)==0,"rounding-mode test failed");
        bool rejected=false;
        try{(void)certify_rounded_prism(distant,sphere,0);}
        catch(const std::runtime_error&){rejected=true;}
        check(std::fesetround(original)==0,"failed to restore FP mode");
        check(rejected,"unsafe non-default rounding mode was accepted");
    }
    {
        // Independent arithmetic enclosure identities, including
        // subtractive cancellation and mixed-sign interval product.
        const auto a=minus(exact(1.),exact(
            std::nextafter(1.,2.)));
        check(a.lo<0 && a.hi<0,
              "outward subtract lost a tiny negative difference");
        const auto m=product({-2.,3.},{-4.,5.});
        check(m.lo<=-12&&m.hi>=15,
              "outward product does not enclose cross-extrema");
        const auto s=root({2.,2.});
        check(s.lo*s.lo<=2.000000000000002 &&
              s.hi*s.hi>=1.999999999999999,
              "sqrt interval failed to enclose positive root");
    }
    std::mt19937_64 rng(2026112301);
    std::uniform_real_distribution<double> xy(-1.4,1.4),
        z(2.2,4.0),shift(-.12,.12);
    std::size_t random_certified=0;
    const RigidSphere sphere{{0,0,0},1,0,.2};
    for(int sample=0;sample<500;sample++){
        LinearTriangleMotion motion;
        for(int v=0;v<3;v++){
            motion.start[v]={xy(rng),xy(rng),z(rng)};
            motion.end[v]=motion.start[v]+V3{
                shift(rng),shift(rng),shift(rng)};
        }
        const auto r=certify_rounded_prism(motion,sphere,4);
        check(r.fully_certified && r.minimum_verified_margin_m>0,
              "obviously separated randomized swept triangle unresolved");
        random_certified++;
    }
    Counters routine,fast;
    std::ofstream routine_csv,fast_csv,face_csv;
    if(argc==2){
        std::filesystem::create_directories(argv[1]);
        const std::filesystem::path dir=argv[1];
        routine_csv.open(dir/"rounded_routine_900.csv");
        fast_csv.open(dir/"rounded_fast_substeps.csv");
        face_csv.open(dir/"rounded_fast_1600_faces.csv");
        routine_csv<<std::setprecision(17);
        fast_csv<<std::setprecision(17);
        face_csv<<std::setprecision(17);
        routine_csv<<"macro_step,time_s,cumulative_triangle_intervals,"
                     "cumulative_strictly_certified,"
                     "cumulative_unresolved,proposed_planes,"
                     "outward_endpoint_dot_products,min_strict_margin_m\n";
        fast_csv<<"leaf_id,time_s,dt_s,cumulative_face_intervals,"
                  "cumulative_strictly_certified,cumulative_unresolved,"
                  "proposed_planes,outward_endpoint_dot_products,"
                  "min_strict_margin_m\n";
        face_csv<<"leaf_id,face_id,strictly_certified,"
                  "time_nodes,proposed_planes,outward_endpoint_dot_products,"
                  "strict_lower_margin_m\n";
    }
    Body routine_body=toroidal_solid();
    const RigidSphere baseline{};
    for(int step=1;step<=900;step++){
        Body before=routine_body;
        std::vector<AcceptedPhysicalSubstep> receipts;
        const auto result=step_time_consistent_whole_face_sphere(
            routine_body,baseline,.00075,{},&receipts);
        check(result.accepted_substeps==receipts.size(),
              "ordinary physical leaf count mismatch");
        Body replay=std::move(before);
        for(const auto& leaf:receipts){
            Body next=replay;
            const auto candidate=step_whole_face_sphere(
                next,baseline,leaf.dt_s);
            check(!candidate.time_clipped,"accepted physical leaf clipped");
            audit_leaf(replay,next,baseline,routine);
            replay=std::move(next);
        }
        check(std::abs(replay.time_seconds-routine_body.time_seconds)<1e-10,
              "replayed physical clock diverged");
        if(routine_csv.is_open())
            routine_csv<<step<<','<<routine_body.time_seconds<<','
                       <<routine.leaves<<','<<routine.certified<<','
                       <<routine.unresolved<<','<<routine.planes<<','
                       <<routine.interval_dots<<','
                       <<(routine.certified?routine.min_margin_m:0.)<<'\n';
    }
    Body fast_body=toroidal_solid();
    RigidSphere tuned{};
    tuned.radius+=audit_surface_sphere(fast_body,tuned)
                      .minimum_surface_clearance_m-.015;
    for(auto& node:fast_body.nodes)node.v={0,0,-50};
    Body replay=fast_body;
    std::vector<AcceptedPhysicalSubstep> fast_receipts;
    const auto result=step_time_consistent_whole_face_sphere(
        fast_body,tuned,.00075,{},&fast_receipts);
    check(result.rejected_clipped_trials>0&&fast_receipts.size()>1,
          "fast tuning did not force physical subdivision");
    for(std::size_t i=0;i<fast_receipts.size();i++){
        Body next=replay;
        const auto candidate=step_whole_face_sphere(
            next,tuned,fast_receipts[i].dt_s);
        check(!candidate.time_clipped,"fast accepted physical leaf clipped");
        audit_leaf(replay,next,tuned,fast,
                   face_csv.is_open()?&face_csv:nullptr,i);
        if(fast_csv.is_open())
            fast_csv<<i<<','<<next.time_seconds<<','
                    <<fast_receipts[i].dt_s<<','
                    <<fast.leaves<<','<<fast.certified<<','
                    <<fast.unresolved<<','<<fast.planes<<','
                    <<fast.interval_dots<<','
                    <<(fast.certified?fast.min_margin_m:0.)<<'\n';
        replay=std::move(next);
    }
    check(routine.leaves==900*320,
          "not all original routine moving triangles evaluated");
    check(fast.leaves==fast_receipts.size()*320,
          "not all original fast-impact moving triangles evaluated");
    check(std::abs(fast_body.time_seconds-replay.time_seconds)<1e-10,
          "fast physical replay time divergent");
    if(argc==2){
        std::ofstream m(std::filesystem::path(argv[1])/
                        "rounded_manifest.json");
        m<<std::setprecision(17)
         <<"{\n"
         <<" \"scope\":\"swept_triangle_vs_analytic_sphere_only\",\n"
         <<" \"ieee754_outward_intervals_used\":true,\n"
         <<" \"assumes_correctly_rounded_ops_ftz_disabled\":true,\n"
         <<" \"formal_hardware_software_proof\":false,\n"
         <<" \"general_mesh_mesh_CCD\":false,\n"
         <<" \"ordinary_macro_steps\":900,\n"
         <<" \"ordinary_face_intervals\":"<<routine.leaves<<",\n"
         <<" \"ordinary_strictly_proven\":"<<routine.certified<<",\n"
         <<" \"ordinary_unresolved\":"<<routine.unresolved<<",\n"
         <<" \"ordinary_plane_proposals\":"<<routine.planes<<",\n"
         <<" \"ordinary_outward_dot_checks\":"<<routine.interval_dots<<",\n"
         <<" \"ordinary_min_verified_margin_m\":"
            <<(routine.certified?routine.min_margin_m:0.)<<",\n"
         <<" \"fast_physical_substeps\":"<<fast_receipts.size()<<",\n"
         <<" \"fast_face_intervals\":"<<fast.leaves<<",\n"
         <<" \"fast_strictly_proven\":"<<fast.certified<<",\n"
         <<" \"fast_unresolved\":"<<fast.unresolved<<",\n"
         <<" \"fast_plane_proposals\":"<<fast.planes<<",\n"
         <<" \"fast_outward_dot_checks\":"<<fast.interval_dots<<",\n"
         <<" \"fast_min_verified_margin_m\":"
            <<(fast.certified?fast.min_margin_m:0.)<<",\n"
         <<" \"random_separated_cases\":"<<random_certified<<"\n"
         <<"}\n";
    }
    std::cout<<"PASS: outward interval IEEE sphere witness auditor "
             <<"routine="<<routine.certified<<"/"<<routine.leaves
             <<" unresolved="<<routine.unresolved
             <<" fast="<<fast.certified<<"/"<<fast.leaves
             <<" unresolved="<<fast.unresolved
             <<" checked_endpoint_dot_products="
             <<routine.interval_dots+fast.interval_dots
             <<" (machine-model assumptions, not formal hardware proof)\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}

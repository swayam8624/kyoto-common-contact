#include "kyoto/time_consistent_contact.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kyoto::solid;
static void must(bool condition,const char* msg){
    if(!condition)throw std::runtime_error(msg);
}
static bool near(double a,double b,double eps=1e-11){
    return std::abs(a-b)<=eps;
}
static void same_body(const Body& a,const Body& b,double eps){
    must(a.nodes.size()==b.nodes.size(),"body topology diverged");
    must(near(a.time_seconds,b.time_seconds,eps),"body time diverged");
    for(std::size_t i=0;i<a.nodes.size();i++){
        must(norm(a.nodes[i].x-b.nodes[i].x)<=eps,
             "body positions diverged");
        must(norm(a.nodes[i].v-b.nodes[i].v)<=eps,
             "body velocities diverged");
    }
}
int main(){
 try{
    // Exact agreement with the old full-step integrator when there is
    // no contact; no spurious numerical changes or solver overhead.
    {
        Body reference=toroidal_solid(),adaptive=reference;
        const RigidSphere distant{{0,0,-200},.7,0,.2};
        for(int step=0;step<40;step++){
            const auto a=step_whole_face_sphere(reference,distant,.00075);
            const auto b=step_time_consistent_whole_face_sphere(
                adaptive,distant,.00075);
            must(!a.time_clipped && b.accepted_substeps==1 &&
                 b.rejected_clipped_trials==0,"free flight unnecessarily split");
            must(near(b.actually_integrated_dt_s,.00075),
                 "free flight lost physical time");
            same_body(reference,adaptive,1e-12);
        }
        std::cout<<"TIME_CONSISTENT_FREE_FLIGHT_IDENTICAL_PASS\n";
    }
    // Normal 900-step deformable impact, with independent full-triangle
    // clearance and finite stress/energy audit at every accepted state.
    {
        Body body=toroidal_solid();
        const RigidSphere sphere{};
        const auto start=body.diagnostics(-100);
        std::size_t accepted=0,splits=0,contacts=0;
        double min_gap=std::numeric_limits<double>::infinity();
        for(int step=0;step<900;step++){
            const auto m=step_time_consistent_whole_face_sphere(
                body,sphere,.00075);
            const auto audit=audit_surface_sphere(body,sphere);
            const auto fem=body.diagnostics(-100);
            must(audit.penetrating_faces==0 && audit.inside_vertex_count==0,
                 "adaptive 3D FEM accepted triangle penetration");
            must(m.minimum_tetra_jacobian>1e-6,
                 "adaptive FEM inverted tetrahedron");
            must(near(m.actually_integrated_dt_s,.00075,1e-12),
                 "adaptive FEM lost physical time");
            must(std::isfinite(fem.elastic_joule)&&
                 std::isfinite(fem.kinetic_joule),"nonfinite physical energy");
            min_gap=std::min(min_gap,audit.minimum_surface_clearance_m);
            accepted+=m.accepted_substeps;
            splits+=m.rejected_clipped_trials;
            contacts+=m.accepted_face_impulse_updates;
        }
        const auto after=body.diagnostics(-100);
        must(near(body.time_seconds,.675,1e-10),
             "900-step integrated clock not complete");
        must(near(start.total_mass_kg,after.total_mass_kg,1e-8),
             "lumped mesh mass changed");
        must(contacts>0 && after.max_displacement>.1,
             "no actual elastic impact");
        std::cout<<"TIME_CONSISTENT_900_PASS substeps="<<accepted
                 <<" splits="<<splits<<" contact_updates="<<contacts
                 <<" min_triangle_gap_m="<<min_gap
                 <<" elapsed_s="<<body.time_seconds<<'\n';
    }
    // The control actually clips a FAST incoming whole 3D mesh, but
    // new method must accept only fully integrated safe substeps.
    {
        Body seed=toroidal_solid();
        for(auto& node:seed.nodes)node.v={0,0,-900};
        Body legacy=seed;
        const RigidSphere sphere{};
        const auto old=step_whole_face_sphere(legacy,sphere,.00075);
        must(old.time_clipped&&old.accepted_global_fraction<1,
             "fast fixture did not activate original unsafe clock shortcut");
        std::vector<AcceptedPhysicalSubstep> receipts;
        const auto m=step_time_consistent_whole_face_sphere(
            seed,sphere,.00075,{},&receipts);
        must(m.rejected_clipped_trials>0 && m.accepted_substeps>1,
             "fast adaptive step did not replay actual remaining time");
        must(receipts.size()==m.accepted_substeps,
             "missing physical accepted substep receipts");
        must(near(m.actually_integrated_dt_s,.00075,1e-12) &&
             near(seed.time_seconds,.00075,1e-12),
             "adaptive fast impact did not integrate full physical time");
        double elapsed=0;
        for(const auto& r:receipts){
            must(r.dt_s>0 && r.minimum_tetra_jacobian>1e-6,
                 "invalid accepted physical substep");
            must(r.minimum_full_triangle_clearance_m>=0,
                 "accepted substep left full-face penetration");
            must(near(r.start_time_s,elapsed,1e-11),
                 "physical substeps are not contiguous");
            elapsed+=r.dt_s;
            must(near(r.end_time_s,elapsed,1e-11),
                 "physical receipt wrong time");
        }
        must(near(elapsed,.00075,1e-12),
             "accepted physical substeps have missing time");
        const auto audit=audit_surface_sphere(seed,sphere);
        must(audit.penetrating_faces==0,
             "fast adaptive solver intersected curved rigid obstacle");
        std::cout<<"TIME_CONSISTENT_ADVERSARIAL_PASS splits="
                 <<m.rejected_clipped_trials
                 <<" accepted="<<m.accepted_substeps
                 <<" deepest="<<m.deepest_bisection
                 <<" min_accepted_dt="<<m.smallest_accepted_dt_s
                 <<" clearance="<<audit.minimum_surface_clearance_m<<'\n';
    }
    // An impossible temporal budget must fail ATOMICALLY; neither
    // physical state nor preexisting receipts may change.
    {
        Body b=toroidal_solid();
        for(auto& n:b.nodes)n.v={0,0,-900};
        const Body original=b;
        TimeConsistentSettings policy;
        policy.maximum_bisection_depth=0;
        std::vector<AcceptedPhysicalSubstep> receipts{{-1,.1,.1}};
        bool failed=false;
        try{
            (void)step_time_consistent_whole_face_sphere(
                b,RigidSphere{},.00075,policy,&receipts);
        }catch(const std::runtime_error&){failed=true;}
        must(failed,"insufficient temporal budget silently accepted");
        same_body(b,original,0);
        must(receipts.size()==1&&receipts[0].start_time_s==-1,
             "failed macro-step mutated receipt output");
        std::cout<<"TIME_CONSISTENT_ROLLBACK_PASS\n";
    }
    std::cout<<"PASS: actual elapsed-time adaptive 3D FEM, 900-step impact, "
                "fast geometric tunnelling and all-or-nothing rollback\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}

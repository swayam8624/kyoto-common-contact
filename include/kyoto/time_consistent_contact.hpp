#pragma once
// Kyoto Pass 8: time-consistent adaptive replay of the Pass-7 whole-face FEM.
//
// CRITICAL DIFFERENCE FROM Pass 7:
// A candidate with full-face certified fraction alpha<1 is NOT COMMITTED.
// We bisect its proposed physical dt and recompute the full elastodynamic
// trial (fresh stresses, accelerations, velocities, impulses) for EACH
// smaller substep. Only candidates with alpha==1 are accepted.
// The requested total physical dt is the SUM of accepted actual substeps.
// This removes the old "truncate displacement but charge full dt" defect.
//
// This is event-aware adaptive explicit integration, NOT yet a continuous
// TOI complementarity solve: contacts use a finite activation shell,
// sequential barycentric impulses, and no self-contact. The underlying
// Lipschitz proof is exact-arithmetic; binary64 is not formally certified.
// A tiny unrecoverable step fails loudly, without altering the input Body.
//
// The entire macro-step is transactional. Failed subdivisions and rejected
// full-step experiments never affect the public state or impulse accounting.

#include "kyoto/whole_face_contact_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace kyoto::solid {

struct TimeConsistentSettings {
    WholeFaceContactSettings contact{};
    unsigned maximum_bisection_depth{18};
    std::size_t maximum_accepted_substeps{32768};
    double minimum_substep_seconds{1e-12};
};
struct AcceptedPhysicalSubstep {
    double start_time_s{};
    double dt_s{};
    double end_time_s{};
    double minimum_full_triangle_clearance_m{};
    double minimum_tetra_jacobian{};
    double normal_impulse_ns{};
    double tangential_impulse_ns{};
    std::size_t face_interval_tests{};
    std::size_t face_contact_updates{};
    unsigned bisection_depth{};
    // True total external vector contact impulse from this accepted leaf.
    V3 net_contact_impulse_vector{};
    V3 net_contact_torque_at_application{};
};
struct TimeConsistentMetrics {
    double requested_dt_s{};
    double actually_integrated_dt_s{};
    double start_time_s{};
    double end_time_s{};
    double smallest_accepted_dt_s{std::numeric_limits<double>::infinity()};
    double largest_accepted_dt_s{};
    double minimum_triangle_clearance_m{std::numeric_limits<double>::infinity()};
    double minimum_tetra_jacobian{std::numeric_limits<double>::infinity()};
    double normal_impulse_ns{};
    double tangential_impulse_ns{};
    std::size_t rejected_clipped_trials{};
    std::size_t rejected_inverted_trials{};
    std::size_t accepted_substeps{};
    std::size_t face_interval_tests{};
    std::size_t accepted_face_impulse_updates{};
    unsigned deepest_bisection{};
    V3 net_contact_impulse_vector{};
    V3 net_contact_torque_at_application{};
};

inline void validate(const TimeConsistentSettings& c) {
    validate(c.contact);
    if(c.maximum_bisection_depth>24||
       c.maximum_accepted_substeps==0||
       !(std::isfinite(c.minimum_substep_seconds)&&
         c.minimum_substep_seconds>0))
        throw std::invalid_argument("invalid time-consistent adaptive settings");
}

inline TimeConsistentMetrics step_time_consistent_whole_face_sphere(
    Body& body,const RigidSphere& sphere,double dt,
    const TimeConsistentSettings& settings={},
    std::vector<AcceptedPhysicalSubstep>* accepted_receipts=nullptr) {
    validate(sphere);validate(settings);
    if(!(std::isfinite(dt)&&dt>0)||
       dt<settings.minimum_substep_seconds)
        throw std::invalid_argument("invalid requested physical timestep");

    const double start_clock=body.time_seconds;
    if(!std::isfinite(start_clock)||!std::isfinite(start_clock+dt)||
       start_clock+dt<=start_clock)
        throw std::invalid_argument("physical clock cannot advance accurately");

    // Work entirely on a private snapshot until the WHOLE requested
    // physical timestep has been consumed. No partial result escapes.
    Body staged=body;
    TimeConsistentMetrics out;
    out.requested_dt_s=dt;
    out.start_time_s=start_clock;
    std::vector<double> accumulated_impulses(body.nodes.size(),0);
    std::vector<AcceptedPhysicalSubstep> local_receipts;
    const auto subdivide=[&](auto&& self,double proposed_dt,
                             unsigned depth)->void {
        if(out.accepted_substeps>=settings.maximum_accepted_substeps)
            throw std::runtime_error("adaptive FEM exceeded accepted substep budget");
        if(proposed_dt<settings.minimum_substep_seconds)
            throw std::runtime_error("adaptive FEM cannot certify minimum physical substep");

        Body proposal=staged;
        WholeFaceStepMetrics trial;
        bool invalid_trial_tetra=false;
        try {
            trial=step_whole_face_sphere(
                proposal,sphere,proposed_dt,settings.contact);
        } catch(const std::runtime_error& e) {
            // A trial that inverts a tet is NEVER an accepted state.
            // Only this specifically recognized step-size instability
            // can trigger retry. Input errors, unexpected NaNs and
            // other failed scientific invariants remain fatal.
            if(std::string_view(e.what())==
               "whole-face trial inverts a tetrahedron") {
                invalid_trial_tetra=true;
            } else {
                throw;
            }
        }
        out.face_interval_tests+=trial.face_interval_tests;

        if(invalid_trial_tetra || trial.time_clipped) {
            if(invalid_trial_tetra)++out.rejected_inverted_trials;
            else ++out.rejected_clipped_trials;
            out.deepest_bisection=std::max(out.deepest_bisection,depth+1);
            // Never adopt the motion-clipped proposal, its impulses,
            // or its unjustified full-dt clock advance. Instead, halve
            // the ACTUAL time and replay both halves from live states.
            if(depth>=settings.maximum_bisection_depth||
               proposed_dt*.5<settings.minimum_substep_seconds)
                throw std::runtime_error(
                  "adaptive FEM cannot resolve whole-face event or "
                  "tet inversion within configured physical-time budget");
            const double half=proposed_dt*.5;
            self(self,half,depth+1);
            self(self,proposed_dt-half,depth+1);
            return;
        }

        // An accepted candidate has evaluated and certified the
        // complete actual substep, not only a short prefix.
        if(!(trial.accepted_global_fraction==1.0)||
           proposal.time_seconds<=staged.time_seconds)
            throw std::runtime_error("adaptive FEM accepted truncated time");
        const auto state=audit_surface_sphere(proposal,sphere);
        const auto fem=proposal.evaluate();
        if(state.penetrating_faces!=0 ||
           state.minimum_surface_clearance_m<
                settings.contact.clearance_skin_m*.5 ||
           !(std::isfinite(fem.min_jacobian)&&fem.min_jacobian>1e-6))
            throw std::runtime_error(
                "adaptive FEM full-interval acceptance fails independent audit");

        local_receipts.push_back({
           staged.time_seconds,proposed_dt,proposal.time_seconds,
           state.minimum_surface_clearance_m,fem.min_jacobian,
           trial.total_normal_impulse_ns,
           trial.total_tangential_impulse_ns,
           trial.face_interval_tests,trial.normal_impulses,depth,
           trial.net_contact_impulse_vector,
           trial.net_contact_torque_at_application});
        for(std::size_t i=0;i<accumulated_impulses.size();i++)
            accumulated_impulses[i]+=proposal.nodes[i].last_normal_impulse;

        out.actually_integrated_dt_s+=proposed_dt;
        out.accepted_substeps++;
        out.smallest_accepted_dt_s=
            std::min(out.smallest_accepted_dt_s,proposed_dt);
        out.largest_accepted_dt_s=
            std::max(out.largest_accepted_dt_s,proposed_dt);
        out.minimum_triangle_clearance_m=std::min(
            out.minimum_triangle_clearance_m,state.minimum_surface_clearance_m);
        out.minimum_tetra_jacobian=std::min(
            out.minimum_tetra_jacobian,fem.min_jacobian);
        out.net_contact_impulse_vector+=trial.net_contact_impulse_vector;
        out.net_contact_torque_at_application+=
            trial.net_contact_torque_at_application;
        out.normal_impulse_ns+=trial.total_normal_impulse_ns;
        out.tangential_impulse_ns+=trial.total_tangential_impulse_ns;
        out.accepted_face_impulse_updates+=trial.normal_impulses;
        staged=std::move(proposal);
    };

    subdivide(subdivide,dt,0);
    const double tol=2e-12*std::max(1.0,dt);
    if(std::abs(out.actually_integrated_dt_s-dt)>tol||
       std::abs(staged.time_seconds-(start_clock+dt))>tol)
        throw std::runtime_error("adaptive FEM lost physical elapsed time");

    // Set exact requested endpoint only AFTER every physical substep
    // was actually integrated. This is not charging unintegrated time.
    staged.time_seconds=start_clock+dt;
    staged.contact_count_step=out.accepted_face_impulse_updates;
    staged.normal_impulse_step=out.normal_impulse_ns;
    for(std::size_t i=0;i<staged.nodes.size();i++)
        staged.nodes[i].last_normal_impulse=accumulated_impulses[i];
    out.end_time_s=staged.time_seconds;

    body=std::move(staged);
    if(accepted_receipts) {
        // Receipts are appended only after the entire macro-step
        // succeeds. The public state and audit remain transactional.
        accepted_receipts->insert(accepted_receipts->end(),
                                 local_receipts.begin(),local_receipts.end());
    }
    return out;
}
} // namespace kyoto::solid

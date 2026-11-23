#pragma once
// Experimental online scheduling of TWO EXACT candidate-generation methods.
// This is not RL and is not a proof of a novel collision algorithm.
//
// Both arms use the same conservative swept 3D features and same official
// category/mask/incident-geometry contract. The policy never drops a pair:
// it selects between fully validated refit BVH and bounded VF/EE frontier.
//
// Every call times *the entire selected query* including preprocessing,
// hierarchical updates and candidate normalization; model lookup, policy
// selection and accounting live inside the caller's external timer.
// Periodic alternative probes and a hysteresis margin limit stale
// decisions and switch thrashing; probes remain real charged queries.
// A previously unselected arm may be stale, but next query rebuilds
// all moving boxes and safely rechecks its entire stored frontier.
#include "kyoto/refit_feature_hierarchy.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace kyoto::contact::atlas {
class AdaptiveCandidatePolicy {
    RefitHierarchy plain_{true,false};
    RefitHierarchy compacted_{true,true,true};
    std::uint64_t frames_{},probes_{},switches_{};
    bool last_frontier_{};
    bool has_plain_{},has_frontier_{};
    double plain_ns_{},frontier_ns_{};

public:
    std::uint64_t frames()const{return frames_;}
    std::uint64_t probes()const{return probes_;}
    std::uint64_t switches()const{return switches_;}
    bool last_used_frontier()const{return last_frontier_;}
    double plain_ema_ns()const{return plain_ns_;}
    double frontier_ema_ns()const{return frontier_ns_;}

    f::Result query(const f::Mesh& mesh) {
        // Cold calibration: each method is executed on a different source
        // frame and tested by the caller's independent oracle. The first
        // recorded costs include cold build and therefore must NOT be
        // interpreted as steady-state performance.
        bool use_frontier=false;
        bool probe=false;
        if(!has_plain_)use_frontier=false;
        else if(!has_frontier_)use_frontier=true;
        else if(frames_%13==0){
            use_frontier=!last_frontier_;
            probe=true;
        }else{
            // A minimum 3% advantage is needed to abandon the previous
            // choice; otherwise avoid flip-flopping on clock noise.
            const double preferred=last_frontier_?frontier_ns_:plain_ns_;
            const double alternative=last_frontier_?plain_ns_:frontier_ns_;
            use_frontier=(alternative<preferred*.97)
                ?!last_frontier_:last_frontier_;
        }
        using Clock=std::chrono::steady_clock;
        const auto start=Clock::now();
        auto result=use_frontier?compacted_.query(mesh):plain_.query(mesh);
        const auto stop=Clock::now();
        const double elapsed=static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                stop-start).count());
        if(!(elapsed>=0.0))throw std::runtime_error("negative policy clock");
        double& estimate=use_frontier?frontier_ns_:plain_ns_;
        bool& known=use_frontier?has_frontier_:has_plain_;
        estimate=known?.8*estimate+.2*elapsed:elapsed;
        known=true;
        if(frames_>0 && use_frontier!=last_frontier_)++switches_;
        if(probe)++probes_;
        last_frontier_=use_frontier;
        ++frames_;
        return result;
    }
};
} // namespace kyoto::contact::atlas

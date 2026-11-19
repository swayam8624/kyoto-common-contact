#pragma once
// RBC prospective analytic margin model, independently testable.
//
// MODEL, NOT A GENERAL ALGORITHM THEOREM:
//   C(m) = c_build * drift / m + c_filter * density * (extent+2m)^3
//
// Assumes an idealized homogeneous 3D box population, constant average
// step displacement, and reference builds amortized over drift/m steps.
// Real query/solver data must FIT and VALIDATE this law on development
// scenes, then evaluate predictions on UNSEEN scenes. This is not
// a substitute for tuned fixed-margin Guard or a physics benchmark.
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ccp::rbc {
struct MarginModel {
    double rebuild_cost{};
    double filter_cost{};
    double density{};
    double primitive_extent{};
    double mean_step_drift{};
};

inline void validate(const MarginModel& m) {
    const double values[]={
        m.rebuild_cost,m.filter_cost,m.density,
        m.primitive_extent,m.mean_step_drift
    };
    for (double x:values)
        if (!std::isfinite(x))
            throw std::invalid_argument("RBC cost model must have finite parameters");
    if (m.rebuild_cost<=0 || m.filter_cost<=0 ||
        m.density<=0 || m.primitive_extent<0 ||
        m.mean_step_drift<0)
        throw std::invalid_argument(
            "RBC cost model requires positive costs/density and nonnegative extent/drift");
}

// Stable positive root of 2*m^2 + extent*m - sqrt(cb*delta/(6*cf*k))=0.
// The rationalized solution avoids catastrophic cancellation for tiny drift.
//
// If drift is zero, m*=0: no rebuilding is necessary in this MODEL.
// Under actual bounded step motion, m<delta_max cannot carry a
// multi-step guarantee; the caller must separately enforce that
// constraint (a conservative exact fallback remains mandatory).
[[nodiscard]] inline double predicted_unconstrained_margin(
    const MarginModel& m) {
    validate(m);
    if (m.mean_step_drift==0.0) return 0.0;
    const double q=std::sqrt(
        (m.rebuild_cost/(6.0*m.filter_cost*m.density))*
        m.mean_step_drift);
    const double discriminant=std::hypot(
        m.primitive_extent,std::sqrt(8.0*q));
    const double opt=2.0*q/(m.primitive_extent+discriminant);
    if (!std::isfinite(opt)||opt<=0.0)
        throw std::overflow_error(
            "RBC predicted optimal margin not representable");
    return opt;
}

[[nodiscard]] inline double predicted_cost(
    const MarginModel& m,double margin) {
    validate(m);
    if (!std::isfinite(margin)||margin<0.0)
        throw std::invalid_argument(
            "RBC cost evaluation requires finite nonnegative margin");
    if (margin==0.0&&m.mean_step_drift>0.0)
        throw std::invalid_argument(
            "RBC positive-drift model has undefined zero-skin cost");
    const double rebuild=margin==0?0:m.rebuild_cost*m.mean_step_drift/margin;
    const double width=m.primitive_extent+2.0*margin;
    return rebuild+m.filter_cost*m.density*width*width*width;
}

[[nodiscard]] inline double predicted_derivative(
    const MarginModel& m,double margin) {
    validate(m);
    if (!std::isfinite(margin)||margin<=0.0)
        throw std::invalid_argument("RBC derivative requires margin>0");
    const double width=m.primitive_extent+2.0*margin;
    return -m.rebuild_cost*m.mean_step_drift/(margin*margin)+
            6.0*m.filter_cost*m.density*width*width;
}
} // namespace ccp::rbc

#pragma once
// Conservative earliest-possible current-AABB overlap deadlines.
// This is a math/physics utility, NOT a snapshot-replay benchmark method.
// It is valid ONLY if the caller provides guaranteed speed and acceleration
// component bounds over the requested time interval, accounting for *every*
// source of shape and box deformation. Never feed unbounded ML estimates or
// past observed velocity as a "guaranteed" future-motion envelope.
//
// If an axis has gap g>0, relative closing speed <= V and acceleration <= A,
// then the gap cannot close before g <= V*t + A*t*t/2.
// The maximum per-separated-axis deadline is safe because overlap needs
// intersection on ALL three axes. On a deadline, re-certify against new state.
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ccp::experiment {
struct ComponentMotionBound {
 std::array<double,3> speed{};        // Guaranteed max absolute |dx/dt|
 std::array<double,3> acceleration{}; // Guaranteed max absolute |d2x/dt2|
};
inline double scalarSafeDeadline(double gap,double closingSpeed,double closingAccel){
 if(!std::isfinite(gap)||gap<0||!std::isfinite(closingSpeed)||closingSpeed<0||
    !std::isfinite(closingAccel)||closingAccel<0)
  throw std::invalid_argument("motion certificate requires finite nonnegative bounds");
 if(gap==0)return 0.;
 if(closingAccel==0)
  return closingSpeed==0?std::numeric_limits<double>::infinity():gap/closingSpeed;
 // Stable positive root: avoids cancellation at large closing speeds.
 const double disc=std::sqrt(closingSpeed*closingSpeed+2.*closingAccel*gap);
 return disc==0?std::numeric_limits<double>::infinity():
        2.*gap/(closingSpeed+disc);
}
inline double earliestPossibleOverlap(const ccp::live::Bounds& a,
                                     const ccp::live::Bounds& b,
                                     const ComponentMotionBound& motionA,
                                     const ComponentMotionBound& motionB){
 const std::array<double,3> amin={a.min.x,a.min.y,a.min.z};
 const std::array<double,3> amax={a.max.x,a.max.y,a.max.z};
 const std::array<double,3> bmin={b.min.x,b.min.y,b.min.z};
 const std::array<double,3> bmax={b.max.x,b.max.y,b.max.z};
 double deadline=0.;
 for(size_t axis=0;axis<3;++axis){
  if(!std::isfinite(amin[axis])||!std::isfinite(amax[axis])||
     !std::isfinite(bmin[axis])||!std::isfinite(bmax[axis])||
     amin[axis]>amax[axis]||bmin[axis]>bmax[axis])
   throw std::invalid_argument("invalid AABB in motion certificate");
  const double speed=motionA.speed[axis]+motionB.speed[axis];
  const double accel=motionA.acceleration[axis]+motionB.acceleration[axis];
  if(!std::isfinite(speed)||!std::isfinite(accel)||speed<0||accel<0)
   throw std::invalid_argument("invalid relative motion certificate bound");
  // Inclusive AABB overlap: touching already counts as intersecting.
  const double gap=std::max({0.,bmin[axis]-amax[axis],amin[axis]-bmax[axis]});
  deadline=std::max(deadline,scalarSafeDeadline(gap,speed,accel));
 }
 return deadline;
}
} // namespace ccp::experiment

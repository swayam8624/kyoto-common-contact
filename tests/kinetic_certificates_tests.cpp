#include "ccp/kinetic_certificates.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ccp::experiment;
using ccp::live::Bounds;

static bool near(double a,double b){return std::abs(a-b)<1e-11;}
int main(){
 const Bounds a{0,{0.,0.,0.},{1.,1.,1.}};
 const Bounds b{1,{2.,1.,0.},{3.,2.,1.}};
 ComponentMotionBound still{};
 ComponentMotionBound toward{};
 toward.speed[0]=1.;
 assert(near(earliestPossibleOverlap(a,b,toward,still),1.));
 assert(std::isinf(earliestPossibleOverlap(a,b,still,still)));
 ComponentMotionBound accelerating{};
 accelerating.acceleration[0]=2.;
 assert(near(earliestPossibleOverlap(a,b,accelerating,still),1.));
 // Requiring both x and y gaps to close gives the MAX safe deadline:
 const Bounds c{2,{2.,4.,0.},{3.,5.,1.}};
 ComponentMotionBound diagonal{};
 diagonal.speed={1.,1.,0.};
 assert(near(earliestPossibleOverlap(a,c,diagonal,still),3.));
 const Bounds touch{3,{1.,0.,0.},{2.,1.,1.}};
 assert(near(earliestPossibleOverlap(a,touch,still,still),0.));
 bool rejected=false;
 try{
  ComponentMotionBound invalid{};invalid.speed[0]=-1.;
  (void)earliestPossibleOverlap(a,b,invalid,still);
 }catch(const std::invalid_argument&){rejected=true;}
 assert(rejected);
 std::cout<<"PASS: kinetic separation lower bounds (axis gaps, acceleration, "
             "immobility, inclusive boundary and invalid bound rejection)\n";
}

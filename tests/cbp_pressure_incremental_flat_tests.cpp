// Independent brute-force correctness for exact persistent cell ownership.
// Tests no-change, within-cell motion, crossings, mass teleport, global
// overlap and N/owner cardinality changes without an external CSV oracle.
#include "ccp/cbp_pressure_incremental_flat.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace ccp::pressure;
void verify(bool value,const char* message){
 if(!value)throw std::runtime_error(message);
}
std::vector<Box> scene(std::size_t n,int frame){
 std::vector<Box> result(n);
 for(std::size_t i=0;i<n;++i){
  const auto h=mix(std::uint64_t(i)*UINT64_C(374761393)+101);
  float x=float(i%13)*.54f;
  float y=float((i/13)%8)*.68f;
  float z=float(i/104)*.77f;
  const float motion=float(.025*std::sin(frame*.18+double(i%23)));
  if(frame>=12&&frame<30){x+=1.3f;z-=.7f;}
  if(frame>=30&&frame<60){x=0;y=0;z=0;}
  if(frame>=60&&frame<130){
   x+=float(int(h%23)-11)*.22f;
   y+=float(int((h>>8)%17)-8)*.23f;
  }
  if(frame>=130){
   // Rapid movement and changes in full-grid occupancy.
   x+=float(std::sin(frame*.071+i*.31))*2.5f;
   z+=float(std::cos(frame*.043+i*.17))*1.4f;
  }
  x+=motion;
  const float half=frame>=30&&frame<60?.4f:.28f;
  auto& box=result[i];
  for(int axis=0;axis<3;++axis){
   const auto center=axis==0?x:axis==1?y:z;
   box.lo[axis]=center-half;
   box.hi[axis]=center+half;
  }
 }
 return result;
}
void test(){
 FlatPersistentGrid g(100000);
 auto current=scene(192,0);
 auto base=g.advance(current);
 verify(base.rebuilt&&base.changedRanges==192,
        "First current-frame build not charged");
 auto first=g.enumerate(10000000,5000);
 verify(first.complete&&first.digest==brute(current),
        "First full 3D exact candidate set incorrect");

 // Identical immutable input must not rebuild/reallocate cells.
 auto again=g.advance(current);
 verify(!again.rebuilt&&again.changedRanges==0&&again.changedBounds==0,
        "Unchanged scene incorrectly caused dirty repair");
 verify(g.enumerate(10000000,5000).digest==first.digest,
        "Unchanged scene changed pairs");
 // Single object moves WITHIN its occupied cell range, and its pair
 // predicate MUST use new coordinates even with zero cell membership edits.
 current=scene(192,1);
 auto within=g.advance(current);
 auto result=g.enumerate(10000000,5000);
 verify(result.complete&&result.digest==brute(current),
        "Within-cell physical motion made a stale pair decision");
 verify(within.changedBounds>0,"Change-to-current AABBs not registered");

 std::size_t rebuilds=0, repaired=0,unchanged=0;
 for(int frame=2;frame<160;++frame){
  if(frame==90)current=scene(256,frame);
  else current=scene(current.size(),frame);
  const auto stats=g.advance(current);
  const auto observed=g.enumerate(10000000,5000);
  verify(observed.complete,"Small-scene full query unexpectedly capped");
  verify(observed.tests==stats.workUpper,
         "Persistent upper bound omitted cell-local pair tests");
  verify(observed.digest==brute(current),
         "Exact pair count/full 64/32-bit hash mismatch");
  verify(stats.entries==g.entries()&&stats.activeCells==g.cells(),
         "Current occupancy bookkeeping drifted");
  rebuilds+=stats.rebuilt;
  repaired+=stats.changedRanges;
  unchanged+=stats.changedRanges==0;
 }
 verify(rebuilds>0&&repaired>0&&unchanged>0,
        "Regression never tested rebuild, dirty repair and quiet reuse");
 // Fail-closed input validation must NOT mutate prior valid state.
 const auto stable=brute(current);
 auto invalid=current;
 invalid[0].lo[0]=std::numeric_limits<float>::quiet_NaN();
 bool rejected=false;
 try{(void)g.advance(invalid);}catch(const std::exception&){rejected=true;}
 verify(rejected,"Nonfinite geometry not rejected");
 verify(g.enumerate(10000000,5000).digest==stable,
        "Bad input invalidated a correctly cached spatial index");
 // Too-large per-box expansion also fails before any update.
 auto oversized=current;
 oversized[0].hi[0]=500.f;
 rejected=false;
 try{(void)g.advance(oversized);}catch(const std::exception&){rejected=true;}
 verify(rejected,"Unbounded per-primitive spatial ownership not rejected");
 verify(g.enumerate(10000000,5000).digest==stable,
        "Oversized bad input destroyed the previous valid spatial index");

 // Phase 16 multithreaded genuine cell query. Every box overlaps every
 // other box, giving >1,000,000 CELL comparisons while the exact pair set
 // is only 130,816 stable ID pairs. The result must match a full brute
 // oracle, independent of schedule and worker completion order.
 auto dense=std::vector<Box>(512);
 for(auto& b:dense)
  for(int axis=0;axis<3;++axis){
   b.lo[axis]=-.35f;
   b.hi[axis]=.35f;
  }
 g.advance(dense);
 const auto expected=brute(dense);
 auto serial=g.enumerate(10000000,5000);
 verify(serial.complete&&serial.digest==expected,
        "Dense serial canonical-cell digest mismatch");
 for(const std::size_t nThreads:{2u,4u,8u}){
  std::size_t used=0;
  const auto parallel=g.enumerateParallel(10000000,5000,nThreads,&used);
  verify(used>1,"Parallel cells failed to allocate multiple workers");
  verify(parallel.complete&&parallel.digest==expected,
         "Parallel cell-enumeration digest mismatch");
  verify(parallel.tests==serial.tests,
         "Parallel candidate-test accounting does not match serial");
 }
 // A capped query is NEVER an exact completed result, even if the
 // scheduler processes a favorable slice first.
 std::size_t used=0;
 const auto capped=g.enumerateParallel(50000,5000,8,&used);
 verify(!capped.complete,
        "Budget-capped parallel query incorrectly reported complete");
 // Unchanged packed occupancy: independent temporal calls must remain
 // bitwise deterministic for the hash+count digest, not execution order.
 for(int i=0;i<4;++i){
  g.advance(dense);
  const auto next=g.enumerateParallel(10000000,5000,4,&used);
  verify(next.complete&&next.digest==expected,
         "Temporal parallel candidate digest drifted across frames");
 }

 std::cout<<"PASS FLAT persistent exact spatial membership oracle across "
          <<"160 frames incl no-op reuse, within-cell motion, cell migration, "
          <<"global overlap, scene resize and input corruption"
          <<" rebuilds="<<rebuilds<<" dirty="<<repaired
          <<" quiet="<<unchanged<<"\n";
}
}
int main(){try{test();}catch(const std::exception& e){
 std::cerr<<"INCREMENTAL ORACLE FAILURE: "<<e.what()<<"\n";return 1;
}return 0;}

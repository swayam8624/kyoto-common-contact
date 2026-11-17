#include "ccp/online_partitioner.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>
namespace ccp::research {
namespace {
using Clock=std::chrono::steady_clock;
bool overlaps(const ccp::live::Bounds& a,const ccp::live::Bounds& b){
 return a.min.x<=b.max.x&&b.min.x<=a.max.x &&
        a.min.y<=b.max.y&&b.min.y<=a.max.y &&
        a.min.z<=b.max.z&&b.min.z<=a.max.z;
}
bool lessPatch(const OnlinePatch& a,const OnlinePatch& b){
 if(a.sheet!=b.sheet)return a.sheet<b.sheet;
 if(a.y!=b.y)return a.y<b.y;
 if(a.x!=b.x)return a.x<b.x;
 return a.span>b.span;
}
}
int OnlinePartitioner::stableId(const OnlinePatch& p){
 if(p.sheet<0||p.sheet>=3||p.x<0||p.x>=48||p.y<0||p.y>=48 ||
    (p.span!=3&&p.span!=6&&p.span!=12&&p.span!=24&&p.span!=48))
  throw std::invalid_argument("invalid online patch identity");
 // Separate ID namespace for all levels and all cloth sheets, disjoint
 // from primitive IDs and from moving-sphere region IDs.
 // Preserve the legacy 12-cell identifiers for its original patch
 // levels; larger domains receive a disjoint ID namespace, encoding the span
 // as well as sheet and cell coordinates to avoid cross-level collisions.
 if(p.x<12&&p.y<12&&(p.span==3||p.span==6||p.span==12)){
  const int level=p.span==12?0:p.span==6?1:2;
  return 5000+p.sheet*1000+level*200+p.y*12+p.x;
 }
 return 60000+p.sheet*160000+p.span*2500+p.y*48+p.x;
}
void OnlinePartitioner::reset(int sheets,int cellsPerSide){
 if(sheets<1||sheets>3)throw std::invalid_argument("online partition sheets must be 1..3");
 if(cellsPerSide!=12&&cellsPerSide!=24&&cellsPerSide!=48)
  throw std::invalid_argument("supported online patch domains: 12, 24 or 48 cells");
 sheets_=sheets;cells_=cellsPerSide;minSpan_=cellsPerSide/4;
 patches_.clear();patches_.reserve(std::size_t(sheets)*8);
 for(int sheet=0;sheet<sheets;++sheet)
  for(int y=0;y<cells_;y+=cells_/2)for(int x=0;x<cells_;x+=cells_/2)
   patches_.push_back({sheet,x,y,cells_/2});
 std::sort(patches_.begin(),patches_.end(),lessPatch);
 lastPatchEditFrame_=-1000;lastModeSwitchFrame_=-1000;
 hierarchical_=true;
 flatTimeEwma_=hierTimeEwma_=0;
 flatSamples_=hierSamples_=0;
}
int OnlinePartitioner::regionForCell(int sheet,int x,int y) const{
 if(sheet<0||sheet>=sheets_||x<0||x>=cells_||y<0||y>=cells_)
  throw std::invalid_argument("online patch cell outside configured domain");
 for(const auto& p:patches_)
  if(p.sheet==sheet&&x>=p.x&&x<p.x+p.span&&y>=p.y&&y<p.y+p.span)
   return stableId(p);
 throw std::runtime_error("online patch cover has a hole");
}
void OnlinePartitioner::split(std::size_t index){
 const OnlinePatch source=patches_.at(index);
 if(source.span<=minSpan_)throw std::logic_error("minimum online patch cannot split");
 patches_.erase(patches_.begin()+static_cast<std::ptrdiff_t>(index));
 const int half=source.span/2;
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
  OnlinePatch child{source.sheet,source.x+dx*half,source.y+dy*half,half};
  // Carry some of the parent's work so the new patches are not interpreted
  // as cold/stationary. Division is an estimate, not a measured child cost.
  child.workEwma=source.workEwma*.25;
  child.contactsEwma=source.contactsEwma*.25;
  child.candidatesEwma=source.candidatesEwma*.25;
  patches_.push_back(child);
 }
 std::sort(patches_.begin(),patches_.end(),lessPatch);
}
void OnlinePartitioner::merge(std::size_t first,const OnlinePatch& parent){
 const int half=parent.span/2;
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
  const int x=parent.x+dx*half,y=parent.y+dy*half;
  auto it=std::find_if(patches_.begin(),patches_.end(),[&](const OnlinePatch& p){
   return p.sheet==parent.sheet&&p.x==x&&p.y==y&&p.span==half;
  });
  if(it==patches_.end())throw std::logic_error("online merge missing child");
  patches_.erase(it);
 }
 (void)first; // selected sibling index is advisory; coordinates define quartet
 patches_.push_back(parent);
 std::sort(patches_.begin(),patches_.end(),lessPatch);
}
OnlineUpdate OnlinePartitioner::observe(int frame,
 const std::map<int,RegionWork>& work,
 const std::vector<ccp::live::Bounds>& guards,
 double broadMilliseconds,std::size_t queryCount){
 const auto start=Clock::now();
 if(frame<=0||!std::isfinite(broadMilliseconds)||broadMilliseconds<0||queryCount==0)
  throw std::invalid_argument("invalid online feedback measurement");
 OnlineUpdate result;
 // Measurements are ACTUAL AABB tests, cache revalidation visits, and
 // confirmed precise-contact response. No oracle-generated "true contacts"
 // or fabricated hardware performance is used here.
 for(auto& p:patches_){
  const auto it=work.find(stableId(p));
  const RegionWork count=it==work.end()?RegionWork{}:it->second;
  const double measuredWork=double(count.aabbChecks)+
        .65*double(count.cachedChecks)+2.*double(count.rejectedCandidates)+
        3.*double(count.invalidations);
  const double measuredContacts=double(count.contactCorrections);
  const double measuredCandidates=double(count.candidateTouches);
  const double beta=.22;
  p.workEwma=(1.-beta)*p.workEwma+beta*measuredWork;
  p.contactsEwma=(1.-beta)*p.contactsEwma+beta*measuredContacts;
  p.candidatesEwma=(1.-beta)*p.candidatesEwma+beta*measuredCandidates;
  result.observedWork+=measuredWork;
  result.observedContacts+=measuredContacts;
 }
 // Topology-independent diagnostic: compare current conservative patch
 // union boxes, not stale narrow-phase contacts.
 // Sweep sorted coarse envelopes: sparse workloads avoid the former
 // quadratic all-guards scan. Dense worst cases still have quadratic true
 // overlaps (an unavoidable output-size lower bound for pair enumeration).
 std::vector<std::size_t> sorted(guards.size());
 std::iota(sorted.begin(),sorted.end(),std::size_t{0});
 std::sort(sorted.begin(),sorted.end(),[&](std::size_t a,std::size_t b){
  return guards[a].min.x<guards[b].min.x||
    (guards[a].min.x==guards[b].min.x&&a<b);
 });
 std::size_t near=0;
 for(std::size_t p=0;p<sorted.size();++p){
  const auto& a=guards[sorted[p]];
  for(std::size_t q=p+1;q<sorted.size();++q){
   const auto& b=guards[sorted[q]];
   if(b.min.x>a.max.x)break;
   if(overlaps(a,b))++near;
  }
 }
 const double count=double(guards.size());
 const double possible=count*(count-1.)*.5;
 result.coarsePairFraction=possible>0.?double(near)/possible:1.0;
 // Both update strategies are measured per broad-phase query while active.
 const double sample=broadMilliseconds/double(queryCount);
 double& ewma=hierarchical_?hierTimeEwma_:flatTimeEwma_;
 int& samples=hierarchical_?hierSamples_:flatSamples_;
 ewma=samples?ewma*.85+sample*.15:sample;
 ++samples;
 // A bounded decision interval, cooldown and a minimum gain avoid frequent
 // invalidation and unbounded topology thrashing.
 constexpr int window=6;
 constexpr int editCooldown=12;
 if(frame%window==0){
  for(auto& p:patches_){
   const bool contactRich=p.contactsEwma>.30||p.candidatesEwma>14.;
   const double threshold=p.span>=12?260.:160.;
   if(p.span>minSpan_&&p.workEwma>threshold&&contactRich){
    ++p.hotWindows;p.quietWindows=0;
   }else if(p.workEwma<45.&&p.contactsEwma<.12){
    ++p.quietWindows;p.hotWindows=0;
   }else{
    p.hotWindows=0;p.quietWindows=0;
   }
  }
  if(frame-lastPatchEditFrame_>=editCooldown){
   // First consider a full four-sibling merge: fragment management costs
   // more than useful candidate culling once local contact activity subsides.
   bool merged=false;
   for(int childSpan:{minSpan_,minSpan_*2}){
    const int parentSpan=childSpan*2;
    for(const auto& source:patches_){
     if(source.span!=childSpan)continue;
     const int px=(source.x/parentSpan)*parentSpan;
     const int py=(source.y/parentSpan)*parentSpan;
     if(source.x!=px||source.y!=py)continue;
     bool allQuiet=true;double sumWork=0,sumContact=0,sumCandidates=0;
     for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
      const int x=px+dx*childSpan,y=py+dy*childSpan;
      const auto it=std::find_if(patches_.begin(),patches_.end(),
        [&](const OnlinePatch& p){return p.sheet==source.sheet&&p.x==x&&p.y==y&&p.span==childSpan;});
      if(it==patches_.end()||it->quietWindows<3){allQuiet=false;break;}
      sumWork+=it->workEwma;sumContact+=it->contactsEwma;
      sumCandidates+=it->candidatesEwma;
     }
     if(allQuiet){
      OnlinePatch parent{source.sheet,px,py,parentSpan};
      parent.workEwma=sumWork;parent.contactsEwma=sumContact;
      parent.candidatesEwma=sumCandidates;
      merge(0,parent);
      ++result.merges;
      lastPatchEditFrame_=frame;
      merged=true;break;
     }
    }
    if(merged)break;
   }
   if(!merged){
    // A single controlled split per interval is a cheap on-line analogue
    // of a best-first refinement step. It responds to *observed* contact
    // work and only changes regions belonging to this hot patch.
    std::size_t selected=patches_.size();
    double highest=0;
    for(std::size_t i=0;i<patches_.size();++i){
     const auto& p=patches_[i];
     if(p.span>minSpan_&&p.hotWindows>=2&&p.workEwma>highest){
      highest=p.workEwma;selected=i;
     }
    }
    if(selected<patches_.size()){
     split(selected);++result.splits;lastPatchEditFrame_=frame;
    }
   }
  }
  // Policy selection is reaction to CURRENT region occupancy + measured
  // broad-phase time. A hysteresis band prevents mode flapping. This is
  // not a fair, same-state dual-policy timing oracle; it is feedback control.
  if(frame-lastModeSwitchFrame_>=18){
   const bool sparse=guards.size()>=12&&result.coarsePairFraction<.42;
   bool proposed=sparse;
   if(flatSamples_>=6&&hierSamples_>=6){
    const double faster=std::min(flatTimeEwma_,hierTimeEwma_);
    const double slower=std::max(flatTimeEwma_,hierTimeEwma_);
    if(slower>faster*1.18)
     proposed=hierTimeEwma_<flatTimeEwma_;
   }
   if(proposed!=hierarchical_){
    hierarchical_=proposed;
    ++result.strategySwitches;lastModeSwitchFrame_=frame;
   }
  }
 }
 result.hierarchy=hierarchical_;
 result.patches=patches_.size();
 result.decisionMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 return result;
}
} // namespace ccp::research

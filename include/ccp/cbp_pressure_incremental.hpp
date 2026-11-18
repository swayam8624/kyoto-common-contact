#pragma once
// Phase 14 — exact persistent canonical-cell interaction index.
//
// Independent ablation; frozen Phase 11-13 algorithms remain intact.
//
// Each AABB retains memberships in the exact integer spatial cells covered
// by its CURRENT position. On each frame we examine all N current objects
// (cost O(N)), but repair only AABBs whose occupied cell range changed.
// Non-crossing motion still updates the stored Float32 bounds so current
// overlap tests remain exact. Large ownership changes force a fully charged
// rebuild. A cell is deleted when its occupancy becomes empty: no unbounded
// accumulation of vacant historical cells.
//
// Every unordered pair present in multiple spatial cells is emitted only in
// its canonical overlap-minimum cell, just like the independent fixed-grid
// oracle. Hash+count verification is required for empirical correctness.
// This is a standard incremental indexing idea, NOT yet novel research.
#include "ccp/cbp_pressure.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ccp::pressure {

struct PersistentRange {
 std::array<int,3> lo{},hi{};
 bool operator==(const PersistentRange&)const=default;
 std::uint64_t cells() const noexcept {
  return std::uint64_t(hi[0]-lo[0]+1)*
         std::uint64_t(hi[1]-lo[1]+1)*
         std::uint64_t(hi[2]-lo[2]+1);
 }
};
struct PersistentStats {
 bool rebuilt{};
 std::size_t changedRanges{},changedBounds{},activeCells{};
 std::uint64_t entries{},workUpper{},insertions{},removals{};
 double preflightMs{},maintenanceMs{};
};
class PersistentGrid {
 struct Bin {std::vector<std::uint32_t> ids;};
 struct Membership {
  Cell cell{};
  std::uint32_t slot{};
 };
 std::unordered_map<Cell,Bin,CellHash> bins_;
 std::vector<std::vector<Membership>> references_;
 std::vector<PersistentRange> occupied_;
 std::vector<Box> current_;
 std::uint64_t entries_{},work_{};
 bool initialized_{};
 std::uint64_t maxEntries_{};
 // Ownership-change fallback is checked BEFORE a partially updated grid
 // is modified. Rebuild is fully counted in this frame's maintenance time.
 static constexpr std::size_t kChangeThresholdPercent=30;

 static PersistentRange rangeFor(const Box& box) {
  PersistentRange r;
  for(int axis=0;axis<3;++axis){
   if(!std::isfinite(box.lo[axis])||!std::isfinite(box.hi[axis])||
      box.lo[axis]>box.hi[axis])
    throw std::invalid_argument("Invalid current AABB at incremental update");
   r.lo[axis]=cellCoord(box.lo[axis]);
   r.hi[axis]=cellCoord(box.hi[axis]);
  }
  if(r.cells()>256)
   throw std::runtime_error("Single AABB spans >256 cells; fixed max budget");
  return r;
 }
 template<class Fn>
 static void forCells(const PersistentRange& r,Fn&& fn) {
  for(int x=r.lo[0];x<=r.hi[0];++x)
   for(int y=r.lo[1];y<=r.hi[1];++y)
    for(int z=r.lo[2];z<=r.hi[2];++z)
     fn(Cell{x,y,z});
 }
 void addMembership(std::uint32_t id,Cell c) {
  auto& bin=bins_[c].ids;
  if(bin.size()>=std::numeric_limits<std::uint32_t>::max())
   throw std::length_error("Cell membership slot overflows");
  const auto old=std::uint64_t(bin.size());
  bin.push_back(id);
  references_[id].push_back({c,std::uint32_t(old)});
  work_+=old; // C(k+1,2)-C(k,2)=k
  ++entries_;
 }
 void removeMembership(std::uint32_t id,Membership member) {
  auto it=bins_.find(member.cell);
  if(it==bins_.end())
   throw std::logic_error("Missing expected persistent spatial cell");
  auto& ids=it->second.ids;
  const std::size_t slot=member.slot;
  if(slot>=ids.size()||ids[slot]!=id)
   throw std::logic_error("Broken reverse slot into persistent cell");
  const auto last=ids.back();
  const auto lastSlot=ids.size()-1;
  ids[slot]=last;
  ids.pop_back();
  if(last!=id) {
   auto& refs=references_[last];
   bool fixed=false;
   for(auto& ref:refs){
    if(ref.cell==member.cell &&
       std::size_t(ref.slot)==lastSlot){
     ref.slot=std::uint32_t(slot);
     fixed=true;
     break;
    }
   }
   if(!fixed)
    throw std::logic_error("Missing swapped object's reverse membership");
  }
  work_-=std::uint64_t(ids.size()); // C(k,2)-C(k-1,2)=k-1
  --entries_;
  if(ids.empty())bins_.erase(it);
 }
 void rebuild(const std::vector<Box>& boxes,
              const std::vector<PersistentRange>& ranges){
  bins_.clear();
  references_.clear();
  references_.resize(boxes.size());
  occupied_=ranges;
  current_=boxes;
  entries_=work_=0;
  // We intentionally do NOT keep a second untouched full grid hot:
  // a rebuild discards old memberships and charges the new construction.
  for(std::size_t i=0;i<boxes.size();++i)
   forCells(occupied_[i],[&](Cell c){
    addMembership(std::uint32_t(i),c);
   });
  initialized_=true;
 }
public:
 explicit PersistentGrid(std::uint64_t maxEntries=12000000):
  maxEntries_(maxEntries) {
  if(maxEntries_==0||maxEntries_>20000000)
   throw std::invalid_argument("Invalid incremental membership cap");
 }
 const std::vector<Box>& currentBoxes() const noexcept {return current_;}
 std::uint64_t workUpperBound()const noexcept{return work_;}
 std::uint64_t entries()const noexcept{return entries_;}
 std::size_t cells()const noexcept{return bins_.size();}
 bool initialized()const noexcept{return initialized_;}
 PersistentStats advance(const std::vector<Box>& boxes){
  const auto start=std::chrono::steady_clock::now();
  if(boxes.size()<16||boxes.size()>2000000)
   throw std::invalid_argument("Persistent broad-phase scene size out of bounds");
  std::vector<PersistentRange> newRanges;
  newRanges.reserve(boxes.size());
  std::size_t changedRanges=0,changedBounds=0;
  std::uint64_t totalEntries=0;
  const bool changedSize=!initialized_||boxes.size()!=current_.size();
  for(std::size_t i=0;i<boxes.size();++i) {
   const auto r=rangeFor(boxes[i]);
   if(r.cells()>maxEntries_-std::min(totalEntries,maxEntries_))
    throw std::runtime_error("Current occupancy exceeds persistent budget");
   totalEntries+=r.cells();
   newRanges.push_back(r);
   if(changedSize||!(r==occupied_[i]))++changedRanges;
   if(changedSize){
    ++changedBounds;
   }else{
    // Only coordinates participating in current AABB overlap matter.
    bool different=false;
    for(int k=0;k<3;++k)
     if(boxes[i].lo[k]!=current_[i].lo[k]||
        boxes[i].hi[k]!=current_[i].hi[k]){
      different=true;
      break;
     }
    changedBounds+=different;
   }
  }
  const auto preflightMs=since(start);
  const auto maintenance=std::chrono::steady_clock::now();
  const bool rebuildNeeded = changedSize ||
    changedRanges>boxes.size()*kChangeThresholdPercent/100;
  std::uint64_t inserts=0,removes=0;
  if(rebuildNeeded){
   const auto previous=entries_;
   rebuild(boxes,newRanges);
   inserts=entries_;
   removes=previous;
  }else{
   // Invalidate only those memberships that truly changed their cell
   // range; the current AABB vector itself is always updated.
   for(std::size_t id=0;id<boxes.size();++id){
    if(newRanges[id]==occupied_[id])continue;
    removes+=references_[id].size();
    for(const auto member:references_[id])
     removeMembership(std::uint32_t(id),member);
    references_[id].clear();
    occupied_[id]=newRanges[id];
    forCells(newRanges[id],[&](Cell c){
     addMembership(std::uint32_t(id),c);
     ++inserts;
    });
   }
   current_=boxes;
  }
  if(entries_!=totalEntries)
   throw std::logic_error("Persistent occupancy count violated");
  return {rebuildNeeded,changedRanges,changedBounds,bins_.size(),
          entries_,work_,inserts,removes,preflightMs,
          since(maintenance)};
 }
 Result enumerate(std::uint64_t maxTests,double maxMs)const {
  if(!initialized_)
   throw std::logic_error("No AABB scene installed in persistent grid");
  Result out;
  const auto start=std::chrono::steady_clock::now();
  for(const auto& [cell,bin]:bins_){
   const auto& ids=bin.ids;
   for(std::size_t i=0;i<ids.size();++i)
    for(std::size_t j=i+1;j<ids.size();++j){
     if(out.tests>=maxTests||
        (out.tests%65536==0&&since(start)>maxMs)){
      out.complete=false;out.ms=since(start);return out;
     }
     ++out.tests;
     const auto a=ids[i],b=ids[j];
     if(overlap(current_[a],current_[b]) &&
        canonical(current_[a],current_[b])==cell)
      add(out.digest,a,b);
    }
  }
  out.ms=since(start);
  if(out.tests!=work_)
   throw std::logic_error("Grid test-count upper bound not reached");
  return out;
 }
};
} // namespace ccp::pressure

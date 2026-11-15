// Regression: a local cache repair must NEVER lose or invent an exact current
// AABB overlap, even under abrupt motion, mask changes or ID permutations.
#include "ccp/selective_guard.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using ccp::live::Bounds;
using ccp::live::Pair;
namespace {
std::vector<Pair> truth(std::vector<Bounds> boxes){
 constexpr double h=.025*.5;
 for(auto& b:boxes){
  b.min.x-=h;b.min.y-=h;b.min.z-=h;
  b.max.x+=h;b.max.y+=h;b.max.z+=h;
 }
 return ccp::live::candidates(boxes,true);
}
void insist(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main(){
 try{
  ccp::research::SelectiveGuard cache;
  std::size_t full=0,repair=0,reuse=0;
  for(int frame=0;frame<280;++frame){
   std::vector<Bounds> boxes;
   for(int i=0;i<96;++i){
    const double x=double(i%12)*.13+.013*std::sin(.11*frame+i*.14);
    const double y=double(i/12)*.13+.013*std::cos(.08*frame+i*.18);
    const double teleport=(frame%31>=22&&i==27)?.65:0;
    const double z=.014*std::sin(frame*.1+i*.33);
    Bounds b;
    b.id=i*3+17;
    b.min={x+teleport,y,z};b.max={x+teleport+.1,y+.10,z+.10};
    b.category=(i%4==0)?2u:1u;
    b.mask=(frame%41>=30&&i%3==0)?1u:3u;
    b.movable=true;
    boxes.push_back(b);
   }
   // A reordered but geometrically identical ID sequence must not reuse
   // index-keyed cached pairs from the previous ordering.
   if(frame%49==23)std::reverse(boxes.begin(),boxes.end());
   ccp::research::SelectiveGuard::Metrics m;
   const auto actual=cache.query(boxes,m);
   const auto expected=truth(boxes);
   if(actual!=expected)
    throw std::runtime_error("selective guard differs from exhaustive-equivalent SAP at frame "+
                             std::to_string(frame));
   full+=m.fullBuilds;repair+=m.partialRepairs;reuse+=m.cacheReuses;
  }
  insist(full>0,"no full rebuild path tested");
  insist(repair>0,"no localized guard repair path tested");
  insist(reuse>0,"no certified pair-cache reuse path tested");
  // Inputs with invalid coordinates must be rejected prior to publishing a
  // cache certificate. Reusing the cache afterwards must remain safe.
  std::vector<Bounds> invalid={
   {5,{0,0,0},{.2,.2,.2},1u,1u,true},
   {6,{0,0,0},{.2,.2,.2},1u,1u,true}};
  invalid[1].min.x=std::numeric_limits<double>::quiet_NaN();
  bool rejected=false;
  ccp::research::SelectiveGuard::Metrics m;
  try{(void)cache.query(invalid,m);}catch(const std::invalid_argument&){rejected=true;}
  insist(rejected,"invalid geometry must fail closed");
  invalid[1].min.x=.02;
  insist(cache.query(invalid,m)==truth(invalid),"cache poisoned by invalid query");
  std::cout<<"PASS selective guard: frames=280 full="<<full
    <<" local_repairs="<<repair<<" cached_reuses="<<reuse
    <<" exact_pair_sets=280 masks=variable teleport=YES permutations=YES\n";
 }catch(const std::exception& e){
  std::cerr<<"SELECTIVE GUARD FAILED: "<<e.what()<<'\n';return 1;
 }
}

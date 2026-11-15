// CPU replay of exact Float32 material snapshots used by the Metal benchmark.
// Every algorithm processes identical immutable bounds. The replay AABBs are
// ALREADY padded by the material exporter; all margins below are therefore 0.
// All methods materialize full current sorted pair sets; every frame is
// compared against independently executed SAP and sampled brute-force oracle.
#include "ccp/generic_online.hpp"
#include "ccp/learned_online.hpp"
#include "ccp/selective_guard.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef CCP_HAVE_BULLET
#include <btBulletCollisionCommon.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#endif
namespace {
using namespace ccp::research;
using Clock=std::chrono::steady_clock;
class MaterialReplay {
 std::ifstream input_;
public:
 uint32_t n{},frames{},seed{},material{};
 explicit MaterialReplay(const std::string& filename):input_(filename,std::ios::binary){
  char magic[8]={};input_.read(magic,8);
  if(!input_||std::string(magic,8)!="KYOTOM01")
   throw std::runtime_error("invalid snapshot file: expected KYOTOM01 header");
  for(auto* p:{&n,&frames,&seed,&material})
   input_.read(reinterpret_cast<char*>(p),sizeof(uint32_t));
  if(!input_||n<1||n>50000||frames<1||frames>10000||material>5)
   throw std::runtime_error("invalid snapshot header dimensions");
  auto start=input_.tellg();
  input_.seekg(0,std::ios::end);
  const auto bytes=input_.tellg()-start;
  const auto expected=std::streamoff(n)*std::streamoff(frames)*std::streamoff(6*sizeof(float));
  if(bytes!=expected)throw std::runtime_error("material snapshot length mismatch");
  input_.seekg(start);
 }
 std::vector<Bounds> next(){
  std::vector<Bounds> result;result.reserve(n);
  for(uint32_t i=0;i<n;++i){
   std::array<float,6> v{};
   input_.read(reinterpret_cast<char*>(v.data()),sizeof(v));
   if(!input_)throw std::runtime_error("material snapshot truncated");
   for(float x:v)if(!std::isfinite(x))throw std::runtime_error("nonfinite material coordinate");
   if(v[0]>v[3]||v[1]>v[4]||v[2]>v[5])
    throw std::runtime_error("inverted material bounds");
   result.push_back({int(i),{v[0],v[1],v[2]},{v[3],v[4],v[5]},1u,1u,true});
  }
  return result;
 }
};
using Pairs=std::vector<Pair>;
bool overlap(const Bounds& a,const Bounds& b){
 return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
        a.min.y<=b.max.y&&b.min.y<=a.max.y&&
        a.min.z<=b.max.z&&b.min.z<=a.max.z;
}
Pairs exactGuard(const Pairs& candidate,const std::vector<Bounds>& boxes){
 Pairs out;out.reserve(candidate.size());
 for(auto p:candidate)if(overlap(boxes.at(size_t(p.a)),boxes.at(size_t(p.b))))
  out.push_back(p);
 return out;
}
std::vector<Primitive> primitives(const std::vector<Bounds>& boxes,int span=64){
 std::vector<Primitive> result;result.reserve(boxes.size());
 for(size_t i=0;i<boxes.size();++i)result.push_back({boxes[i],int(i/size_t(span)),-1});
 return result;
}
uint64_t pairHash(const Pairs& pairs){
 uint64_t h=UINT64_C(14695981039346656037);
 for(auto p:pairs){
  for(uint32_t x:{uint32_t(p.a),uint32_t(p.b)}){
   h^=uint64_t(x);h*=UINT64_C(1099511628211);
  }
 }
 return h;
}
void requireSortedUnique(const Pairs& pairs,size_t count){
 for(size_t i=0;i<pairs.size();++i){
  const auto p=pairs[i];
  if(p.a<0||p.b<=p.a||size_t(p.b)>=count)
   throw std::runtime_error("invalid replay pair IDs");
  if(i && !(pairs[i-1]<p))throw std::runtime_error("non-unique or unsorted replay pairs");
 }
}
#ifdef CCP_HAVE_BULLET
// External published-family baseline: upstream Bullet persistent DBVT.
// Update the same proxies each frame, never rebuild the tree from scratch.
class PersistentBullet {
 btDefaultCollisionConfiguration config_;
 btCollisionDispatcher dispatcher_{&config_};
 btDbvtBroadphase broadphase_;
 std::vector<Bounds> stored_;
 std::vector<btBroadphaseProxy*> proxies_;
public:
 ~PersistentBullet(){
  for(auto* p:proxies_)broadphase_.destroyProxy(p,&dispatcher_);
 }
 Pairs query(const std::vector<Bounds>& boxes){
  if(proxies_.empty()){
   stored_=boxes;
   proxies_.reserve(stored_.size());
   for(auto& b:stored_){
    auto* proxy=broadphase_.createProxy(
     btVector3(btScalar(b.min.x),btScalar(b.min.y),btScalar(b.min.z)),
     btVector3(btScalar(b.max.x),btScalar(b.max.y),btScalar(b.max.z)),
     BOX_SHAPE_PROXYTYPE,static_cast<void*>(&b),1,1,&dispatcher_);
    if(!proxy)throw std::runtime_error("Bullet failed to construct DBVT proxy");
    proxies_.push_back(proxy);
   }
  }else{
   if(boxes.size()!=stored_.size())
    throw std::runtime_error("Bullet material topology changed");
   for(size_t i=0;i<boxes.size();++i){
    if(boxes[i].id!=stored_[i].id)
     throw std::runtime_error("Bullet material stable-ID mismatch");
    stored_[i]=boxes[i];
    const auto& b=stored_[i];
    broadphase_.setAabbForceUpdate(
     proxies_[i],btVector3(btScalar(b.min.x),btScalar(b.min.y),btScalar(b.min.z)),
     btVector3(btScalar(b.max.x),btScalar(b.max.y),btScalar(b.max.z)),
     &dispatcher_);
   }
  }
  broadphase_.calculateOverlappingPairs(&dispatcher_);
  Pairs result;
  const auto& cached=broadphase_.getOverlappingPairCache()->getOverlappingPairArray();
  result.reserve(size_t(cached.size()));
  for(int i=0;i<cached.size();++i){
   const auto* pa=cached[i].m_pProxy0,*pb=cached[i].m_pProxy1;
   if(!pa||!pb||!pa->m_clientObject||!pb->m_clientObject)
    throw std::runtime_error("invalid Bullet pair");
   const auto& a=*static_cast<const Bounds*>(pa->m_clientObject);
   const auto& b=*static_cast<const Bounds*>(pb->m_clientObject);
   if(!overlap(a,b))continue;
   result.push_back({std::min(a.id,b.id),std::max(a.id,b.id)});
  }
  std::sort(result.begin(),result.end());
  result.erase(std::unique(result.begin(),result.end()),result.end());
  return result;
 }
};
#endif
enum class Kind {Sap,Global,Selective,Fixed,Adaptive,Hierarchy,Online,Learned
#ifdef CCP_HAVE_BULLET
,Bullet
#endif
};
const char* name(Kind kind){
 switch(kind){
 case Kind::Sap:return "sap_rebuild";
 case Kind::Global:return "global_guarded_exact";
 case Kind::Selective:return "selective_guard_exact";
 case Kind::Fixed:return "region_fixed";
 case Kind::Adaptive:return "region_adaptive";
 case Kind::Hierarchy:return "region_hierarchical";
 case Kind::Online:return "online_cost_aware";
 case Kind::Learned:return "ml_cost_aware";
#ifdef CCP_HAVE_BULLET
 case Kind::Bullet:return "bullet_dbvt_persistent";
#endif
 }
 return "invalid";
}
struct Observation {
 Pairs pairs;
 double ms{};
 size_t builds{},reuses{},repairs{},aabbTests{},workers{1};
 int span{64};bool usedGlobal{};
 ccp::learned::Features features{};
 std::string selected{"none"};
 bool switched{};
};
class Method {
 Kind kind_;
 ccp::live::BroadPhaseService global_;
 ccp::research::SelectiveGuard selective_{0,.045};
 RegionalCandidates regional_;
 ccp::bench::GenericOnline online_{0};
 std::unique_ptr<ccp::bench::LearnedOnline> learned_;
 ccp::learned::Extractor features_;
#ifdef CCP_HAVE_BULLET
 PersistentBullet bullet_;
#endif
public:
 explicit Method(Kind kind,const std::string& model=""):kind_(kind){
  if(kind==Kind::Learned){
   if(model.empty())throw std::invalid_argument("ML model required for learned method");
   learned_=std::make_unique<ccp::bench::LearnedOnline>(model,0.);
  }
 }
 Observation run(const std::vector<Bounds>& boxes){
  Observation r;
  const auto fullStart=Clock::now();
  r.features=features_.describe(boxes);
  // Existing teacher timings EXCLUDE feature collection. Only learned
  // selector wall time INCLUDES it and network inference.
  const auto start=Clock::now();
  if(kind_==Kind::Learned){
   const auto result=learned_->query(primitives(boxes));
   r.pairs=result.pairs;r.span=result.span;r.usedGlobal=result.usedGlobal;
   r.builds=result.builds+result.guardBuilds;
   r.reuses=result.reuses+result.guardReuses;
   r.repairs=result.selectiveRepairs;r.aabbTests=result.aabbTests;
   r.workers=result.workers;r.selected=result.learnedAction;
   r.switched=result.switched;
  }else if(kind_==Kind::Sap)
   r.pairs=ccp::live::candidates(boxes,true);
#ifdef CCP_HAVE_BULLET
  else if(kind_==Kind::Bullet)r.pairs=bullet_.query(boxes);
#endif
  else if(kind_==Kind::Global){
   ccp::live::QueryMetrics m;
   const auto& cached=global_.query(boxes,{0,.045,ccp::live::UpdatePolicy::GuardedReuse},true,m);
   r.pairs=exactGuard(cached,boxes);
   r.builds=m.builds;r.reuses=m.reuses;r.aabbTests=m.pairTests;
  }else if(kind_==Kind::Selective){
   ccp::research::SelectiveGuard::Metrics m;
   const auto& result=selective_.query(boxes,m);
   r.pairs.assign(result.begin(),result.end());
   r.builds=m.fullBuilds;r.reuses=m.cacheReuses;
   r.repairs=m.partialRepairs;r.aabbTests=m.pairChecks;
   r.workers=m.parallelWorkers;
  }else{
   auto items=primitives(boxes);
   if(kind_==Kind::Online){
    const auto result=online_.query(items);
    r.pairs=result.pairs;r.span=result.span;r.usedGlobal=result.usedGlobal;
    r.builds=result.builds+result.guardBuilds;
    r.reuses=result.reuses+result.guardReuses;
    r.repairs=result.selectiveRepairs;r.aabbTests=result.aabbTests;
    r.workers=result.workers;
   }else{
    Metrics m;
    const auto& result=regional_.query(items,0,kind_!=Kind::Fixed,m,kind_==Kind::Hierarchy);
    r.pairs.assign(result.begin(),result.end());
    r.builds=m.regionalPairRebuilds;r.reuses=m.regionalPairReuses;
    r.aabbTests=m.broadAabbTests;
   }
  }
  const auto backendEnd=Clock::now();
  features_.observe(boxes,r.pairs.size());
  r.ms=std::chrono::duration<double,std::milli>(
   (kind_==Kind::Learned?Clock::now():backendEnd)-
   (kind_==Kind::Learned?fullStart:start)).count();
  return r;
 }
};
void run(const std::string& filename,const std::string& output,int verbose,
         const std::string& model){
 MaterialReplay header(filename);
 if(std::filesystem::exists(output))throw std::runtime_error("refusing to overwrite CPU replay results");
 std::filesystem::path file(output);
 if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
 std::ofstream csv(output);
 if(!csv)throw std::runtime_error("cannot create replay CSV");
 csv<<"backend,material,n,frame,seed,ms,pairs,pair_hash,oracle_checked,misses,"
       "builds,reuses,selective_repairs,aabb_tests,parallel_workers,online_span,online_path,chosen_backend,ml_switched";
 for(int i=0;i<ccp::learned::kInputs;++i)csv<<",f"<<i;
 csv<<'\n';
 csv<<std::fixed<<std::setprecision(9);
 std::vector<uint64_t> expectedHashes(header.frames,0);
 std::vector<std::size_t> expectedCounts(header.frames,0);
 std::vector<Kind> methods={Kind::Sap,Kind::Global,Kind::Selective,
  Kind::Fixed,Kind::Adaptive,Kind::Hierarchy,Kind::Online};
#ifdef CCP_HAVE_BULLET
 methods.push_back(Kind::Bullet);
#endif
 if(!model.empty())methods.push_back(Kind::Learned);
 for(Kind method:methods){
  MaterialReplay stream(filename);
  Method algorithm(method,model);
  double sum=0;
  for(uint32_t frame=0;frame<header.frames;++frame){
   const auto boxes=stream.next();
   const auto observed=algorithm.run(boxes);
   requireSortedUnique(observed.pairs,boxes.size());
   const auto hash=pairHash(observed.pairs);
   bool audited=frame%5==0;
   if(method==Kind::Sap){
    expectedHashes[frame]=hash;expectedCounts[frame]=observed.pairs.size();
    if(audited&&header.n<=512){
     const auto brute=ccp::live::candidates(boxes,false);
     if(brute!=observed.pairs)
      throw std::runtime_error("SAP disagrees with independent exhaustive AABB oracle");
    }
   }else if(hash!=expectedHashes[frame]||observed.pairs.size()!=expectedCounts[frame]){
    throw std::runtime_error("replay pair mismatch: "+std::string(name(method))+
                             " frame "+std::to_string(frame));
   }
   sum+=observed.ms;
   csv<<name(method)<<','<<header.material<<','<<header.n<<','<<frame<<','<<header.seed<<','
      <<observed.ms<<','<<observed.pairs.size()<<','<<hash<<','<<(audited?1:0)
      <<",0,"<<observed.builds<<','<<observed.reuses<<','<<observed.repairs<<','
      <<observed.aabbTests<<','<<observed.workers<<','<<observed.span<<','
      <<(observed.usedGlobal?"localguard":"region")<<','
      <<observed.selected<<','<<(observed.switched?1:0);
   for(double f:observed.features)csv<<','<<f;
   csv<<'\n';
   if(verbose>=2||(verbose==1&&(frame==0||(frame+1)%std::max(1u,header.frames/10)==0)))
    std::cerr<<"[REPLAY] method="<<name(method)<<" frame="<<frame+1<<'/'
      <<header.frames<<" ms="<<observed.ms<<" pairs="<<observed.pairs.size()
      <<" path="<<(observed.usedGlobal?"localguard":"region")
      <<" oracle="<<(audited?"PASS":"SAP reference match")<<'\n';
  }
  std::cout<<"[RESULT] method="<<name(method)<<" mean_ms="<<sum/header.frames
   <<" frames="<<header.frames<<" n="<<header.n<<"\n";
 }
 if(!csv)throw std::runtime_error("failed to write replay CSV");
 std::cout<<"[PASS] exact pair hashes matched across "<<methods.size()<<" CPU policies for "
   <<header.frames<<" frames: "<<output<<"\n";
}
}
int main(int argc,char** argv){
 try{
  if(argc<3||argc>5)
   throw std::runtime_error("usage: ccp_material_replay INPUT.kmcb OUTPUT.csv [VERBOSE=1] [ML_MODEL.txt]");
  int verbose=argc==4?std::stoi(argv[3]):1;
  if(verbose<0||verbose>2)throw std::runtime_error("verbosity must be 0..2");
  run(argv[1],argv[2],verbose,argc==5?argv[4]:"");
 }catch(const std::exception& e){
  std::cerr<<"MATERIAL CPU REPLAY FAILED: "<<e.what()<<'\n';return 1;
 }
}

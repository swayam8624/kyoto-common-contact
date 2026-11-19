// CPU replay of exact Float32 material snapshots used by the Metal benchmark.
// Every algorithm processes identical immutable bounds. The replay AABBs are
// ALREADY padded by the material exporter; all margins below are therefore 0.
// All methods materialize full current sorted pair sets; every frame is
// compared against independently executed SAP and sampled brute-force oracle.
#include "ccp/generic_online.hpp"
#include "ccp/learned_online.hpp"
#include "ccp/final_broadphase.hpp"
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
#include <random>
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
using ccp::final_broadphase::PersistentSAP;
using ccp::final_broadphase::GuardSAPBoundedTrial;
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

// Persistent SAP maintains the previous x-axis order across frames. In
// coherent motion, insertion repair can be much cheaper than rebuilding the
// sort. In adversarial motion, cap the repair shifts and fall back to std::sort,
// so a reverse-ordered frame cannot trigger quadratic sorting work.
// Output contract is identical to the independent fresh-sort SAP oracle.
// Exploratory causal hybrid: never queries a learned model, never uses
// a hindsight framewise oracle, and does not rely on material labels.
// Guard is used by default. Only large, potentially expensive workloads
// trigger occasional measured probes against a persistent x-axis SAP.
// Crucially, probes execute both algorithms and FULL probe overhead remains
// in the measured wall time. Every returned pair list is exact.
class ProbedGuardSAP {
 ccp::live::BroadPhaseService guard_;
 PersistentSAP sap_;
 int frame_{},lastProbe_{-10000};
 bool useSAP_{};
 double lastGuardMs_{};
 static bool inside(const Bounds& raw,const Bounds& envelope){
  return raw.id==envelope.id &&
    raw.category==envelope.category && raw.mask==envelope.mask &&
    raw.movable==envelope.movable &&
    raw.min.x>=envelope.min.x && raw.min.y>=envelope.min.y &&
    raw.min.z>=envelope.min.z && raw.max.x<=envelope.max.x &&
    raw.max.y<=envelope.max.y && raw.max.z<=envelope.max.z;
 }
 double sampleEscapeFraction(const std::vector<Bounds>& boxes) const {
  const auto& envelopes=guard_.envelopes();
  if(envelopes.size()!=boxes.size()||boxes.empty())return 1.;
  const size_t samples=std::min(size_t{32},boxes.size());
  size_t escaped=0;
  for(size_t i=0;i<samples;++i){
   const size_t index=samples==1?0:i*(boxes.size()-1)/(samples-1);
   if(!inside(boxes[index],envelopes[index]))++escaped;
  }
  return double(escaped)/double(samples);
 }
 Pairs queryGuard(const std::vector<Bounds>& boxes){
  ccp::live::QueryMetrics metrics;
  const auto& candidates=guard_.query(
    boxes,{0.,.045,ccp::live::UpdatePolicy::GuardedReuse},true,metrics);
  return exactGuard(candidates,boxes);
 }
public:
 struct Outcome {
  Pairs pairs;
  const char* action{};
  bool switched{},probed{};
 };
 Outcome query(const std::vector<Bounds>& boxes){
  ++frame_;
  const bool previouslySAP=useSAP_;
  // A short-list certificate check is a causal trigger, not a truth oracle:
  // a false negative merely defers exploration and never misses collisions.
  const bool canProbe=boxes.size()>=384 && frame_>1 &&
     frame_-lastProbe_>=24 &&
     (sampleEscapeFraction(boxes)>=.25 || lastGuardMs_>.5);
  if(canProbe){
   lastProbe_=frame_;
   const auto startGuard=Clock::now();
   Pairs fromGuard=queryGuard(boxes);
   const auto afterGuard=Clock::now();
   Pairs fromSAP=sap_.query(boxes);
   const auto afterSAP=Clock::now();
   const double guardMs=std::chrono::duration<double,std::milli>(
                         afterGuard-startGuard).count();
   const double sapMs=std::chrono::duration<double,std::milli>(
                         afterSAP-afterGuard).count();
   lastGuardMs_=guardMs;
   // 12% hysteresis: switching must be meaningfully better.
   // In contrast to an offline optimum, both probe costs are charged.
   if(useSAP_){
    if(guardMs*1.12<sapMs)useSAP_=false;
   }else if(sapMs*1.12<guardMs)useSAP_=true;
   return {useSAP_?std::move(fromSAP):std::move(fromGuard),
           useSAP_?"sap_persistent_coherent":"global_guarded_exact",
           useSAP_!=previouslySAP,true};
  }
  if(useSAP_)
   return {sap_.query(boxes),"sap_persistent_coherent",false,false};
  const auto began=Clock::now();
  auto pairs=queryGuard(boxes);
  lastGuardMs_=std::chrono::duration<double,std::milli>(
                Clock::now()-began).count();
  return {std::move(pairs),"global_guarded_exact",false,false};
 }
};


// One-backend-per-frame causal exploration. The old hybrid double-executed
// Guard and SAP on probe frames and compared a stale-Guard cache with warmed
// SAP, confounding probe cost and cache state. This candidate instead records
// Guard costs while Guard is continuously active; it tests SAP only after
// repeated expensive Guard rebuilds. It can abandon a bad trial after two
// SAP frames. There is NO per-frame hindsight oracle or material label.
// Thresholds are exploratory and MUST be validated on never-inspected seeds.
enum class Kind {Sap,PersistentSap,GuardSapProbe,GuardSapTrial,Global,Selective,Fixed,Adaptive,Hierarchy,Online,Learned,LearnedConservative
#ifdef CCP_HAVE_BULLET
,Bullet
#endif
};
const char* name(Kind kind){
 switch(kind){
 case Kind::Sap:return "sap_rebuild";
 case Kind::PersistentSap:return "sap_persistent_coherent";
 case Kind::GuardSapProbe:return "guard_sap_measured_probe";
 case Kind::GuardSapTrial:return "guard_sap_bounded_trial";
 case Kind::Global:return "global_guarded_exact";
 case Kind::Selective:return "selective_guard_exact";
 case Kind::Fixed:return "region_fixed";
 case Kind::Adaptive:return "region_adaptive";
 case Kind::Hierarchy:return "region_hierarchical";
 case Kind::Online:return "online_cost_aware";
 case Kind::Learned:return "ml_cost_aware";
 case Kind::LearnedConservative:return "ml_cost_aware_conservative";
#ifdef CCP_HAVE_BULLET
 case Kind::Bullet:return "bullet_dbvt_persistent";
#endif
 }
 return "invalid";
}
struct Observation {
 Pairs pairs;
 double ms{},mlFeatureMs{},mlInferenceMs{},mlDecisionMs{},mlBackendMs{},mlBookkeepingMs{},mlWrapperMs{},mlInnerMs{};
 size_t builds{},reuses{},repairs{},aabbTests{},workers{1};
 int span{64};bool usedGlobal{};
 ccp::learned::Features features{};
 std::string selected{"none"};
 bool switched{},bypassed{},hybridProbed{},hybridSwitched{};
};
class Method {
 Kind kind_;
 PersistentSAP persistentSap_;
 ProbedGuardSAP guardSapProbe_;
 GuardSAPBoundedTrial guardSapTrial_;
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
  if(kind==Kind::Learned||kind==Kind::LearnedConservative){
   if(model.empty())throw std::invalid_argument("ML model required for learned method");
   // Experimental conservative control (not a claim of superiority):
   // begin with the strong verified Guard baseline, require a much larger
   // cost advantage, and amortize cache invalidation over longer windows.
   // Compare against default ML and all standalone methods on fresh seeds.
   if(kind==Kind::LearnedConservative)
    learned_=std::make_unique<ccp::bench::LearnedOnline>(model,0.,.10,.30,24,true);
   else learned_=std::make_unique<ccp::bench::LearnedOnline>(model,0.);
  }
 }
 Observation run(const std::vector<Bounds>& boxes){
  Observation r;
  // Common training-feature bookkeeping is outside EVERY method's timed
  // section. The learned controller independently computes its OWN features
  // inside the measured query: never charge an extra duplicate O(N) pass.
  r.features=features_.describe(boxes);
  const auto start=Clock::now();
  if(kind_==Kind::Learned||kind_==Kind::LearnedConservative){
   const auto result=learned_->query(boxes);
   r.pairs=result.pairs;r.span=result.span;r.usedGlobal=result.usedGlobal;
   r.builds=result.builds+result.guardBuilds;
   r.reuses=result.reuses+result.guardReuses;
   r.repairs=result.selectiveRepairs;r.aabbTests=result.aabbTests;
   r.workers=result.workers;r.selected=result.learnedAction;
   r.switched=result.switched;r.bypassed=result.learnedBypassed;
   r.mlFeatureMs=result.mlFeatureMs;r.mlInferenceMs=result.mlInferenceMs;
   r.mlDecisionMs=result.mlDecisionMs;r.mlBackendMs=result.mlBackendMs;
   r.mlBookkeepingMs=result.mlBookkeepingMs;r.mlInnerMs=result.queryMs;
  }else if(kind_==Kind::Sap)
   r.pairs=ccp::live::candidates(boxes,true);
  else if(kind_==Kind::PersistentSap)
   r.pairs=persistentSap_.query(boxes);
  else if(kind_==Kind::GuardSapProbe){
   auto outcome=guardSapProbe_.query(boxes);
   r.pairs=std::move(outcome.pairs);
   r.selected=outcome.action;
   r.hybridSwitched=outcome.switched;
   r.hybridProbed=outcome.probed;
   r.usedGlobal=r.selected=="global_guarded_exact";
  }
  else if(kind_==Kind::GuardSapTrial){
   auto outcome=guardSapTrial_.query(boxes);
   r.pairs=std::move(outcome.pairs);
   r.selected=outcome.action;
   r.hybridSwitched=outcome.switched;
   r.hybridProbed=outcome.trialFrame;
   r.usedGlobal=r.selected=="global_guarded_exact";
  }
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
  r.ms=std::chrono::duration<double,std::milli>(backendEnd-start).count();
  if(kind_==Kind::Learned||kind_==Kind::LearnedConservative)
   r.mlWrapperMs=std::max(0.,r.ms-r.mlInnerMs);
  return r;
 }
};
void run(const std::string& filename,const std::string& output,int verbose,
         const std::string& model,std::uint32_t orderSeed=0){
 MaterialReplay header(filename);
 if(std::filesystem::exists(output))throw std::runtime_error("refusing to overwrite CPU replay results");
 std::filesystem::path file(output);
 if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
 std::ofstream csv(output);
 if(!csv)throw std::runtime_error("cannot create replay CSV");
 csv<<"backend,material,n,frame,seed,ms,pairs,pair_hash,oracle_checked,misses,"
       "builds,reuses,selective_repairs,aabb_tests,parallel_workers,online_span,online_path,"
       "chosen_backend,ml_switched,ml_bypassed,ml_feature_ms,ml_inference_ms,"
       "ml_decision_ms,ml_backend_ms,ml_bookkeeping_ms,ml_wrapper_ms,hybrid_probed,hybrid_switched";
 for(int i=0;i<ccp::learned::kInputs;++i)csv<<",f"<<i;
 csv<<",execution_slot,order_seed\n";
 csv<<std::fixed<<std::setprecision(9);
 std::vector<uint64_t> expectedHashes(header.frames,0);
 std::vector<std::size_t> expectedCounts(header.frames,0);
 std::vector<Kind> methods={Kind::Sap,Kind::PersistentSap,Kind::GuardSapProbe,Kind::GuardSapTrial,Kind::Global,Kind::Selective,
  Kind::Fixed,Kind::Adaptive,Kind::Hierarchy,Kind::Online};
#ifdef CCP_HAVE_BULLET
 methods.push_back(Kind::Bullet);
#endif
 if(!model.empty()){
  methods.push_back(Kind::Learned);
  methods.push_back(Kind::LearnedConservative);
 }
 // An explicitly seeded policy-order experiment is not allowed to rely on
 // rebuilt SAP executing first. Build one untimed, independent reference from
 // the frozen AABB stream before changing execution order. This reference
 // generation is outside every method's timed callback (including SAP).
 if(orderSeed!=0){
  MaterialReplay truth(filename);
  for(std::uint32_t frame=0;frame<header.frames;++frame){
   const auto boxes=truth.next();
   const auto pairs=ccp::live::candidates(boxes,true);
   requireSortedUnique(pairs,boxes.size());
   expectedHashes[frame]=pairHash(pairs);
   expectedCounts[frame]=pairs.size();
  }
  std::mt19937 shuffleEngine(orderSeed);
  std::shuffle(methods.begin(),methods.end(),shuffleEngine);
 }
 for(std::size_t slot=0;slot<methods.size();++slot){
  const Kind method=methods[slot];
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
    if(orderSeed==0){
     expectedHashes[frame]=hash;expectedCounts[frame]=observed.pairs.size();
    }else if(hash!=expectedHashes[frame]||
             observed.pairs.size()!=expectedCounts[frame]){
     throw std::runtime_error("Randomized SAP baseline disagrees with precomputed oracle");
    }
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
      <<observed.selected<<','<<(observed.switched?1:0)<<','<<(observed.bypassed?1:0)<<','
      <<observed.mlFeatureMs<<','<<observed.mlInferenceMs<<','
      <<observed.mlDecisionMs<<','<<observed.mlBackendMs<<','
      <<observed.mlBookkeepingMs<<','<<observed.mlWrapperMs<<','
      <<(observed.hybridProbed?1:0)<<','<<(observed.hybridSwitched?1:0);
   for(double f:observed.features)csv<<','<<f;
   csv<<','<<slot<<','<<orderSeed<<'\n';
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
  // Machine-readable executable ABI check: fail fast if an older binary is
  // accidentally used after a compile failure. No snapshots or model needed.
  if(argc==2&&std::string(argv[1])=="--capabilities"){
   std::cout<<"KYOTO_REPLAY_ABLATION_V4 sap_persistent_coherent "
               "ml_cost_aware_conservative guard_sap_measured_probe guard_sap_bounded_trial "
               "ml_bookkeeping_ms ml_wrapper_ms hybrid_probed hybrid_switched\n";
   return 0;
  }
  if(argc<3||argc>7)
   throw std::runtime_error("usage: ccp_material_replay INPUT.kmcb OUTPUT.csv [VERBOSE=1] [ML_MODEL.txt] [--order-seed N]");
  int verbose=argc>=4?std::stoi(argv[3]):1;
  if(verbose<0||verbose>2)throw std::runtime_error("verbosity must be 0..2");
  std::string model;
  std::uint32_t orderSeed=0;
  const auto parseOrderSeed=[](const char* input)->std::uint32_t {
   const std::string value(input);
   if(value.empty()||
      !std::all_of(value.begin(),value.end(),[](char c){
       return c>='0'&&c<='9';
      }))
    throw std::invalid_argument("Order seed must be decimal digits only");
   const auto parsed=std::stoull(value);
   if(parsed==0||parsed>std::numeric_limits<std::uint32_t>::max())
    throw std::out_of_range("Order seed must fit an unsigned nonzero 32-bit value");
   return std::uint32_t(parsed);
  };
  if(argc>=5&&std::string(argv[4])!="--order-seed")model=argv[4];
  if(argc==6){
   if(std::string(argv[4])!="--order-seed")
    throw std::runtime_error("Use --order-seed N after optional ML model path");
   orderSeed=parseOrderSeed(argv[5]);
  }else if(argc==7){
   if(std::string(argv[5])!="--order-seed")
    throw std::runtime_error("Expected --order-seed before seed value");
   orderSeed=parseOrderSeed(argv[6]);
  }else if(argc==5&&std::string(argv[4])=="--order-seed"){
   throw std::runtime_error("Missing order seed");
  }
  if((argc==6||argc==7)&&orderSeed==0)
   throw std::runtime_error("Randomized execution-order seed must be positive");
  run(argv[1],argv[2],verbose,model,orderSeed);
 }catch(const std::exception& e){
  std::cerr<<"MATERIAL CPU REPLAY FAILED: "<<e.what()<<'\n';return 1;
 }
}

// Standalone, reproducible CPU broad-phase scaling benchmark.
// Measures immutable 3D AABB snapshots; does not measure full cloth dynamics.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
using Clock=std::chrono::steady_clock;
struct Box {float lo[3],hi[3];};
struct Pair {uint32_t a,b; bool operator<(const Pair& r)const{return a<r.a||(a==r.a&&b<r.b);} bool operator==(const Pair& r)const{return a==r.a&&b==r.b;}};
bool overlap(const Box& a,const Box& b){
 return a.lo[0]<=b.hi[0]&&b.lo[0]<=a.hi[0]&&
        a.lo[1]<=b.hi[1]&&b.lo[1]<=a.hi[1]&&
        a.lo[2]<=b.hi[2]&&b.lo[2]<=a.hi[2];
}
struct Snapshot{std::vector<Box> boxes;};
Snapshot makeScene(size_t n,unsigned seed,int frame,const std::string& scene) {
 std::mt19937 rng(seed);
 std::uniform_real_distribution<float> random(-1.f,1.f);
 Snapshot s; s.boxes.reserve(n);
 // Each object's base position is deterministic across frames. Temporal motion
 // changes between frames, unlike regenerating random positions each frame.
 for(size_t i=0;i<n;++i){
  float x=random(rng),y=random(rng),z=random(rng);
  float half=.008f,scale=10.f;
  if(scene=="cluster"){scale=1.5f; half=.03f;}
  else if(scene=="dense"){scale=.6f;half=.07f;}
  else if(scene=="hotspot"){scale=6.f; half=.025f;
   if(i%5==0){x*=.05f;y*=.05f;z*=.05f;}
  }else if(scene=="all_overlap"){x=y=z=0.f; half=1.f;scale=1.f;}
  else if(scene!="sparse")throw std::runtime_error("unknown scene");
  const float t=float(frame)*.011f;
  if(scene!="all_overlap"){
    x+=.006f*std::sin(t+float(i%97));
    y+=.006f*std::cos(t*1.1f+float(i%83));
  }
  s.boxes.push_back({{x*scale-half,y*scale-half,z*scale-half},
                     {x*scale+half,y*scale+half,z*scale+half}});
 }
 return s;
}
using Pairs=std::vector<Pair>;
void canonical(Pairs& pairs){
 std::sort(pairs.begin(),pairs.end());
 auto last=std::unique(pairs.begin(),pairs.end());
 if(last!=pairs.end())throw std::runtime_error("duplicate candidate pairs");
}
Pairs exhaustive(const Snapshot& s){
 Pairs out;
 for(uint32_t i=0;i<s.boxes.size();++i)
  for(uint32_t j=i+1;j<s.boxes.size();++j)
   if(overlap(s.boxes[i],s.boxes[j]))out.push_back({i,j});
 return out;
}
std::vector<uint32_t> sortedX(const Snapshot& s){
 std::vector<uint32_t> order(s.boxes.size());
 std::iota(order.begin(),order.end(),0u);
 std::sort(order.begin(),order.end(),[&](uint32_t a,uint32_t b){
  return s.boxes[a].lo[0]<s.boxes[b].lo[0]||
    (s.boxes[a].lo[0]==s.boxes[b].lo[0]&&a<b);
 });
 return order;
}
// Workers operate on disjoint left-endpoint ranges. No mutex is required
// during pair generation. Results are merged deterministically afterward.
Pairs sap(const Snapshot& s,unsigned workers){
 auto order=sortedX(s);
 const size_t n=order.size();
 workers=std::max(1u,std::min(workers,unsigned(std::max(size_t(1),n))));
 std::vector<Pairs> local(workers);
 std::vector<std::thread> threads;
 auto job=[&](unsigned t){
  const size_t begin=n*t/workers,end=n*(t+1)/workers;
  auto& out=local[t];
  for(size_t k=begin;k<end;++k){
   const uint32_t i=order[k]; const Box& a=s.boxes[i];
   for(size_t q=k+1;q<n;++q){
    const uint32_t j=order[q];const Box& b=s.boxes[j];
    if(b.lo[0]>a.hi[0])break;
    if(overlap(a,b))out.push_back({std::min(i,j),std::max(i,j)});
   }
  }
 };
 if(workers==1)job(0);
 else {
  for(unsigned t=0;t<workers;++t)threads.emplace_back(job,t);
  for(auto& thread:threads)thread.join();
 }
 size_t count=0;for(auto& v:local)count+=v.size();
 Pairs result;result.reserve(count);
 for(auto& v:local)result.insert(result.end(),v.begin(),v.end());
 canonical(result);return result;
}

// Work-aware SAP: create deterministic chunks according to estimated X-axis
// candidate work, rather than giving each thread the same number of objects.
// This is a CPU optimization candidate, not the original cloth controller.
// No pair ownership overlaps: each chunk owns unique sorted left indices.
Pairs sapWorkAware(const Snapshot& s,unsigned workers){
 const auto order=sortedX(s);
 const size_t n=order.size();
 workers=std::max(1u,std::min(workers,unsigned(std::max(size_t(1),n))));
 // The sorted low-X sequence is monotone. upper_bound returns the final
 // possible X-overlap for each left endpoint, reducing load imbalance when a
 // small subset of objects sits in a dense collision hotspot.
 std::vector<size_t> cost(n),prefix(n+1,0);
 for(size_t i=0;i<n;++i){
  const float high=s.boxes[order[i]].hi[0];
  const auto end=std::upper_bound(order.begin()+i+1,order.end(),high,
   [&](float value,uint32_t id){return value<s.boxes[id].lo[0];});
  cost[i]=size_t(end-(order.begin()+i+1));
  prefix[i+1]=prefix[i]+cost[i]+1; // +1 ensures progress for sparse scenes
 }
 const size_t chunks=std::min(n,size_t(workers)*8);
 std::vector<size_t> boundaries(chunks+1,0);
 for(size_t c=1;c<chunks;++c){
  const size_t target=prefix[n]*c/chunks;
  boundaries[c]=size_t(std::lower_bound(prefix.begin(),prefix.end(),target)-prefix.begin());
 }
 boundaries[chunks]=n;
 std::vector<Pairs> local(chunks);
 std::atomic<size_t> next{0};
 auto work=[&](){
  while(true){
   const size_t chunk=next.fetch_add(1,std::memory_order_relaxed);
   if(chunk>=chunks)break;
   auto& out=local[chunk];
   for(size_t i=boundaries[chunk];i<boundaries[chunk+1];++i){
    const uint32_t a=order[i];const Box& left=s.boxes[a];
    for(size_t j=i+1;j<=i+cost[i];++j){
     const uint32_t b=order[j];const Box& right=s.boxes[b];
     if(overlap(left,right))out.push_back({std::min(a,b),std::max(a,b)});
    }
   }
  }
 };
 std::vector<std::thread> pool;
 if(workers==1)work();
 else{
  for(unsigned t=0;t<workers;++t)pool.emplace_back(work);
  for(auto& thread:pool)thread.join();
 }
 size_t total=0;for(const auto& v:local)total+=v.size();
 Pairs all;all.reserve(total);
 for(const auto& v:local)all.insert(all.end(),v.begin(),v.end());
 canonical(all);
 return all;
}

// This is a *measured CPU benchmark* of SAP, NOT the project's online policy.
// Integrating the actual adaptive regional implementation is a separate task.
struct Result{double ms;size_t pairs;};
Result timed(const Snapshot& s,unsigned threads,int runs,bool balanced=false){
 std::vector<double> samples;size_t pairs=0;
 for(int r=0;r<runs;++r){
  const auto start=Clock::now();
  const auto output=balanced?sapWorkAware(s,threads):sap(s,threads);
  const auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
  samples.push_back(elapsed);pairs=output.size();
 }
 std::sort(samples.begin(),samples.end());
 return {samples[samples.size()/2],pairs};
}
size_t positive(const std::string& arg,const char* label,size_t max){
 size_t consumed=0;unsigned long long value=0;
 try{value=std::stoull(arg,&consumed);}catch(...){throw std::runtime_error(std::string("invalid ")+label);}
 if(consumed!=arg.size()||value==0||value>max)throw std::runtime_error(std::string("out of range ")+label);
 return size_t(value);
}
void selfTest(){
 for(auto scene:{"sparse","cluster","dense","hotspot","all_overlap"})
  for(int frame=0;frame<3;++frame){
   const auto s=makeScene(96,1234,frame,scene);
   const auto truth=exhaustive(s);
   for(unsigned t:{1u,2u,3u,8u}){
    const auto actual=sap(s,t);
    if(actual!=truth)throw std::runtime_error(std::string("SAP mismatch: ")+scene);
    if(sapWorkAware(s,t)!=truth)throw std::runtime_error(std::string("work-aware SAP mismatch: ")+scene);
   }
  }
 std::cout<<"PASS: serial and threaded SAP match exhaustive pair oracle on all test scenes\n";
}
}
int main(int argc,char** argv){
 try{
  if(argc==2&&std::string(argv[1])=="--self-test"){selfTest();return 0;}
  if(argc!=7)throw std::runtime_error("usage: ccp_scaling_cpu OUTPUT.csv N FRAMES THREADS SCENE SEED | --self-test");
  const size_t n=positive(argv[2],"N",250000);
  const int frames=int(positive(argv[3],"FRAMES",10000));
  const unsigned threads=unsigned(positive(argv[4],"THREADS",256));
  const std::string scene=argv[5];
  const auto seed=unsigned(positive(argv[6],"SEED",std::numeric_limits<unsigned>::max()));
  if(scene=="all_overlap"&&n>2048)throw std::runtime_error("all_overlap safety limit N=2048");
  std::filesystem::path path(argv[1]);
  if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);if(!out)throw std::runtime_error("CSV open failed");
  out<<"backend,scene,n,frame,seed,threads,ms,candidates,oracle_checked,misses\n";
  out<<std::fixed<<std::setprecision(8);
  for(int frame=0;frame<frames;++frame){
   const auto s=makeScene(n,seed,frame,scene);
   const auto one=sap(s,1);
   const auto many=sap(s,threads);
   if(one!=many || one!=sapWorkAware(s,threads))throw std::runtime_error("parallel SAP or work-aware scheduler differs from serial SAP");
   bool checked=n<=2048&&(frame%5==0);
   if(checked&&one!=exhaustive(s))throw std::runtime_error("SAP oracle failure");
   // The simple crossover gate below avoids thread launch overhead for small
   // scenes. Its 4096-object threshold is a provisional engineering guess,
   // not an empirically fitted optimal threshold; held-out trials must tune it.
   // Repeat each measurement to reduce timer granularity effects. Timing
   // includes x-axis sorting, thread startup, pair materialization and merge.
   for(unsigned variant=0;variant<(threads==1?2u:4u);++variant){
    const bool autoPolicy=variant==3||(threads==1&&variant==1);
    const unsigned count=autoPolicy?(n<4096?1u:threads):(variant==0?1u:threads);
    const bool balanced=(variant==2)||(autoPolicy&&count>1);
    const auto measured=timed(s,count,3,balanced);
    out<<(autoPolicy?"cpu_sap_auto":balanced?"cpu_sap_work_aware":count==1?"cpu_sap_serial":"cpu_sap_threaded")<<','<<scene<<','<<n<<','<<frame<<','
       <<seed<<','<<count<<','<<measured.ms<<','<<measured.pairs<<','
       <<(checked?1:0)<<",0\n";
   }
  }
  if(!out)throw std::runtime_error("CSV write failed");
  std::cout<<"Completed CPU SAP reference timings. This is not an adaptive-controller result.\n";
 }catch(const std::exception& e){std::cerr<<"SCALING BENCHMARK FAILED: "<<e.what()<<'\n';return 1;}
}

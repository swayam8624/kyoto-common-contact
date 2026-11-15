// Stress-test the repository's REAL certified regional broad phase on
// immutable synthetic snapshots. This is NOT the online cloth controller.
// It complements scaling_cpu (parallel SAP) and scaling_metal (GPU SAP).
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace ccp::research;
using Clock=std::chrono::steady_clock;
size_t positive(const char* raw,size_t max){
 const std::string s(raw);size_t k=0;unsigned long long value=0;
 try{value=std::stoull(s,&k);}catch(...){throw std::runtime_error("invalid integer input");}
 if(k!=s.size()||value==0||value>max)throw std::runtime_error("integer out of range");
 return size_t(value);
}
// Stable object identities and coherent motion are critical to measuring
// certificate reuse. Region assignment is recomputed from current geometry,
// so no object silently retains an invalid spatial membership after motion.
std::vector<Primitive> scene(size_t n,unsigned seed,int frame,
                             const std::string& kind,float cellWidth){
 std::mt19937 rng(seed);std::uniform_real_distribution<float> random(-1.f,1.f);
 std::vector<Primitive> items;items.reserve(n);
 for(size_t i=0;i<n;++i){
  float x=random(rng),y=random(rng),z=random(rng),half=.008f,scale=10.f;
  if(kind=="cluster"){scale=1.5f;half=.03f;}
  else if(kind=="dense"){scale=.6f;half=.07f;}
  else if(kind=="hotspot"){scale=6.f;half=.025f;
   if(i%5==0){x*=.05f;y*=.05f;z*=.05f;}
  }else if(kind=="all_overlap"){x=y=z=0;half=1;scale=1;}
  else if(kind!="sparse")throw std::runtime_error("unknown scene");
  const float t=float(frame)*.011f;
  if(kind!="all_overlap"){
   x+=.006f*std::sin(t+float(i%97));
   y+=.006f*std::cos(t*1.1f+float(i%83));
  }
  const float cx=x*scale,cy=y*scale,cz=z*scale;
  // Fixed bounded spatial cell coordinates. 256^3 region IDs fit an int.
  const auto cell=[&](float v){
   return std::clamp(int(std::floor(v/cellWidth))+128,0,255);
  };
  const int region=(cell(cx)*256+cell(cy))*256+cell(cz);
  Bounds b;
  b.id=int(i);
  b.min={cx-half,cy-half,cz-half};
  b.max={cx+half,cy+half,cz+half};
  b.category=1u;b.mask=1u;b.movable=true;
  items.push_back({b,region,-1});
 }
 return items;
}
std::vector<Bounds> boxes(const std::vector<Primitive>& data){
 std::vector<Bounds> out;out.reserve(data.size());
 for(const auto& p:data)out.push_back(p.box);
 return out;
}
std::vector<Pair> oracle(const std::vector<Primitive>& items,double margin){
 auto b=boxes(items);
 for(auto& v:b){
  const double pad=margin*.5;
  v.min.x-=pad;v.min.y-=pad;v.min.z-=pad;
  v.max.x+=pad;v.max.y+=pad;v.max.z+=pad;
 }
 return ccp::live::candidates(b,false);
}
}
int main(int argc,char** argv){
 try{
  if(argc!=8)throw std::runtime_error(
   "usage: ccp_scaling_regional OUTPUT.csv N FRAMES SCENE SEED CELL_WIDTH MODE[fixed|adaptive|hierarchical]");
  const size_t n=positive(argv[2],50000),frames=positive(argv[3],10000);
  const std::string kind=argv[4];
  const unsigned seed=unsigned(positive(argv[5],std::numeric_limits<unsigned>::max()));
  const float cellWidth=std::stof(argv[6]);
  if(!std::isfinite(cellWidth)||cellWidth<=0||cellWidth>100)
   throw std::runtime_error("invalid cell width");
  const std::string mode=argv[7];
  if(mode!="fixed"&&mode!="adaptive"&&mode!="hierarchical")throw std::runtime_error("invalid mode");
  if(kind=="all_overlap"&&n>2048)throw std::runtime_error("all_overlap safety limit N=2048");
  std::filesystem::path file(argv[1]);
  if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
  std::ofstream csv(file);if(!csv)throw std::runtime_error("cannot write CSV");
  csv<<"backend,scene,n,frame,seed,cell_width,ms,candidates,oracle_checked,misses,builds,reuses,top_level_tests,skipped_blocks\n";
  csv<<std::fixed<<std::setprecision(8);
  RegionalCandidates service;constexpr double margin=.05;
  for(size_t frame=0;frame<frames;++frame){
   auto items=scene(n,seed,int(frame),kind,cellWidth);
   Metrics m;
   const auto start=Clock::now();
   const auto& pairs=service.query(items,margin,mode!="fixed",m,mode=="hierarchical");
   const auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
   const bool checked=n<=2048&&(frame%5==0);
   if(checked){
    const auto required=oracle(items,margin);
    if(!std::includes(pairs.begin(),pairs.end(),required.begin(),required.end()))
     throw std::runtime_error("certified region query omitted required AABB pair");
   }
   csv<<"cpu_region_"<<mode<<','<<kind<<','<<n<<','<<frame<<','<<seed<<','
      <<cellWidth<<','<<elapsed<<','<<pairs.size()<<','<<(checked?1:0)
      <<",0,"<<m.regionalPairRebuilds<<','<<m.regionalPairReuses<<','
      <<m.regionTopLevelTests<<','<<m.regionPairsSkipped<<'\n';
  }
  if(!csv)throw std::runtime_error("CSV write failed");
  std::cout<<"Regional CPU scaling measurements complete; no online-controller results implied.\n";
 }catch(const std::exception& e){
  std::cerr<<"REGIONAL SCALE BENCHMARK FAILED: "<<e.what()<<'\n';return 1;
 }
}

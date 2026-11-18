// Kyoto CBP Phase 7: many truly stepped lightweight material instances.
//
// Each instance owns a DIFFERENT independent mass-spring World with actual
// cloth/sponge/cushion/soft-ball/spring vertices, bonds, damping and forcing.
// We step every World once per frame and translate its real element AABBs into
// one mixed 3D world. This creates real changing material geometry, NOT just
// animated random boxes. Importantly, NO inter-instance narrow-phase contact
// or solver coupling is applied: this is an isolated broad-phase stress input.
//
// The unchanged KYOTOM01 binary is consumed by BOTH native CPU and Apple Metal
// GPU replay. Instance ownership and preview envelopes use sidecar files.
#include "ccp/material_scenes.hpp"
#include <algorithm>
#include <array>
#include <bit>
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

namespace {
using ccp::materials::World;
namespace fs=std::filesystem;
constexpr std::array<const char*,5> kKinds={
 "cloth","sponge","cushion","soft_ball","spring"};
constexpr std::array<const char*,6> kRegimes={
 "baseline","compression","shear","impact","rapid","relax"};
struct Instance {
 std::unique_ptr<World> world;
 int id{},kind{},first{},count{};
 double x{},y{},z{};
 std::string regime;
};
int parse(const char* value,int least,int most){
 std::string text=value;std::size_t end=0;
 const auto n=std::stoll(text,&end);
 if(end!=text.size()||n<least||n>most)
  throw std::invalid_argument("integer argument outside safety range");
 return static_cast<int>(n);
}
struct Six {std::array<float,6> v{};};
void write32(std::ostream& stream,std::uint32_t value){
 stream.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
void writeBox(std::ostream& stream,const Six& box){
 stream.write(reinterpret_cast<const char*>(box.v.data()),24);
}
double origin(int component,int side,int layout,int frame,int frames,int id){
 const int x=id%side,y=(id/side)%side,z=id/(side*side);
 const double spacing=layout==0?3.8:layout==1?2.3:3.3;
 double t=double(frame)/double(std::max(1,frames-1));
 const double front=layout==2?(.95*std::clamp((t-.30)/.6,0.,1.)):0.;
 if(component==0)
  return (x-side*.5)*(spacing-front)+.13*std::sin(.075*frame+id*.31);
 if(component==1)
  return (y-side*.5)*(spacing-.4*front)+.12*std::cos(.06*frame+id*.2);
 return z*(layout==0?3.3:2.6)+.05*std::sin(.1*frame+id*.11);
}
}
int main(int argc,char** argv){
 try{
  if(argc!=6)
   throw std::invalid_argument(
    "Usage: ccp_cbp_mass_export OUTPUT_DIRECTORY INSTANCES[12..1000] "
    "FRAMES[12..240] SEED[1..INT_MAX] LAYOUT[grid|clusters|contact_front]");
  static_assert(std::endian::native==std::endian::little,
                "KYOTOM01 requires little-endian host");
  const fs::path dir(argv[1]);
  if(fs::exists(dir))
   throw std::runtime_error("Refusing to overwrite existing stress evidence directory");
  const int number=parse(argv[2],12,1000);
  const int frames=parse(argv[3],12,240);
  const std::uint32_t seed=static_cast<std::uint32_t>(
      parse(argv[4],1,std::numeric_limits<int>::max()));
  const std::string layout=argv[5];
  const int layoutCode=layout=="grid"?0:layout=="clusters"?1:
       layout=="contact_front"?2:-1;
  if(layoutCode<0)throw std::invalid_argument("Invalid 3D layout");
  const int side=static_cast<int>(std::ceil(std::cbrt(double(number))));
  std::vector<Instance> instances;
  instances.reserve(static_cast<std::size_t>(number));
  int count=0;
  for(int i=0;i<number;++i){
   const int type=(i+int(seed%5))%5;
   const std::string regime=kRegimes[static_cast<std::size_t>(
       (i/5+int(seed%6))%6)];
   auto w=std::make_unique<World>(
       kKinds[static_cast<std::size_t>(type)],16,
       seed+static_cast<std::uint32_t>(i)*9719U,regime);
   const int n=static_cast<int>(w->elements());
   if(n<=0||count>65536-n)
    throw std::runtime_error("Too many primitives for shared CPU/Metal KYOTOM01 replay");
   instances.push_back({std::move(w),i,type,count,n,0,0,0,regime});
   count+=n;
  }

  if(!fs::create_directories(dir))
   throw std::runtime_error("Cannot create experiment directory");
  std::ofstream binary(dir/"snapshots.kmcb",std::ios::binary);
  std::ofstream table(dir/"instances.csv");
  std::ofstream preview(dir/"preview.jsonl");
  std::ofstream receipt(dir/"frames.csv");
  if(!binary||!table||!preview||!receipt)
   throw std::runtime_error("Could not open stress dataset files");
  binary.write("KYOTOM01",8);
  write32(binary,static_cast<std::uint32_t>(count));
  write32(binary,static_cast<std::uint32_t>(frames));
  write32(binary,seed);
  write32(binary,5U); // Registered mixed-material code.
  table<<"id,kind,regime,first_primitive,primitive_count\n";
  for(const auto& item:instances){
   table<<item.id<<','<<kKinds[static_cast<std::size_t>(item.kind)]<<','
        <<item.regime<<','<<item.first<<','<<item.count<<'\n';
  }
  receipt<<"frame,instances,primitives,layout,seed,geometry_fnv64\n";
  preview<<std::fixed<<std::setprecision(4);
  std::vector<Six> boxes(static_cast<std::size_t>(count));
  for(int frame=0;frame<frames;++frame){
   for(auto& item:instances)item.world->step();
   std::uint64_t fingerprint=14695981039346656037ULL;
   const auto mix=[&](std::uint32_t word){
    fingerprint^=word;
    fingerprint*=1099511628211ULL;
   };
   preview<<"{\"frame\":"<<frame<<",\"instances\":[";
   for(int i=0;i<number;++i){
    auto& item=instances[static_cast<std::size_t>(i)];
    item.x=origin(0,side,layoutCode,frame,frames,i);
    item.y=origin(1,side,layoutCode,frame,frames,i);
    item.z=origin(2,side,layoutCode,frame,frames,i);
    const auto current=item.world->snapshot(16);
    if(static_cast<int>(current.size())!=item.count)
     throw std::runtime_error("Material instance changed topology mid-run");
    std::array<double,6> envelope{
     std::numeric_limits<double>::infinity(),
     std::numeric_limits<double>::infinity(),
     std::numeric_limits<double>::infinity(),
     -std::numeric_limits<double>::infinity(),
     -std::numeric_limits<double>::infinity(),
     -std::numeric_limits<double>::infinity()};
    for(int local=0;local<item.count;++local){
     const auto& box=current[static_cast<std::size_t>(local)].box;
     const std::array<double,6> xyz={
      box.min.x+item.x,box.min.y+item.y,box.min.z+item.z,
      box.max.x+item.x,box.max.y+item.y,box.max.z+item.z};
     auto& output=boxes[static_cast<std::size_t>(item.first+local)];
     for(int k=0;k<6;++k){
      if(!std::isfinite(xyz[static_cast<std::size_t>(k)]))
       throw std::runtime_error("Nonfinite material instance geometry");
      output.v[static_cast<std::size_t>(k)]=
        static_cast<float>(xyz[static_cast<std::size_t>(k)]);
      if(!std::isfinite(output.v[static_cast<std::size_t>(k)]))
       throw std::runtime_error("Float32 conversion overflowed");
     }
     for(int axis=0;axis<3;++axis){
      envelope[static_cast<std::size_t>(axis)]=
       std::min(envelope[static_cast<std::size_t>(axis)],xyz[static_cast<std::size_t>(axis)]);
      envelope[static_cast<std::size_t>(axis+3)]=
       std::max(envelope[static_cast<std::size_t>(axis+3)],xyz[static_cast<std::size_t>(axis+3)]);
     }
    }
    if(i)preview<<',';
    preview<<'['<<item.kind;
    for(double value:envelope)preview<<','<<value;
    preview<<']';
   }
   preview<<"]}\n";
   for(const auto& box:boxes){
    for(float value:box.v)
     mix(std::bit_cast<std::uint32_t>(value));
    writeBox(binary,box);
   }
   receipt<<frame<<','<<number<<','<<count<<','<<layout<<','<<seed<<','
          <<std::hex<<fingerprint<<std::dec<<'\n';
   if((frame+1)%std::max(1,frames/12)==0)
    std::cerr<<"[mass materials] frame "<<frame+1<<"/"<<frames
             <<" instances "<<number<<" primitives "<<count<<"\n";
  }
  binary.flush();table.flush();preview.flush();receipt.flush();
  if(!binary||!table||!preview||!receipt)
   throw std::runtime_error("Stress output not fully flushed");
  const auto expected=std::uintmax_t(24)+
    std::uintmax_t(frames)*std::uintmax_t(count)*24;
  binary.close();
  if(fs::file_size(dir/"snapshots.kmcb")!=expected)
   throw std::runtime_error("KYOTOM01 byte count contract violated");
  std::cout<<"PASS many-world fixture: actual independently stepped material "
           <<"worlds="<<number<<" AABB primitives="<<count
           <<" frames="<<frames<<" layout="<<layout
           <<" CPU/Metal-compatible identical Float32 snapshots\n";
 }catch(const std::exception& e){
  std::cerr<<"MANY-WORLD EXPORT FAILURE: "<<e.what()<<'\n';
  return 1;
 }
}

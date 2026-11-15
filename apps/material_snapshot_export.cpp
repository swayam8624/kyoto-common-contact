// Export immutable 3D padded AABB snapshots from the SAME deformable
// material generator used by the CPU research fixture. Float32 on disk is
// intentional: Metal and CPU replay read identical quantized coordinates.
// Binary ABI v1: 8-byte ASCII magic "KYOTOM01", then four native little-endian
// uint32 (N, frames, seed, material code), followed by N*6 Float32 each frame:
// minXYZ, maxXYZ. Stable IDs are implicit indices 0..N-1. Masks all ones.
#include "ccp/material_scenes.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace {
int number(const char* s,int minimum,int maximum){
 std::size_t end=0;long long n=0;
 try{n=std::stoll(s,&end);}catch(...){throw std::runtime_error("invalid integer");}
 if(end!=std::string(s).size()||n<minimum||n>maximum)throw std::runtime_error("integer out of range");
 return int(n);
}
void writeU32(std::ofstream& out,uint32_t n){
 out.write(reinterpret_cast<const char*>(&n),sizeof(n));
}
}
int main(int argc,char** argv){
 try{
  if(argc!=6)throw std::runtime_error("usage: ccp_material_export OUTPUT.kmcb MATERIAL ELEMENTS FRAMES SEED");
  const std::filesystem::path dest(argv[1]);
  const std::string material=argv[2];
  if(material!="mixed")ccp::materials::materialCode(material);
  const int budget=number(argv[3],16,50000);
  const int frames=number(argv[4],1,10000);
  const uint32_t seed=uint32_t(number(argv[5],1,2000000000));
  ccp::materials::World world(material,budget,seed);
  if(world.elements()>std::numeric_limits<uint32_t>::max())
   throw std::runtime_error("too many material AABBs");
  if(!dest.parent_path().empty())std::filesystem::create_directories(dest.parent_path());
  if(std::filesystem::exists(dest))throw std::runtime_error("refusing to overwrite immutable material dataset");
  std::ofstream out(dest,std::ios::binary);
  if(!out)throw std::runtime_error("cannot write material dataset");
  out.write("KYOTOM01",8);
  writeU32(out,uint32_t(world.elements()));
  writeU32(out,uint32_t(frames));
  writeU32(out,seed);
  writeU32(out,material=="mixed"?5u:uint32_t(ccp::materials::materialCode(material)));
  for(int frame=0;frame<frames;++frame){
   world.step();
   const auto scene=world.snapshot(64);
   if(scene.size()!=world.elements())throw std::runtime_error("material topology changed during replay");
   for(const auto& primitive:scene){
    const auto& a=primitive.box;
    constexpr double h=.0125; // same half-margin as material CPU benchmark
    const std::array<float,6> values={
     float(a.min.x-h),float(a.min.y-h),float(a.min.z-h),
     float(a.max.x+h),float(a.max.y+h),float(a.max.z+h)};
    for(float value:values)if(!std::isfinite(value))
     throw std::runtime_error("nonfinite float32 material envelope");
    out.write(reinterpret_cast<const char*>(values.data()),sizeof(values));
   }
   if((frame+1)%std::max(1,frames/10)==0)
    std::cerr<<"[EXPORT] "<<material<<" frame="<<frame+1<<"/"<<frames
     <<" primitives="<<scene.size()<<"\n";
  }
  out.flush();
  if(!out)throw std::runtime_error("material snapshot write failed");
  std::cout<<"[PASS] material snapshots: "<<dest<<" N="<<world.elements()
   <<" frames="<<frames<<" seed="<<seed<<"\n";
 }catch(const std::exception& e){
  std::cerr<<"MATERIAL EXPORT FAILED: "<<e.what()<<'\n';return 1;
 }
}

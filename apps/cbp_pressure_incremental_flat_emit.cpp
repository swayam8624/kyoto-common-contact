// Phase 17: materially emit FULL current-AABB pairs from the *persistent*
// flat index rather than only count/fingerprints. Output is a bounded binary
// stream, not a narrow-phase, CCD, or coupled cloth solver. The independent
// rebuilt-grid Phase 11 streamer must also agree on every audited frame.
//
// Usage: APP REPORT.csv SCENE N FRAMES SEED MAX_TESTS MAX_FRAME_MS
//            MAX_GRID_ENTRIES MAX_OUTPUT_MIB PAIR_SINK_BASE
// Outputs are immutable; transient .part files are discarded on safety stop.
#include "ccp/cbp_pressure_incremental_flat.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace ccp::pressure;
static_assert(std::endian::native == std::endian::little,
              "Pair binary ABI is little-endian uint32 a,b");
struct Pair { std::uint32_t a{}, b{}; };
static_assert(sizeof(Pair) == 8);

struct StreamStats {
 Result result{};
 std::uint64_t bytes{};
 double writeMs{}, streamMs{};
};

StreamStats writePairs(const FlatPersistentGrid& grid,
                       const fs::path& filename,
                       std::uint64_t testsCap,
                       std::uint64_t pairCap,
                       double timeCap) {
 if(fs::exists(filename) || fs::exists(filename.string()+".part"))
  throw std::runtime_error("Refusing to overwrite pair output or temp file");
 const auto temporary=fs::path(filename.string()+".part");
 StreamStats stats;
 const auto started=std::chrono::steady_clock::now();
 try {
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if(!output)throw std::runtime_error("Cannot create stream temp file");
  std::vector<Pair> buffer;
  buffer.reserve(1u<<16);
  auto flush=[&](){
   const auto began=std::chrono::steady_clock::now();
   if(!buffer.empty()){
    output.write(reinterpret_cast<const char*>(buffer.data()),
                 std::streamsize(buffer.size()*sizeof(Pair)));
    if(!output)throw std::runtime_error("Pair write failed");
    stats.bytes+=std::uint64_t(buffer.size())*sizeof(Pair);
    buffer.clear();
   }
   stats.writeMs+=since(began);
  };
  stats.result=grid.streamPairs(testsCap,pairCap,timeCap,
                              [&](std::uint32_t a,std::uint32_t b){
   buffer.push_back({a,b});
   if(buffer.size()==buffer.capacity())flush();
  });
  if(!stats.result.complete){
   output.close();
   fs::remove(temporary);
   return stats;
  }
  flush();
  const auto closeBegan=std::chrono::steady_clock::now();
  output.flush();
  if(!output)throw std::runtime_error("Pair flush failed");
  output.close();
  if(!output)throw std::runtime_error("Pair close failed");
  stats.writeMs+=since(closeBegan);
  if(stats.bytes!=stats.result.digest.count*sizeof(Pair))
   throw std::logic_error("Streamed pair byte/count disagreement");
  fs::rename(temporary,filename);
 } catch(...) {
  fs::remove(temporary);
  throw;
 }
 stats.streamMs=since(started);
 return stats;
}
} // namespace

int main(int argc,char** argv) {
 try {
  if(argc!=11)throw std::invalid_argument(
    "Usage: ccp_cbp_pressure_incremental_flat_emit REPORT.csv "
    "SCENE N FRAMES SEED MAX_TESTS MAX_FRAME_MS MAX_GRID_ENTRIES "
    "MAX_OUTPUT_MIB PAIR_SINK_BASE");
  const fs::path report=argv[1],sink=argv[10];
  const std::string scene=argv[2];
  const auto n=number(argv[3],16,2000000);
  const auto frames=number(argv[4],1,240);
  const auto seed=number(argv[5],1,2147483647);
  const auto maxTests=number(argv[6],1000,600000000);
  const auto maxMs=number(argv[7],20,120000);
  const auto entries=number(argv[8],10000,20000000);
  const auto mib=number(argv[9],1,4096);
  if(!known(scene))throw std::invalid_argument("Unknown synthetic pressure scene");
  if(fs::exists(report)||fs::exists(report.string()+".part"))
   throw std::runtime_error("Refusing to overwrite previous stream report");
  if(!report.parent_path().empty())fs::create_directories(report.parent_path());
  if(!sink.parent_path().empty())fs::create_directories(sink.parent_path());
  FlatPersistentGrid grid(entries);
  const auto pairCap=std::uint64_t(mib)*1024*1024/sizeof(Pair);
  std::ofstream out(report.string()+".part");
  if(!out)throw std::runtime_error("Cannot open stream report");
  out<<std::fixed<<std::setprecision(6);
  out<<"scene,n,frame,seed,status,sink,first_count_digest_matches,"
       "exact_candidate_pairs,real_pair_bytes,total_stream_ms,"
       "preflight_ms,index_maintenance_ms,reference_digest_ms,"
       "stream_enumeration_ms,stream_write_ms,rebuilt,changed_box_bounds,"
       "output_file\n";
  for(std::uint64_t f=0;f<frames;++f){
   const auto boxes=generate(std::size_t(n),std::uint32_t(f),
                             std::uint32_t(frames),std::uint32_t(seed),scene);
   const auto maintenance=grid.advance(boxes);
   // An independent same-grid digest is a local implementation invariant,
   // NOT the external rebuilt-grid proof (checked by the Python auditor).
   const auto reference=grid.enumerate(maxTests,double(maxMs));
   const auto prefix=scene+","+std::to_string(n)+","+
                     std::to_string(f)+","+std::to_string(seed)+",";
   if(!reference.complete||reference.digest.count>pairCap){
    out<<prefix<<"SAFETY_SKIP,file,0,"<<reference.digest.count
       <<",0,0,"<<maintenance.preflightMs<<','
       <<maintenance.maintenanceMs<<','<<reference.ms
       <<",0,0,"<<int(maintenance.rebuilt)<<','
       <<maintenance.changedBounds<<",none\n";
    std::cerr<<"[PERSISTENT STREAM] frame "<<f
             <<" skipped (count pass incomplete or pair cap)\n";
    continue;
   }
   const auto path=fs::path(sink.string()+"-frame-"+std::to_string(f)+".pairs");
   auto streamed=writePairs(grid,path,maxTests,pairCap,double(maxMs));
   if(!streamed.result.complete){
    out<<prefix<<"SAFETY_SKIP,file,0,"<<reference.digest.count
       <<",0,0,"<<maintenance.preflightMs<<','
       <<maintenance.maintenanceMs<<','<<reference.ms
       <<",0,0,"<<int(maintenance.rebuilt)<<','
       <<maintenance.changedBounds<<",none\n";
    continue;
   }
   if(streamed.result.digest!=reference.digest ||
      streamed.result.tests!=reference.tests)
    throw std::runtime_error("Persistent stream mismatched full count pass");
   if(n<=1024&&reference.digest!=brute(boxes))
    throw std::runtime_error("Persistent stream disagrees with brute AABB reference");
   out<<prefix<<"VERIFIED_WRITTEN,file,1,"
      <<streamed.result.digest.count<<','<<streamed.bytes<<','
      <<streamed.streamMs<<','<<maintenance.preflightMs<<','
      <<maintenance.maintenanceMs<<','<<reference.ms<<','
      <<std::max(0.0,streamed.streamMs-streamed.writeMs)<<','
      <<streamed.writeMs<<','<<int(maintenance.rebuilt)<<','
      <<maintenance.changedBounds<<','<<path.string()<<"\n";
   std::cerr<<"[PERSISTENT STREAM] "<<scene<<" frame "<<f+1<<'/'
            <<frames<<" output "<<streamed.result.digest.count
            <<" candidate pairs, bytes "<<streamed.bytes<<"\n";
  }
  out.flush();
  if(!out)throw std::runtime_error("Could not flush persistent stream report");
  out.close();
  fs::rename(report.string()+".part",report);
  std::cout<<"PASS persistent bounded candidate-pair file stream; "
              "verify the CSV and binary files against the independent "
              "rebuilt-grid and Phase 16 fingerprints next.\n";
 } catch(const std::exception& error) {
  std::cerr<<"PERSISTENT PAIR STREAM FAILED: "<<error.what()<<"\n";
  return 1;
 }
}

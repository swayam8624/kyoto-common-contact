// Phase 11: exact, real candidate-pair streaming pressure from 3D cells.
//
// Unlike count-only pressure, this actually creates every 8-byte stable-ID
// pair and writes it in bounded chunks to an explicit sink. /dev/null tests
// syscall/buffer throughput but NOT physical disk-write bandwidth. Supplying
// a directory base produces real binary output files; no input or report
// may be overwritten. This is STILL a synthetic AABB candidate generator,
// not a contact/CCD solver.
//
// Query validity: canonical overlap-minimum cell owns each pair uniquely.
// After streaming, pair count and BOTH 64/32-bit commutative fingerprints
// must equal the first independently run exact grid enumeration; small inputs
// also have brute + sweep oracles. No selected frame may exceed pair bytes.
#include "ccp/cbp_pressure.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
using namespace ccp::pressure;
struct Pair {std::uint32_t a{},b{};};
static_assert(sizeof(Pair)==8);
struct Work {
 Digest digest{};
 std::uint64_t tests{},bytes{};
 double enumerationMs{},writeMs{},totalMs{};
};
Work actualWrite(const std::vector<Box>& boxes,const Grid& grid,
                 const fs::path& sink,std::uint64_t maxPairs){
 const auto start=std::chrono::steady_clock::now();
 if(sink!="/dev/null"&&fs::exists(sink))
  throw std::runtime_error("Refusing to overwrite candidate pair binary file");
 std::ofstream out(sink,std::ios::binary|std::ios::out|std::ios::trunc);
 if(!out)throw std::runtime_error("Cannot open exact pair output sink");
 const std::size_t chunkSize=1u<<18;
 std::vector<Pair> buf;
 buf.reserve(chunkSize);
 Work result;
 auto flush=[&](){
  const auto began=std::chrono::steady_clock::now();
  if(!buf.empty()){
   out.write(reinterpret_cast<const char*>(buf.data()),
             std::streamsize(buf.size()*sizeof(Pair)));
   if(!out)throw std::runtime_error("Candidate output sink write failed");
   result.bytes+=buf.size()*sizeof(Pair);
   buf.clear();
  }
  result.writeMs+=since(began);
 };
 for(const auto& bin:grid.bins){
  const auto& ids=bin.ids;
  for(std::size_t i=0;i<ids.size();++i){
   const auto a=ids[i];
   for(std::size_t j=i+1;j<ids.size();++j){
    const auto b=ids[j];
    ++result.tests;
    if(!overlap(boxes[a],boxes[b])||
       !(canonical(boxes[a],boxes[b])==bin.cell))continue;
    if(result.digest.count>=maxPairs)
     throw std::runtime_error("Pair count changed beyond a validated cap");
    buf.push_back({std::min(a,b),std::max(a,b)});
    add(result.digest,a,b);
    if(buf.size()>=chunkSize)flush();
   }
  }
 }
 flush();
 const auto wrote=std::chrono::steady_clock::now();
 out.flush();
 if(!out)throw std::runtime_error("Failed to flush exact pair binary");
 out.close();
 result.writeMs+=since(wrote);
 result.totalMs=since(start);
 result.enumerationMs=result.totalMs-result.writeMs;
 if(result.bytes!=result.digest.count*sizeof(Pair))
  throw std::runtime_error("Candidate output size/digest contradiction");
 return result;
}
}
int main(int argc,char** argv){
 try{
  if(argc!=10)throw std::invalid_argument(
    "Usage: ccp_cbp_pressure_emit REPORT.csv SCENE N FRAMES SEED "
    "MAX_TESTS MAX_FRAME_MS MAX_OUTPUT_MIB SINK_BASE_OR_/dev/null");
  const fs::path report=argv[1],sinkBase=argv[9];
  const std::string scene=argv[2];
  const auto n=number(argv[3],16,2000000),
             frames=number(argv[4],1,240),
             seed=number(argv[5],1,2147483647),
             maxTests=number(argv[6],1000,1000000000),
             maxMs=number(argv[7],20,120000),
             maxPairMiB=number(argv[8],1,4096);
  if(!known(scene))throw std::invalid_argument("Invalid stream scene");
  if(fs::exists(report)||fs::exists(report.string()+".part"))
   throw std::runtime_error("Output report immutable and already exists");
  if(!report.parent_path().empty())fs::create_directories(report.parent_path());
  if(sinkBase!="/dev/null"&&!sinkBase.parent_path().empty())
   fs::create_directories(sinkBase.parent_path());
  std::ofstream csv(report.string()+".part");
  if(!csv)throw std::runtime_error("Could not create output evidence CSV");
  csv<<std::fixed<<std::setprecision(6);
  csv<<"scene,n,frame,seed,status,generate_ms,grid_build_ms,"
        "count_only_ms,enumeration_ms,write_ms,total_stream_ms,"
        "cell_pair_tests,exact_candidate_pairs,real_pair_bytes,"
        "sink,first_count_digest_matches,oracle_brute,"
        "max_pair_output_mib,output_file\n";
  std::uint64_t successes=0;
  const std::uint64_t maxBytes=maxPairMiB*1024*1024;
  for(std::uint64_t frame=0;frame<frames;++frame){
   const auto began=std::chrono::steady_clock::now();
   const auto boxes=generate(std::size_t(n),std::uint32_t(frame),
     std::uint32_t(frames),std::uint32_t(seed),scene);
   const auto genMs=since(began);
   const auto gridStarted=std::chrono::steady_clock::now();
   const auto grid=buildGrid(boxes,12000000);
   const auto gridMs=since(gridStarted);
   const auto counted=queryGrid(boxes,grid,maxTests,double(maxMs));
   const auto truthChecked=n<=1024&&counted.complete;
   if(truthChecked&&counted.digest!=brute(boxes))
    throw std::runtime_error("Independent brute reference invalid");
   const std::string prefix=scene+","+std::to_string(n)+","+
        std::to_string(frame)+","+std::to_string(seed)+",";
   if(!counted.complete ||
      counted.digest.count>maxBytes/sizeof(Pair)){
    csv<<prefix<<"SAFETY_SKIP,"<<genMs<<','<<gridMs<<','
       <<counted.ms<<",0,0,0,"<<counted.tests<<','
       <<counted.digest.count<<",0,"
       <<(sinkBase=="/dev/null"?"null_device":"file")
       <<",0,"<<int(truthChecked)<<','<<maxPairMiB<<",none\n";
    std::cerr<<"[STREAM BUDGET] frame="<<frame+1<<"/"<<frames
             <<" complete_count="<<int(counted.complete)
             <<" pair_output_bytes_would_be="<<counted.digest.count*8
             <<" cap="<<maxBytes<<"\n";
    continue;
   }
   fs::path target="/dev/null";
   if(sinkBase!="/dev/null")
    target=sinkBase.string()+"-frame-"+std::to_string(frame)+".pairs";
   const auto emitted=actualWrite(boxes,grid,target,
                                  counted.digest.count);
   if(emitted.digest!=counted.digest ||
      emitted.tests!=grid.workUpperBound)
    throw std::runtime_error(
      "Full exact streamed pair set disagrees with independent count pass");
   ++successes;
   csv<<prefix<<"VERIFIED_WRITTEN,"<<genMs<<','<<gridMs<<','
      <<counted.ms<<','<<emitted.enumerationMs<<','
      <<emitted.writeMs<<','<<emitted.totalMs<<','
      <<emitted.tests<<','<<emitted.digest.count<<','<<emitted.bytes
      <<','<<(sinkBase=="/dev/null"?"null_device":"file")
      <<",1,"<<int(truthChecked)<<','<<maxPairMiB<<','
      <<target.string()<<"\n";
   std::cerr<<"[REAL PAIR STREAM] frame "<<frame+1<<"/"<<frames
            <<" exact outputs "<<emitted.digest.count<<" pairs ("
            <<double(emitted.bytes)/1e6<<" MB)"
            <<" stream "<<emitted.totalMs<<" ms"
            <<" separated_write "<<emitted.writeMs<<" ms"
            <<" verified against count-only digest\n";
  }
  csv.flush();
  if(!csv)throw std::runtime_error("Incomplete pair streaming CSV");
  csv.close();fs::rename(report.string()+".part",report);
  std::cout<<"PASS bounded real pair materialization: streamed "
           <<successes<<'/'<<frames
           <<" frames. SAFETY_SKIP frames are NOT completed pair output. "
           <<" /dev/null does not measure persistent disk throughput.\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP PAIR STREAM FAILED: "<<e.what()<<"\n";
  return 1;
 }
}

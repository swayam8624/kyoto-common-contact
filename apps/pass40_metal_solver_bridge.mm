// Real Metal device frontend for the audited full native integration campaign.
// External candidate calls are exhaustively checked; this is correctness only.
#include "kyoto/tiled_solver_bridge_campaign.hpp"
#include "kyoto/metal_tiled_dispatch.hpp"
#include <filesystem>
#include <fstream>
namespace f=kyoto::contact::features;namespace t=kyoto::contact::gpu_tiles;namespace c=kyoto::solid::device_bridge;
using kyoto::contact::metal_dispatch::MetalDispatch;
static c::Counters count;
static thread_local MetalDispatch* device=nullptr;
static f::Result produce(const f::Mesh& m){c::need(device!=nullptr,"missing Metal dispatch");t::Work w;auto r=t::query(m,[](const auto& a,const auto& b,const auto& jobs){return device->tiled(a,b,jobs);},w);++count.calls;count.keys+=r.pairs.size();count.slots+=w.pair_slots;count.jobs+=w.jobs;count.words+=w.mask_words;return r;}
static f::Result corrupt(const f::Mesh& m){auto r=produce(m);c::need(!r.pairs.empty(),"missing-candidate control has no candidate");r.pairs.pop_back();return r;}
static f::Result broken(const f::Mesh& m){t::Work w;return t::query(m,[](const auto& a,const auto& b,const auto& jobs){auto r=device->tiled(a,b,jobs);r.pop_back();return r;},w);}
int main(int argc,char** argv){@autoreleasepool{try{
    c::need(argc==2||argc==3,"usage: OUTPUT_DIR [maximum width]");id<MTLDevice> d=MTLCreateSystemDefaultDevice();if(!d){std::cerr<<"no Metal device\n";return 77;}
    const std::filesystem::path out(argv[1]);c::need(!std::filesystem::exists(out),"output exists");std::filesystem::create_directories(out);std::ofstream csv(out/"metal_solver_bridge.csv"),meta(out/"device.txt");meta<<d.name.UTF8String<<'\n';c::need(meta.good()&&csv.good(),"receipt open failed");
    MetalDispatch gpu(d);device=&gpu;struct Reset{~Reset(){device=nullptr;}} reset;
    c::campaign(produce,corrupt,broken,count,argc==3?unsigned(std::stoul(argv[2])):64,csv);return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}}

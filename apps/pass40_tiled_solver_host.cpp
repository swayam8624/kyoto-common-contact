// Execute OpenCL C compiled to x86-64, not a GPU device.
#include "kyoto/tiled_solver_bridge_campaign.hpp"
namespace f=kyoto::contact::features;namespace g=kyoto::contact::gpu_mask;namespace t=kyoto::contact::gpu_tiles;namespace c=kyoto::solid::device_bridge;
static thread_local std::size_t wi;
std::size_t get_global_id(unsigned axis){c::need(axis==0,"invalid axis");return wi;}
extern "C" void overlap_tiles(const g::Box*,const g::Box*,const t::Job*,std::uint32_t*,const std::uint32_t*);
static c::Counters count;
static auto dispatch(const std::vector<g::Box>& a,const std::vector<g::Box>& b,const std::vector<t::Job>& jobs){const auto n=std::uint32_t(jobs.size());std::vector<std::uint32_t> out(n*32+1,0xdeadbeefu);for(wi=0;wi<out.size();++wi)overlap_tiles(a.data(),b.data(),jobs.data(),out.data(),&n);c::need(out.back()==0xdeadbeefu,"kernel wrote beyond dispatch");out.pop_back();return out;}
static f::Result produce(const f::Mesh& m){t::Work w;auto r=t::query(m,dispatch,w);++count.calls;count.keys+=r.pairs.size();count.slots+=w.pair_slots;count.jobs+=w.jobs;count.words+=w.mask_words;return r;}
static f::Result corrupt(const f::Mesh& m){auto r=produce(m);c::need(!r.pairs.empty(),"missing-candidate control has no candidate");r.pairs.pop_back();return r;}
static f::Result broken(const f::Mesh& m){t::Work w;return t::query(m,[](const auto& a,const auto& b,const auto& jobs){auto r=dispatch(a,b,jobs);r.pop_back();return r;},w);}
int main(int argc,char** argv){try{c::need(argc<=2,"usage: [maximum width]");c::campaign(produce,corrupt,broken,count,argc==2?unsigned(std::stoul(argv[1])):64,std::cout);return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

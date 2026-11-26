// Execute the actual shared OpenCL kernel compiled to host-native code.
// get_global_id is provided by this serial work-item harness. NOT device timing.
#include "kyoto/gpu_packed_mask.hpp"
#include "kyoto/physical_scaling_snapshot.hpp"
#include <iomanip>
#include <iostream>
#include <random>
namespace f=kyoto::contact::features;
namespace g=kyoto::contact::gpu_mask;
namespace s=kyoto::solid;
static thread_local std::size_t work_id;
std::size_t get_global_id(unsigned axis){if(axis!=0)throw std::runtime_error("unexpected work dimension");return work_id;}
extern "C" void overlap_packed(const g::Box*,const g::Box*,std::uint32_t*,const g::Params*);
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Counts{std::size_t slots{},words{},dispatches{},positives{};};
static Counts check(const f::Mesh& m,std::uint32_t chunk){
    Counts c;
    auto dispatch=[&](const std::vector<g::Box>& a,const std::vector<g::Box>& b,g::Params p){
        const auto n=g::words(p.count);std::vector<std::uint32_t> result(n+1,0xdeadbeefu);
        for(work_id=0;work_id<=n;++work_id)overlap_packed(a.data(),b.data(),result.data(),&p);
        need(result.back()==0xdeadbeefu,"out-of-range work item wrote mask");result.pop_back();
        for(std::uint32_t i=0;i<n;++i){need(result[i]==g::packed_word(a.data(),b.data(),p,i),"kernel and CPU word mismatch");c.positives+=std::popcount(result[i]);}
        c.slots+=p.count;c.words+=n;++c.dispatches;return result;
    };
    need(g::query(m,dispatch,chunk).pairs==f::exhaustive(m).pairs,"packed GPU candidate mismatch");return c;
}
int main(){
    std::mt19937_64 rng(40202611);std::uniform_int_distribution<int> d(-8,8);Counts random;
    for(int trial=0;trial<1000;++trial){f::Mesh m;const double scale=trial%3==0?1e-40:trial%3==1?1.:16777216.;const double base=scale==16777216.?scale:0.;const double delta=scale==16777216.?.125:scale*.125;
        auto point=[&](){return kyoto::contact::Vec3{base+d(rng)*delta,d(rng)*delta,d(rng)*delta};};
        for(int i=0;i<12;++i){const auto p=point(),q=point();m.vertices.push_back({std::uint64_t(10+i),{{p,p,q,q}}});}
        for(std::uint64_t i=10;i<22;i+=2)m.edges.push_back({m.edges.size()+50,{i,i+1}});
        for(std::uint64_t i=10;i<22;i+=3)m.faces.push_back({m.faces.size()+100,{i,i+1,i+2}});
        if(trial%4==0)m.vertices[0].mask=0;
        const auto c=check(m,1u+std::uint32_t(rng()%73));random.slots+=c.slots;random.words+=c.words;random.dispatches+=c.dispatches;random.positives+=c.positives;
    }
    std::cerr<<"PASS 1000 random scenes slots="<<random.slots<<" words="<<random.words<<" dispatches="<<random.dispatches<<'\n';
    need(g::words(0)==0&&g::words(31)==1&&g::words(32)==1&&g::words(33)==2,"tail count");
    bool refused=false;try{(void)g::bound(1e300,true);}catch(const std::overflow_error&){refused=true;}need(refused,"overflow not refused");
    std::cout<<"width,regime,step,leaf,slots,mask_words,dispatches,positives,reference_bytes,packed_bytes\n";
    for(unsigned width:{1u,4u,16u,32u,64u})for(int near:{0,1}){
        auto a=s::fixtures::u_solid(width,near?.0025:.02,.1),b=s::fixtures::remote();s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;s::CoupledFEMContext warm;
        std::size_t impulses=0;
        for(int step=0;step<4;++step){std::vector<s::CoupledFEMLeaf> leaves;const auto r=s::step_coupled_fem(a,b,.00025,cfg,&leaves,&warm);need(!leaves.empty()&&leaves.size()==r.accepted_leaves,"missing source leaf");impulses+=r.self_a.impulse_updates;
            for(std::size_t i=0;i<leaves.size();++i){const auto& l=leaves[i];auto m=kyoto::contact::physical_scaling::snapshot(l.before_a,l.after_a,cfg.skin);const auto c=check(m,262144u);
                std::cout<<width<<','<<near<<','<<step<<','<<i<<','<<c.slots<<','<<c.words<<','<<c.dispatches<<','<<c.positives<<','<<4*c.slots<<','<<4*c.words<<'\n';}}
        need(near?impulses>0:impulses==0,"vacuous source regime");std::cerr<<"PASS physical width="<<width<<" near="<<near<<'\n';
    }
}

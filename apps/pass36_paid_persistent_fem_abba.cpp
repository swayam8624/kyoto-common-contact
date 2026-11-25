// Pass 36B: paid whole-macrostep ABBA measurement on the original
// connected U-solid contact workload. No trace copies in timed regions.
// Cold and warm paths use identical solver settings and geometry;
// every resulting state is compared. CSV stores raw times, not claims.
#include "kyoto/connected_self_contact_fixture.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace kyoto::solid;
using Clock=std::chrono::steady_clock;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void same(const Body& a,const Body& b){
    need(a.nodes.size()==b.nodes.size()&&a.time_seconds==b.time_seconds&&
         a.normal_impulse_step==b.normal_impulse_step&&
         a.contact_count_step==b.contact_count_step,"benchmark state metadata diverged");
    for(std::size_t i=0;i<a.nodes.size();++i){
        need(norm(a.nodes[i].x-b.nodes[i].x)==0.&&norm(a.nodes[i].v-b.nodes[i].v)==0.&&
             a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,
             "benchmark physical state diverged");
    }
}
struct Run {
    Body a=fixtures::u_solid(),b=fixtures::remote();
    CoupledFEMContext context;
    double ns{};
    std::size_t leaves{},impulses{},rejects{},certificates{};
};
static void advance(Run& r,bool warm,const CoupledFEMSettings& cfg){
    const auto begin=Clock::now();
    const auto s=step_coupled_fem(r.a,r.b,.00025,cfg,nullptr,warm?&r.context:nullptr);
    const auto end=Clock::now();
    r.ns+=std::chrono::duration<double,std::nano>(end-begin).count();
    r.leaves+=s.accepted_leaves;r.impulses+=s.self_a.impulse_updates;
    r.rejects+=s.rejected_geometry+s.rejected_tets;
    r.certificates+=s.self_a.certificate_tests+s.self_b.certificate_tests;
}
int main(int argc,char** argv){
    try{
        if(argc!=2)throw std::invalid_argument("usage: pass36 benchmark OUTPUT_DIR");
        std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
        std::ofstream out(dir/"native_paid_cold_warm_abba.csv");
        if(!out)throw std::runtime_error("cannot open benchmark CSV");
        out<<std::setprecision(17)
           <<"block,slot,arm,macrosteps,total_step_ns,accepted_leaves,self_impulses,rejections,certificates,mapping_rebuilds,hierarchy_rebuilds,hierarchy_refits\n";
        CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;
        constexpr int blocks=5,steps=8;
        // Independent runs. Alternate ABBA and BAAB by block to
        // reduce fixed ordering effects. Warm means across macrosteps
        // of THIS trajectory, not across repetitions or scenes.
        for(int block=-1;block<blocks;++block){
            std::array<Run,4> runs;
            const bool flip=block%2==1;
            const std::array<bool,4> arms{flip,!flip,!flip,flip};
            for(int step=0;step<steps;++step){
                for(int slot=0;slot<4;++slot)advance(runs[slot],arms[slot],cfg);
                for(int slot=1;slot<4;++slot){
                    same(runs[0].a,runs[slot].a);same(runs[0].b,runs[slot].b);
                    need(runs[0].leaves==runs[slot].leaves&&runs[0].impulses==runs[slot].impulses&&
                         runs[0].rejects==runs[slot].rejects&&runs[0].certificates==runs[slot].certificates,
                         "benchmark physical receipts diverged");
                }
            }
            for(int slot=0;slot<4;++slot){
                const auto& r=runs[slot];
                need(r.ns>0&&r.impulses>0,"benchmark lacked paid time or contact");
                if(arms[slot])need(r.context.hierarchy_rebuilds()==2&&r.context.mapping_rebuilds()==2&&
                                   r.context.hierarchy_refits()>0,"warm benchmark failed reuse gate");
                if(block<0)continue; // discard entire warmup block
                out<<block<<','<<slot<<','<<(arms[slot]?"warm":"cold")<<','<<steps<<','
                   <<r.ns<<','<<r.leaves<<','<<r.impulses<<','<<r.rejects<<','<<r.certificates<<',';
                if(arms[slot])out<<r.context.mapping_rebuilds()<<','<<r.context.hierarchy_rebuilds()
                                 <<','<<r.context.hierarchy_refits();
                else out<<",,"; // cold local counters unavailable, not zero
                out<<'\n';
            }
        }
        need(out.good(),"paid benchmark receipt write failed");
        std::cout<<"PASS36B raw paid native cold/warm ABBA blocks="<<blocks
                 <<" steps_per_trajectory="<<steps<<" physical parity required\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

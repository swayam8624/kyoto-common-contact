// Paid full native macrostep comparison of equivalent interval products.
// Each arm starts from the same physical state, maintains its own context,
// and is checked against the other arms after every actual macrostep.
#include "kyoto/connected_self_contact_fixture.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
using namespace kyoto::solid;
using Clock=std::chrono::steady_clock;
static void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static void same(const Body& a,const Body& b){
    need(a.nodes.size()==b.nodes.size()&&a.time_seconds==b.time_seconds&&a.normal_impulse_step==b.normal_impulse_step&&a.contact_count_step==b.contact_count_step,"physical metadata mismatch");
    for(std::size_t i=0;i<a.nodes.size();++i){const auto& x=a.nodes[i];const auto& y=b.nodes[i];need(norm(x.x-y.x)==0.&&norm(x.v-y.v)==0.&&x.last_normal_impulse==y.last_normal_impulse,"physical state mismatch");}
}
struct Run{Body a,b;CoupledFEMContext context;double ns{};std::size_t leaves{},impulses{},rejects{},certificates{};};
int main(){
    std::cout<<std::setprecision(17)<<"width,regime,block,slot,arm,macrosteps,elapsed_ns,leaves,self_impulses,rejections,certificates\n";
    for(unsigned w:{1u,4u,16u,32u,64u})for(int near:{0,1}){
        for(int block=-1;block<3;++block){
            std::array<Run,4> runs;
            for(auto& r:runs){r.a=fixtures::u_solid(w,near?.0025:.02,.1);r.b=fixtures::remote();}
            for(int step=0;step<4;++step){
                for(int slot=0;slot<4;++slot){auto& r=runs[slot];CoupledFEMSettings c;c.typed_self_contact=c.hierarchy=true;c.fast_relative_projection=(slot==0||slot==3)==(block%2==0);
                    const auto start=Clock::now();const auto v=step_coupled_fem(r.a,r.b,.00025,c,nullptr,&r.context);const auto stop=Clock::now();
                    r.ns+=std::chrono::duration<double,std::nano>(stop-start).count();r.leaves+=v.accepted_leaves;r.impulses+=v.self_a.impulse_updates;r.rejects+=v.rejected_geometry+v.rejected_tets;r.certificates+=v.self_a.certificate_tests+v.self_b.certificate_tests;
                }
                for(int slot=1;slot<4;++slot){const auto& a=runs[0];const auto& b=runs[slot];same(a.a,b.a);same(a.b,b.b);need(a.leaves==b.leaves&&a.impulses==b.impulses&&a.rejects==b.rejects&&a.certificates==b.certificates,"physical decision mismatch");}
            }
            for(int slot=0;slot<4;++slot){const auto& r=runs[slot];need((near?r.impulses>0:r.impulses==0)&&r.context.mapping_rebuilds()==2&&r.context.hierarchy_rebuilds()==2&&r.context.hierarchy_refits()>0,"vacuous solver comparison");if(block>=0)std::cout<<w<<','<<(near?"near":"separated")<<','<<block<<','<<slot<<','<<(((slot==0||slot==3)==(block%2==0))?"fast_projection":"reference")<<",4,"<<r.ns<<','<<r.leaves<<','<<r.impulses<<','<<r.rejects<<','<<r.certificates<<'\n';}
        }
        std::cerr<<"PASS width="<<w<<" near="<<near<<" whole-solver state parity\n";
    }
}

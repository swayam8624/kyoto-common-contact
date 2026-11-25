// Pass 36A: full physical parity across macrosteps, mapping epochs,
// cache lifetime and failed macrostep recovery. No timings are used
// to infer speedups from rebuild/refit counts.
#include "kyoto/connected_self_contact_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void same(const Body& a,const Body& b){
    need(a.time_seconds==b.time_seconds&&a.nodes.size()==b.nodes.size()&&
         a.normal_impulse_step==b.normal_impulse_step&&
         a.contact_count_step==b.contact_count_step,"physical metadata changed");
    for(std::size_t i=0;i<a.nodes.size();++i)
        need(norm(a.nodes[i].x-b.nodes[i].x)==0.&&norm(a.nodes[i].v-b.nodes[i].v)==0.&&
             a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,
             "persistent candidate state changed physical state");
}
int main(){
    try{
        Body a=fixtures::u_solid(),b=fixtures::remote(),aa=a,bb=b;
        CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;
        CoupledFEMContext warm;
        std::size_t impulses=0,leaves=0;
        for(int i=0;i<8;++i){
            std::vector<CoupledFEMLeaf> xtrace,ytrace;
            const auto x=step_coupled_fem(a,b,.00025,cfg,&xtrace);
            const auto y=step_coupled_fem(aa,bb,.00025,cfg,&ytrace,&warm);
            same(a,aa);same(b,bb);
            need(x.accepted_leaves==y.accepted_leaves&&x.rejected_geometry==y.rejected_geometry&&
                 x.rejected_tets==y.rejected_tets&&x.certified_min_j==y.certified_min_j&&
                 x.self_a.normal_impulse==y.self_a.normal_impulse&&
                 x.self_a.contact_work==y.self_a.contact_work&&
                 x.self_a.certificate_tests==y.self_a.certificate_tests&&
                 xtrace.size()==ytrace.size(),"persistent physical receipts changed");
            for(std::size_t k=0;k<xtrace.size();++k){
                need(xtrace[k].dt==ytrace[k].dt&&xtrace[k].depth==ytrace[k].depth,
                     "persistent physical subdivision changed");
                same(xtrace[k].after_a,ytrace[k].after_a);same(xtrace[k].after_b,ytrace[k].after_b);
            }
            impulses+=y.self_a.impulse_updates;leaves+=y.accepted_leaves;
            need(warm.mapping_rebuilds()==2&&warm.hierarchy_rebuilds()==2,
                 "unchanged physical topology rebuilt");
        }
        need(impulses>0&&warm.hierarchy_refits()>16,"persistent workload did not exercise reuse");
        const auto refits=warm.hierarchy_refits();
        // Reordering face rows changes explicit topology identity even
        // when the geometry does not move. No stale dense row indices.
        TypedSelfAuditor audit;
        auto s=fixtures::u_solid();
        const auto before=audit.candidates(s,s,cfg.skin);
        std::reverse(s.surface_faces.begin(),s.surface_faces.end());
        const auto after=audit.candidates(s,s,cfg.skin);
        const auto truth=audit.candidates(s,s,cfg.skin,true);
        need(after.keys==truth.keys&&audit.mapping_rebuilds()==2&&audit.hierarchy_rebuilds()==2,
             "face row reorder did not invalidate mapping and tree");
        // Initial cross-body overlap fails after context is bound.
        auto bad_a=aa,bad_b=aa;const auto copy_a=bad_a,copy_b=bad_b;
        std::vector<CoupledFEMLeaf> trace(1);trace[0].dt=-1.;
        bool rejected=false;
        try{(void)step_coupled_fem(bad_a,bad_b,.00025,cfg,&trace,&warm);}
        catch(const std::runtime_error&){rejected=true;}
        need(rejected&&warm.mapping_rebuilds()==0&&warm.hierarchy_rebuilds()==0&&
             trace.size()==1&&trace[0].dt==-1.,"failed physical transaction retained caches or trace");
        same(bad_a,copy_a);same(bad_b,copy_b);
        const auto x=step_coupled_fem(a,b,.00025,cfg);
        const auto y=step_coupled_fem(aa,bb,.00025,cfg,nullptr,&warm);
        same(a,aa);same(b,bb);
        need(x.accepted_leaves==y.accepted_leaves&&warm.mapping_rebuilds()==2&&
             warm.hierarchy_rebuilds()==2,"recovered context failed cold initialization");
        std::cout<<"PASS36 persistent physical leaves="<<leaves<<" self_impulses="<<impulses
                 <<" previous_refits="<<refits<<" stable_mapping_rebuilds=2"
                 <<" reordered topology and failed transaction recovered\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

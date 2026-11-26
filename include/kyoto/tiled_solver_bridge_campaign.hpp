#pragma once
// Physical integration test, not performance: the external producer is always
// checked against exhaustive canonical candidates before physical response.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/tiled_gpu_mask.hpp"
#include <iostream>
#include <string_view>
namespace kyoto::solid::device_bridge {
namespace f=kyoto::contact::features;
namespace t=kyoto::contact::gpu_tiles;
using Producer=kyoto::contact::atlas::ExternalCandidateProducer;
struct Counters {std::uint64_t calls{},slots{},jobs{},words{},keys{};};
inline void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
inline bool equal(V3 a,V3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
inline void same(const Body& a,const Body& b){
    need(a.nodes.size()==b.nodes.size()&&a.time_seconds==b.time_seconds&&a.normal_impulse_step==b.normal_impulse_step&&a.contact_count_step==b.contact_count_step,"physical metadata mismatch");
    for(std::size_t i=0;i<a.nodes.size();++i){const auto& x=a.nodes[i];const auto& y=b.nodes[i];need(equal(x.x,y.x)&&equal(x.v,y.v)&&x.last_normal_impulse==y.last_normal_impulse,"physical state mismatch");}
}
inline void same_self(const SelfContactStats& a,const SelfContactStats& b){need(a.certificate_tests==b.certificate_tests&&a.interval_nodes==b.interval_nodes&&a.projections==b.projections&&a.impulse_updates==b.impulse_updates&&a.normal_impulse==b.normal_impulse&&a.contact_work==b.contact_work,"self receipt mismatch");}
inline void same_receipt(const CoupledFEMStats& a,const CoupledFEMStats& b){
    need(a.integrated_dt==b.integrated_dt&&a.normal_impulse==b.normal_impulse&&a.contact_kinetic_change==b.contact_kinetic_change&&a.accepted_leaves==b.accepted_leaves&&a.rejected_geometry==b.rejected_geometry&&a.rejected_tets==b.rejected_tets&&a.impulse_updates==b.impulse_updates&&a.certificate_tests==b.certificate_tests&&a.interval_nodes==b.interval_nodes&&a.orientation_nodes==b.orientation_nodes&&a.min_j==b.min_j&&a.certified_min_j==b.certified_min_j&&equal(a.impulse_on_a,b.impulse_on_a),"physical decision mismatch");same_self(a.self_a,b.self_a);same_self(a.self_b,b.self_b);
}
inline V3 rotate(V3 p){return {(.6*p.x-.8*p.z)*.8-.6*p.y,(.6*p.x-.8*p.z)*.6+.8*p.y,.8*p.x+.6*p.z};}
inline void campaign(Producer backend,Producer corrupt,Producer broken_dispatch,Counters& count,unsigned max_width,std::ostream& csv){
    need(backend&&corrupt&&broken_dispatch,"missing backend");need(max_width==1||max_width==4||max_width==16||max_width==32||max_width==64,"invalid width");
    CoupledFEMSettings reference;reference.hierarchy=reference.typed_self_contact=reference.cached_self_sap=reference.spatial_first_self_sap=reference.fused_self_sap=true;
    CoupledFEMSettings external;external.hierarchy=external.typed_self_contact=true;external.external_self_candidates=backend;
    csv<<"width,near,rotated,step,leaves,self_impulses,calls,keys,tile_slots,tile_jobs,mask_words\n";
    auto scene=[&](unsigned width,bool near,bool rotated,int steps){
        auto a=fixtures::u_solid(width,near?.0025:.02,.1),b=fixtures::remote();if(rotated)for(auto* body:{&a,&b})for(auto& n:body->nodes){n.x=rotate(n.x);n.v=rotate(n.v);}
        auto aa=a,bb=b;CoupledFEMContext ca,cb;std::size_t impacts=0;
        for(int step=0;step<steps;++step){const auto old=count;std::vector<CoupledFEMLeaf> x,y;const auto r=step_coupled_fem(a,b,.00025,reference,&x,&ca);const auto s=step_coupled_fem(aa,bb,.00025,external,&y,&cb);
            same(a,aa);same(b,bb);same_receipt(r,s);need(!x.empty()&&x.size()==y.size()&&x.size()==r.accepted_leaves&&count.calls>old.calls,"vacuous external execution");
            for(std::size_t i=0;i<x.size();++i){const auto& l=x[i];const auto& k=y[i];need(l.dt==k.dt&&l.depth==k.depth,"leaf subdivision mismatch");same(l.before_a,k.before_a);same(l.before_b,k.before_b);same(l.after_a,k.after_a);same(l.after_b,k.after_b);same_receipt(l.receipt,k.receipt);}
            impacts+=r.self_a.impulse_updates;csv<<width<<','<<near<<','<<rotated<<','<<step<<','<<r.accepted_leaves<<','<<r.self_a.impulse_updates<<','<<count.calls-old.calls<<','<<count.keys-old.keys<<','<<count.slots-old.slots<<','<<count.jobs-old.jobs<<','<<count.words-old.words<<'\n';
        }
        need(near?impacts>0:impacts==0,"vacuous physical regime");std::cerr<<"PASS width="<<width<<" near="<<near<<" rotated="<<rotated<<" steps="<<steps<<" impacts="<<impacts<<'\n';
    };
    for(unsigned w:{1u,4u,16u,32u,64u})if(w<=max_width)for(bool near:{false,true})scene(w,near,false,4);
    if(max_width>=4){scene(4,true,true,8);scene(4,false,true,8);}
    auto a=fixtures::u_solid(),b=fixtures::remote(),aa=a,bb=b;CoupledFEMContext ca,cb;
    (void)step_coupled_fem(a,b,.00025,reference,nullptr,&ca);(void)step_coupled_fem(aa,bb,.00025,external,nullptr,&cb);same(a,aa);same(b,bb);
    for(auto invalid:{corrupt,broken_dispatch}){const auto before_a=aa,before_b=bb;std::vector<CoupledFEMLeaf> trace(1);trace[0].dt=-1.;auto cfg=external;cfg.external_self_candidates=invalid;bool rejected=false;
        try{(void)step_coupled_fem(aa,bb,.00025,cfg,&trace,&cb);}catch(const std::runtime_error& e){rejected=std::string_view(e.what())==(invalid==corrupt?"external candidate producer differs from exhaustive contract":"tile mask size");}
        need(rejected&&trace.size()==1&&trace[0].dt==-1.&&cb.mapping_rebuilds()==0&&cb.hierarchy_rebuilds()==0,"bad backend escaped rollback");same(aa,before_a);same(bb,before_b);
        (void)step_coupled_fem(a,b,.00025,reference,nullptr,&ca);(void)step_coupled_fem(aa,bb,.00025,external,nullptr,&cb);same(a,aa);same(b,bb);
    }
    (void)step_coupled_fem(a,b,.00025,reference,nullptr,&ca);(void)step_coupled_fem(aa,bb,.00025,reference,nullptr,&cb);same(a,aa);same(b,bb);need(cb.hierarchy_rebuilds()==2,"switch to fused did not rebuild");
    (void)step_coupled_fem(a,b,.00025,reference,nullptr,&ca);(void)step_coupled_fem(aa,bb,.00025,external,nullptr,&cb);same(a,aa);same(b,bb);need(cb.mapping_rebuilds()==2,"switch to external did not rebuild");
    need(csv.good(),"receipt write failed");std::cerr<<"PASS missing candidate, malformed readback, rollback and mode switching\n";
}
} // namespace kyoto::solid::device_bridge

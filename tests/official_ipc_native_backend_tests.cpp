// Pass 35A: replace the candidate producer INSIDE native FEM with
// pinned upstream IPC LBVH; compare complete actual physical states.
// The upstream producer is audited against exhaustive candidates on
// every query, so this test is deliberately NOT a performance test.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/official_ipc_candidate_producer.hpp"
#include <iostream>
#include <stdexcept>
using namespace kyoto::solid;
namespace f=kyoto::contact::features;
namespace at=kyoto::contact::atlas;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static std::size_t calls=0;
static f::Result upstream(const f::Mesh& m){
    ++calls;return at::official_ipc_candidate_producer(m);
}
static f::Result corrupt(const f::Mesh& m){
    auto r=f::exhaustive(m);
    if(!r.pairs.empty())r.pairs.pop_back();
    else r.pairs.push_back({f::Kind::VertexFace,0,999999});
    return r;
}
static void same(const Body& a,const Body& b){
    need(a.time_seconds==b.time_seconds&&a.nodes.size()==b.nodes.size()&&
         a.contact_count_step==b.contact_count_step&&
         a.normal_impulse_step==b.normal_impulse_step,"physical metadata mismatch");
    for(std::size_t i=0;i<a.nodes.size();++i){
        need(norm(a.nodes[i].x-b.nodes[i].x)==0.&&norm(a.nodes[i].v-b.nodes[i].v)==0.&&
             a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,
             "external producer changed native physical state");
    }
}
int main(){
    try{
        Body a=fixtures::u_solid(),b=fixtures::remote(),aa=a,bb=b;
        CoupledFEMSettings native,external;
        native.typed_self_contact=external.typed_self_contact=true;
        native.hierarchy=external.hierarchy=true;
        external.external_self_candidates=upstream;
        std::size_t impulses=0,leaves=0;
        for(int i=0;i<8;++i){
            std::vector<CoupledFEMLeaf> xtrace,ytrace;
            const auto x=step_coupled_fem(a,b,.00025,native,&xtrace);
            const auto y=step_coupled_fem(aa,bb,.00025,external,&ytrace);
            same(a,aa);same(b,bb);
            need(xtrace.size()==ytrace.size()&&x.accepted_leaves==y.accepted_leaves&&
                 x.rejected_geometry==y.rejected_geometry&&x.rejected_tets==y.rejected_tets&&
                 x.self_a.impulse_updates==y.self_a.impulse_updates&&
                 x.self_a.normal_impulse==y.self_a.normal_impulse&&
                 x.self_a.contact_work==y.self_a.contact_work&&
                 x.certified_min_j==y.certified_min_j,"external producer altered physical decisions");
            for(std::size_t j=0;j<xtrace.size();++j){
                need(xtrace[j].dt==ytrace[j].dt&&xtrace[j].depth==ytrace[j].depth,
                     "external producer changed physical time subdivision");
                same(xtrace[j].before_a,ytrace[j].before_a);
                same(xtrace[j].after_a,ytrace[j].after_a);
                same(xtrace[j].before_b,ytrace[j].before_b);
                same(xtrace[j].after_b,ytrace[j].after_b);
            }
            impulses+=x.self_a.impulse_updates;leaves+=x.accepted_leaves;
        }
        need(calls>0&&impulses>0&&leaves>=8,"external integration was vacuous");
        const auto before_a=aa,before_b=bb;
        std::vector<CoupledFEMLeaf> trace(1);trace[0].dt=-1.;
        external.external_self_candidates=corrupt;
        bool rejected=false;
        try{(void)step_coupled_fem(aa,bb,.00025,external,&trace);}
        catch(const std::runtime_error&){rejected=true;}
        need(rejected&&trace.size()==1&&trace[0].dt==-1.,
             "corrupt external producer escaped transaction guard");
        same(aa,before_a);same(bb,before_b);
        std::cout<<"PASS35 external official LBVH physical parity calls="<<calls
                 <<" accepted_leaves="<<leaves<<" self_impulses="<<impulses
                 <<" corrupt producer rolled back\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

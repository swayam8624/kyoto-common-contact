// Pass 32C / 33A: a genuinely connected U-shaped tetrahedral solid.
// Its separated arms approach each other while the bottom bridge
// remains part of the same constitutive FEM domain.
#include "kyoto/connected_self_contact_fixture.hpp"
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
using kyoto::solid::fixtures::u_solid;
using kyoto::solid::fixtures::remote;
static V3 momentum(const Body& b){V3 p{};for(const auto& n:b.nodes)p+=n.v*n.mass;return p;}
static V3 angular(const Body& b){V3 p{};for(const auto& n:b.nodes)p+=cross(n.x,n.v*n.mass);return p;}
static double kinetic(const Body& b){double r=0;for(const auto& n:b.nodes)r+=.5*n.mass*dot(n.v,n.v);return r;}
static void shared_stencil(){
    Body b;
    for(V3 p:{V3{0,0,0},V3{1,0,0},V3{0,1,0}})b.nodes.push_back({p,p,{},2.,0.});
    b.nodes[1].v={-1,1,0};
    const TypedSelfStencil s{{0,1,1},{0,2,2},kyoto::contact::features::Kind::EdgeEdge};
    const auto p=momentum(b);const auto l=angular(b);const auto k=kinetic(b);
    const auto r=apply_indexed_normal_impulse(b,s,{.5,.25,.25},{.5,.25,.25},.5);
    need(r.normal_impulse>0&&std::abs(r.inverse_mass-.25)<1e-14,
         "duplicate weights or shared node effective mass wrong");
    need(norm(b.nodes[0].v)==0.&&norm(momentum(b)-p)<1e-12&&norm(angular(b)-l)<1e-12,
         "shared node cancellation or momentum failed");
    need(std::abs(kinetic(b)-k-r.predicted_kinetic_change)<1e-12,
         "shared node energy identity failed");
}
static void same(const Body& a,const Body& b){
    need(a.time_seconds==b.time_seconds&&a.nodes.size()==b.nodes.size(),"paired state metadata mismatch");
    for(std::size_t i=0;i<a.nodes.size();++i)
        need(norm(a.nodes[i].x-b.nodes[i].x)==0.&&norm(a.nodes[i].v-b.nodes[i].v)==0.&&
             a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,"paired state diverged");
}
int main(){
    try{
        shared_stencil();
        Body a=u_solid(),b=remote(),aa=a,bb=b;
        CoupledFEMSettings flat,tree;flat.typed_self_contact=tree.typed_self_contact=true;
        tree.hierarchy=true;flat.activation_width=tree.activation_width=.003;
        TypedSelfAuditor independent;
        need(independent.audit(a,a,flat.skin,12,true).separated,"connected fixture initial typed clearance failed");
        const auto p=momentum(a),l=angular(a);
        std::size_t impulses=0,certificates=0;
        double min_j=1.;
        for(int i=0;i<8;++i){
            std::vector<CoupledFEMLeaf> trace;
            const auto x=step_coupled_fem(a,b,.00025,flat,&trace);
            const auto y=step_coupled_fem(aa,bb,.00025,tree);
            same(a,aa);same(b,bb);
            need(x.self_a.impulse_updates==y.self_a.impulse_updates&&
                 x.self_a.normal_impulse==y.self_a.normal_impulse&&
                 x.self_a.contact_work==y.self_a.contact_work&&
                 x.self_a.certificate_tests==y.self_a.certificate_tests&&
                 x.rejected_geometry==y.rejected_geometry&&x.accepted_leaves==y.accepted_leaves,
                 "typed exhaustive/retained parity failed");
            need(x.impulse_updates==0&&x.certified_min_j>1e-6,"invalid connected physical leaf");
            for(const auto& leaf:trace)
                need(independent.audit(leaf.before_a,leaf.after_a,flat.skin,12,true).separated,
                     "accepted native connected path failed exhaustive typed audit");
            double nodal=0;for(const auto& n:a.nodes)nodal+=n.last_normal_impulse;
            need(std::abs(nodal-2*x.self_a.normal_impulse)<1e-9,"typed gross nodal ledger failed");
            impulses+=x.self_a.impulse_updates;certificates+=x.self_a.certificate_tests;
            min_j=std::min(min_j,x.certified_min_j);
        }
        need(impulses>0&&certificates>0,"connected arms did not exchange actual contact impulses");
        need(norm(momentum(a)-p)<1e-8&&norm(angular(a)-l)<1e-8,
             "isolated connected solid momentum balance failed");
        need(std::abs(a.time_seconds-.002)<1e-12,"connected physical clock mismatch");
        std::cout<<"PASS connected U-solid typed self contact nodes="<<a.nodes.size()
                 <<" tetrahedra="<<a.tets.size()<<" faces="<<a.surface_faces.size()
                 <<" impulses="<<impulses<<" certificates="<<certificates
                 <<" certified_min_j="<<min_j<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

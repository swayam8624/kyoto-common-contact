// Optional exact static source audit of actual accepted native FEM leaves.
// Endpoints only: no claim of continuous self-volume injectivity.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/exact_tet_intersection.hpp"
#include <iostream>
using namespace kyoto::solid;
static kyoto::geometry::ExactSelfVolumeReceipt audit(const Body& b){
    std::vector<kyoto::geometry::Point> x;
    std::vector<kyoto::geometry::ExactTetIds> cells;
    for(const auto& n:b.nodes)x.push_back({n.x.x,n.x.y,n.x.z});
    for(const auto& t:b.tets)cells.push_back(t.n);
    return kyoto::geometry::audit_exact_self_volume(x,cells);
}
int main(){
    Body a=fixtures::u_solid(),b=fixtures::remote();
    CoupledFEMSettings cfg;cfg.typed_self_contact=cfg.hierarchy=true;
    CoupledFEMContext context;
    std::size_t states=0,pairs=0,impulses=0;
    auto check=[&](const Body& x){
        const auto r=audit(x);
        if(!r.valid)throw std::runtime_error("native static self-volume intersection beyond shared simplex");
        ++states;pairs+=r.exact_pairs;
    };
    check(a);check(b);
    for(int i=0;i<8;++i){
        std::vector<CoupledFEMLeaf> trace;
        const auto r=step_coupled_fem(a,b,.00025,cfg,&trace,&context);
        impulses+=r.self_a.impulse_updates;
        if(trace.size()!=r.accepted_leaves)throw std::runtime_error("missing native accepted leaf");
        for(const auto& l:trace){check(l.after_a);check(l.after_b);}
    }
    if(!impulses||states<18||!pairs)throw std::runtime_error("vacuous native audit");
    std::cout<<"PASS exact native static endpoint audit states="<<states<<" exact_pairs="<<pairs<<" impulses="<<impulses<<'\n';
}

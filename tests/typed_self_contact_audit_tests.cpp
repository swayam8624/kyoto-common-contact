// Pass 32B: constructive adjacent-face collision missed by the
// whole-face adjacency exclusion, retained by typed VF/EE stencils.
#include "kyoto/typed_self_contact_audit.hpp"
#include "kyoto/nonincident_self_contact.hpp"
#include <iostream>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){
    try{
        Body a;
        for(V3 p:{V3{0,0,0},V3{1,0,0},V3{.25,.25,.2},V3{0,1,0},V3{9,9,9}})
            a.nodes.push_back({p,p,{0,0,0},1.,0.});
        a.surface_faces={{0,1,2},{0,1,3}};
        Body b=a;b.nodes[2].x.z=-.2;
        constexpr double skin=.001;
        SelfContactStats flat;
        need(certify_nonincident_self_sweep(a,b,skin,12,false,flat)&&flat.certificate_tests==0,
             "negative-control face filter changed its adjacency contract");
        TypedSelfAuditor audit;
        const auto start=audit.audit(a,a,skin);
        need(start.separated&&start.vertices==4&&start.edges==5&&start.faces==2,
             "valid initial surface or compact connectivity failed");
        const auto crossing=audit.audit(a,b,skin);
        need(!crossing.separated&&crossing.candidates>0&&crossing.certificates>0,
             "adjacent-face nonincident stencil crossing missed");
        const auto end=audit.audit(b,b,skin);
        need(end.separated,"separated final surface failed");
        // Co-motion of both faces must remain separated even though
        // their world-space swept boxes overlap considerably.
        auto translated=a;for(auto& n:translated.nodes)n.x+=V3{4,-3,2};
        need(audit.audit(a,translated,skin).separated,"typed co-motion failed");
        auto changed=a;changed.surface_faces={{0,1,2}};
        const auto smaller=audit.audit(changed,changed,skin);
        need(smaller.separated&&smaller.vertices==3&&smaller.edges==3,
             "typed retained topology change failed");
        std::cout<<"PASS typed adjacent-face audit candidates="<<crossing.candidates
                 <<" tested="<<crossing.certificates<<" projections="<<crossing.projections
                 <<" unresolved_kind="<<int(crossing.unresolved.kind)<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

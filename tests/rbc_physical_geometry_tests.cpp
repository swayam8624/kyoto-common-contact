// Independent geometry QA. A passed broad-phase pair hash does not
// certify physically separated deformable triangles.
//
// Old source: the deliberately overlapped mirror fold MUST remain
// diagnosed as geometrically INVALID, and unchanged in history.
// New source: a finite-radius U-turn with original constraints must
// start without a nonlocal same-sheet edge-face piercing.
//
// This does NOT solve continuous CCD or nearly-coplanar overlap.
#include "ccp/research_multicloth.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace {
using ccp::research::MultiCloth;
using ccp::research::Policy;

void require(bool b,const char* why){
    if(!b)throw std::runtime_error(why);
}
void test_prepared_initial_conditions(){
    MultiCloth old(2,2,Policy::RebuildSAP);
    old.setFixtureVariation(73013,86011);
    old.prepareFoldedInitialStateForResearch();
    const auto oldBad=old.countNonlocalSelfIntersections();
    require(oldBad>0,
       "historical deliberately overlapping fold must remain negative control");
    for(auto policy:{
        Policy::RebuildSAP,Policy::GlobalGuard,Policy::FinalBounded,
        Policy::RBCSpatialCertified,Policy::RBCFlatCertified
    }){
        MultiCloth clean(2,2,policy);
        clean.setFixtureVariation(73013,86011);
        clean.prepareRoundedNonintersectingFoldForResearch();
        require(clean.countNonlocalSelfIntersections()==0,
           "new prepared rounded fold already pierces itself");
        require(clean.positions().size()==2*13*13,
           "rounded fold changed original physical particle count");
        require(clean.triangles().size()==2*12*12*2,
           "rounded fold changed source physical topology");
        for(int frame=0;frame<24;++frame){
            clean.step();
            const auto crossing=clean.countNonlocalSelfIntersections();
            if(crossing!=0) {
                std::cerr<<"rounded fold NEW physical failure on frame "
                         <<frame<<" detected "<<crossing
                         <<" independent final-state piercing pairs\n";
                throw std::runtime_error(
                  "new cloth still self-intersects despite exact broad-phase");
            }
        }
    }
    std::cout<<"PASS: preserved old invalid fold negative; "
             <<"five real new rounded-fold policies, 24 physical frames, "
             <<"no independent current self-intersections\n";
}
} // namespace

int main(){
    try{test_prepared_initial_conditions();}
    catch(const std::exception& e){
        std::cerr<<"FAIL RBC cloth physics geometry QA: "<<e.what()<<"\n";
        return 1;
    }
}

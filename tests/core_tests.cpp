#include "ccp/core.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    const auto headOn=ccp::sphereSphereTimeOfImpact({2.0,0.0,0.0},{-2.0,0.0,0.0},1.0,1.0);
    require(headOn.has_value() && std::abs(*headOn-0.5)<1e-12,"analytic sphere TOI for head-on motion");
    require(!ccp::sphereSphereTimeOfImpact({2.0,2.0,0.0},{-2.0,0.0,0.0},1.0,1.0),"analytic sphere TOI rejects a miss");
    require(!ccp::sphereSphereTimeOfImpact({0.5,0.0,0.0},{1.0,0.0,0.0},1.0,1.0),"overlapping separating spheres do not report a new impact");

    ccp::Simulation sim;
    const auto start=sim.snapshot();
    require(start.bodies.size()==3,"elastic scene has three bodies");
    require(start.potentialPairs==3,"all possible pairs counted");
    require(start.candidates==0,"separated initial scene has no candidate pairs");
    sim.setBroadPhase("brute");
    for(int i=0;i<52;++i) sim.step();
    const auto brute=sim.snapshot();
    require(brute.frame==52,"fixed-step frame count");
    require(!brute.contacts.empty(),"head-on collision produces a contact");
    require(std::abs(brute.bodies[0].velocity.x+brute.bodies[1].velocity.x)<1e-8,"elastic response preserves momentum");
    require(std::abs(brute.bodies[0].velocity.x+1.25)<1e-8,"equal-mass elastic collision reverses first body velocity");
    require(brute.contacts.front().penetration>=0.0,"contact reports nonnegative penetration");
    const auto beforeSwitch=sim.snapshot();
    sim.setBroadPhase("sweep");
    const auto afterSwitch=sim.snapshot();
    require(beforeSwitch.bodies[0].position.x==afterSwitch.bodies[0].position.x,"algorithm switch does not move simulated bodies");
    require(beforeSwitch.bodies[0].velocity.x==afterSwitch.bodies[0].velocity.x,"algorithm switch does not reapply response");
    sim.reset("elastic"); sim.setBroadPhase("sweep");
    for(int i=0;i<52;++i) sim.step();
    const auto sweep=sim.snapshot();
    require(brute.contacts.size()==sweep.contacts.size(),"broad phases agree on contact count");
    require(brute.candidates==sweep.candidates,"broad phases agree on candidate count");
    require(brute.candidatePairs==sweep.candidatePairs,"broad phases agree on candidate identity and order");
    sim.reset("dense");
    const auto dense=sim.snapshot();
    require(dense.bodies.size()==45,"dense scene population");
    require(dense.candidates<dense.potentialPairs,"dense SAP candidates are fewer than all possible pairs");
    for(const auto& c:dense.contacts) require(c.a>=0 && c.b>c.a,"contact pair IDs are canonical");
    sim.setBroadPhase("brute");
    const auto denseBrute=sim.snapshot();
    sim.setBroadPhase("sweep");
    const auto denseSweep=sim.snapshot();
    require(denseBrute.candidatePairs==denseSweep.candidatePairs,"dense broad phases return identical candidate sets");
    sim.reset("islands");
    const auto islands=sim.snapshot();
    require(islands.bodies.size()==10,"island scene population");
    require(islands.islands.front()!=islands.islands.back(),"separated contact groups have distinct islands");
    // Island colors must reflect real contacts between previously separate
    // moving groups, not merely a preassigned static object category.
    bool observedMerge=false;
    for(int frame=0;frame<100;++frame) {
        sim.step();
        const auto state=sim.snapshot();
        // Any confirmed inter-group contact joins two graph components.
        for(const auto& contact:state.contacts)
            if((contact.a<6 && contact.b>=6)||(contact.b<6 && contact.a>=6))
                observedMerge=true;
    }
    require(observedMerge,"moving island groups form a verified cross-group contact");
    require(ccp::toJson(islands).find("\"candidatePairs\"")!=std::string::npos,"snapshot JSON contains candidates");

    ccp::Simulation discrete;
    discrete.reset("tunneling");
    discrete.setBroadPhase("brute");
    discrete.step();
    const auto missed=discrete.snapshot();
    require(missed.contacts.empty(),"discrete endpoint test misses the fast crossing");
    require(missed.bodies[0].velocity.x==110.0,"discrete miss leaves velocity unchanged");
    require(missed.bodies[0].position.x>0.0 && missed.bodies[1].position.x<0.0,"discrete bodies pass through each other");

    ccp::Simulation swept;
    swept.reset("tunneling");
    swept.setBroadPhase("sweep");
    const auto beforeQuerySwitch=swept.snapshot();
    swept.setQueryMode("swept");
    const auto preStep=swept.snapshot();
    require(beforeQuerySwitch.bodies[0].position.x==preStep.bodies[0].position.x &&
            beforeQuerySwitch.bodies[0].velocity.x==preStep.bodies[0].velocity.x,
            "query-mode switch changes inspection semantics without advancing physics");
    require(preStep.candidatePairs==std::vector<ccp::Pair>{{0,1}},"swept AABB produces the fast-motion candidate before stepping");
    swept.step();
    const auto detected=swept.snapshot();
    require(detected.queryMode=="swept","snapshot reports the selected continuous query mode");
    require(detected.contacts.size()==1,"analytic TOI records the tunneling collision");
    require(detected.contacts.front().timeOfImpact>0.0 && detected.contacts.front().timeOfImpact<1.0/120.0,"impact time lies inside the fixed step");
    require(std::abs(detected.bodies[0].velocity.x+110.0)<1e-8,"swept collision applies the elastic impulse at impact");
    require(detected.bodies[0].position.x<0.0 && detected.bodies[1].position.x>0.0,"swept response prevents passage through the contact");
    require(detected.motionPaths[0].size()>=3,"swept replay path includes the impact point");
    require(detected.freeFlightPositions[0].x>0.0,"snapshot retains the no-collision predicted endpoint for comparison");

    ccp::Simulation sweptBrute;
    sweptBrute.reset("tunneling");
    sweptBrute.setBroadPhase("brute");
    sweptBrute.setQueryMode("swept");
    sweptBrute.step();
    require(sweptBrute.snapshot().candidatePairs==detected.candidatePairs,"brute and SAP agree on swept candidate identities");

    ccp::Simulation sweptDense;
    sweptDense.reset("dense");
    sweptDense.setQueryMode("swept");
    sweptDense.step();
    require(!sweptDense.snapshot().contacts.empty(),"swept mode retains and resolves resting overlap contacts");
}

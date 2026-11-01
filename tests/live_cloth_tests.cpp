#include "ccp/live_cloth.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char* message) {
    if(!ok) throw std::runtime_error(message);
}
double distance(ccp::live::Vec3 a, ccp::live::Vec3 b) {
    const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
    return std::sqrt(x*x+y*y+z*z);
}
}

int main() {
    using namespace ccp::live;
    try {
        const std::vector<Bounds> shuffled{
            {40,{2,0,0},{3,1,1}}, {7,{0,0,0},{1,1,1}},
            {-2,{1,1,1},{2,2,2}}, {13,{8,8,8},{9,9,9}}
        };
        std::size_t bruteTests=0,sapTests=0;
        const auto brute=candidates(shuffled,false,&bruteTests);
        const auto sap=candidates(shuffled,true,&sapTests);
        require(brute==std::vector<Pair>{{-2,7},{-2,40}},"inclusive touching AABBs and canonical shuffled IDs");
        require(sap==brute,"SAP agrees with brute-force shared AABB oracle");
        require(bruteTests==6 && sapTests<=bruteTests,"reported broad-phase pair tests");
        const std::vector<Bounds> categorized{
            {20,{0,0,0},{1,1,1},1u,2u},
            {3,{0,0,0},{1,1,1},2u,1u},
            {9,{0,0,0},{1,1,1},1u,1u},
            {12,{0,0,0},{1,1,1},2u,0u}
        };
        std::size_t categorizedBruteTests=0,categorizedSapTests=0;
        const auto categorizedBrute=candidates(categorized,false,&categorizedBruteTests);
        const auto categorizedSAP=candidates(categorized,true,&categorizedSapTests);
        require(categorizedBrute==std::vector<Pair>{{3,20}},"category and mask restrict candidates to intended narrow-phase pairs");
        require(categorizedSAP==categorizedBrute,"SAP and reference modes honor identical category and mask filters");
        require(categorizedBruteTests==1 && categorizedSapTests==1,"pair tests count only category-eligible overlap predicates");
        require(candidates({},true).empty(),"empty AABB input");
        BroadPhaseService service;
        QueryMetrics serviceMetrics;
        const QueryRequest serviceRequest{0.1,0.4,UpdatePolicy::GuardedReuse};
        std::vector<Bounds> serviceBounds{
            {41,{0,0,0},{1,1,1},4u,8u,true},
            {9,{1.2,0,0},{2,1,1},8u,4u,false}
        };
        const auto& firstServicePairs=service.query(serviceBounds,serviceRequest,true,serviceMetrics);
        require(firstServicePairs==std::vector<Pair>{{9,41}},"solver-independent service returns eligible arbitrary-ID pairs");
        require(serviceMetrics.builds==1 && serviceMetrics.reuses==0 && serviceMetrics.pairTests==1,
                "first guarded service query builds one pair set");
        require(std::abs(service.envelopes()[0].min.x+0.45)<1e-12 &&
                std::abs(service.envelopes()[1].min.x-1.15)<1e-12,
                "service applies margin to all bounds and displacement budget only to movable bounds");
        const auto savedServicePairs=firstServicePairs;
        const auto testsBeforeReuse=serviceMetrics.pairTests;
        const auto& reusedServicePairs=service.query(serviceBounds,serviceRequest,true,serviceMetrics);
        require(reusedServicePairs==savedServicePairs && serviceMetrics.builds==1 && serviceMetrics.reuses==1,
                "contained arbitrary geometry reuses cached pair vector");
        require(serviceMetrics.pairTests==testsBeforeReuse,"reuse performs no new AABB pair predicates");
        (void)service.query(serviceBounds,{0.2,0.4,UpdatePolicy::GuardedReuse},true,serviceMetrics);
        require(serviceMetrics.builds==2,"margin change forces a service rebuild");
        (void)service.query(serviceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},true,serviceMetrics);
        require(serviceMetrics.builds==3,"displacement-budget change forces a service rebuild");
        const auto beforeCategoryChange=serviceMetrics.builds;
        serviceBounds[1].category=16u;
        const auto& categoryFiltered=service.query(serviceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},true,serviceMetrics);
        require(categoryFiltered.empty() && serviceMetrics.builds==beforeCategoryChange+1,
                "category metadata change rebuilds and updates output");
        serviceBounds[1].category=8u;
        const auto beforeMovableChange=serviceMetrics.builds;
        serviceBounds[1].movable=true;
        (void)service.query(serviceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},true,serviceMetrics);
        require(serviceMetrics.builds==beforeMovableChange+1,"movability metadata change forces a service rebuild");
        serviceBounds[1].movable=false;
        const auto beforeIDChange=serviceMetrics.builds;
        serviceBounds[1].id=10;
        const auto& changedIDPairs=service.query(serviceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},true,serviceMetrics);
        require(changedIDPairs==std::vector<Pair>{{10,41}} && serviceMetrics.builds==beforeIDChange+1,
                "primitive ID change rebuilds and changes pair IDs");
        serviceBounds[1].id=9;
        serviceBounds[1].mask=0u;
        const auto beforeMaskChange=serviceMetrics.builds;
        const auto& maskedOut=service.query(serviceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},true,serviceMetrics);
        require(maskedOut.empty() && serviceMetrics.builds==beforeMaskChange+1,"mask metadata change rebuilds and updates output");
        serviceBounds[1].mask=4u;
        const auto beforeBackendChange=serviceMetrics.builds;
        const auto& bruteServicePairs=service.query(serviceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},false,serviceMetrics);
        require(bruteServicePairs==savedServicePairs && serviceMetrics.builds==beforeBackendChange+1,
                "backend change invalidates service cache while preserving output");
        auto movedServiceBounds=serviceBounds;
        movedServiceBounds[0].min.x=-0.7;
        (void)service.query(movedServiceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},false,serviceMetrics);
        require(serviceMetrics.builds==beforeBackendChange+2,"movable geometry leaving its envelope rebuilds the service cache");
        auto invalidServiceBounds=serviceBounds;
        invalidServiceBounds[0].min.x=std::numeric_limits<double>::quiet_NaN();
        const auto reusesBeforeInvalid=serviceMetrics.reuses;
        bool rejectedNaN=false;
        try { (void)service.query(invalidServiceBounds,{0.2,0.4,UpdatePolicy::GuardedReuse},false,serviceMetrics); }
        catch(const std::invalid_argument&) { rejectedNaN=true; }
        require(rejectedNaN && serviceMetrics.reuses==reusesBeforeInvalid,
                "service validates nonfinite raw bounds even when a prior cache exists");
        (void)service.query(serviceBounds,{0.2,0.5,UpdatePolicy::RebuildEveryIteration},false,serviceMetrics);
        const auto buildsAfterBaseline=serviceMetrics.builds;
        (void)service.query(serviceBounds,{0.2,0.5,UpdatePolicy::RebuildEveryIteration},false,serviceMetrics);
        require(serviceMetrics.builds==buildsAfterBaseline+1,"baseline service policy always rebuilds");
        const auto beforePolicyChange=serviceMetrics.builds;
        (void)service.query(serviceBounds,{0.2,0.5,UpdatePolicy::GuardedReuse},false,serviceMetrics);
        require(serviceMetrics.builds==beforePolicyChange+1,"update-policy change invalidates service cache");
        BroadPhaseService zeroMarginService;
        QueryMetrics zeroMarginMetrics;
        const std::vector<Bounds> touchingGeometry{
            {1,{0,0,0},{1,1,1}}, {2,{1,0,0},{2,1,1}}
        };
        const auto& zeroMarginPairs=zeroMarginService.query(touchingGeometry,{0.0,0.0,UpdatePolicy::GuardedReuse},true,zeroMarginMetrics);
        require(zeroMarginPairs==std::vector<Pair>{{1,2}},"solver-independent service accepts zero margin and inclusive touching bounds");
        bool rejectedNegativeServiceMargin=false;
        try { (void)zeroMarginService.query(touchingGeometry,{-0.01,0.0,UpdatePolicy::GuardedReuse},true,zeroMarginMetrics); }
        catch(const std::invalid_argument&) { rejectedNegativeServiceMargin=true; }
        require(rejectedNegativeServiceMargin,"generic service rejects negative query margin");
        auto rejectsBounds=[](const std::vector<Bounds>& boxes) {
            try { (void)candidates(boxes,true); }
            catch(const std::invalid_argument&) { return true; }
            return false;
        };
        require(rejectsBounds({{1,{1,0,0},{0,1,1}}}),"reject inverted bounds");
        require(rejectsBounds({{1,{0,0,0},{std::numeric_limits<double>::infinity(),1,1}}}),"reject infinite bounds");
        require(rejectsBounds({{1,{0,0,0},{1,std::numeric_limits<double>::quiet_NaN(),1}}}),"reject NaN bounds");
        auto rejectsRequest=[](QueryRequest request) {
            try { ClothSimulation sim; sim.setQueryRequest(request); }
            catch(const std::invalid_argument&) { return true; }
            return false;
        };
        require(rejectsRequest({0.019,0.04,UpdatePolicy::GuardedReuse}),"reject query margin below response thickness");
        require(rejectsRequest({0.05,-0.01,UpdatePolicy::GuardedReuse}),"reject negative displacement budget");
        require(rejectsRequest({std::numeric_limits<double>::infinity(),0.04,UpdatePolicy::GuardedReuse}),"reject nonfinite query margin");

        ClothSimulation idle;
        require(idle.positions().size()==625 && idle.triangles().size()==1152,"25 by 25 live cloth topology");
        // Verify that the independent exhaustive oracle cannot affect physics.
        ClothSimulation production, verified;
        production.setOracleEnabled(false);
        require(!production.oracleEnabled() && verified.oracleEnabled(),"oracle toggle explicit");
        for(int frame=0;frame<120;++frame) {
            production.step(); verified.step();
            require(!production.stats().oracleChecked && production.stats().oracleTests==0,
                    "unchecked run must not claim zero omissions");
            require(verified.stats().oracleChecked && verified.stats().oracleMisses==0,
                    "independent oracle covers required candidate pairs");
            require(production.stats().preciseTests<=production.triangles().size()*8,
                    "candidate-driven narrow phase limits precise checks");
            for(std::size_t i=0;i<production.positions().size();++i)
                require(distance(production.positions()[i],verified.positions()[i])<1e-9,
                        "oracle cannot change the physical trajectory");
        }
        require(production.stats().diagnosticTests==production.triangles().size(),
                "final global diagnostic scan counted separately");
        require(idle.stats().frame==0,"reset starts at frame zero");

        ClothSimulation sapSim, bruteSim;
        bruteSim.setBroadPhase(false);
        for(int frame=0;frame<120;++frame) { sapSim.step(); bruteSim.step(); }
        require(sapSim.stats().frame==120 && bruteSim.stats().frame==120,"fixed-step simulations advance 120 frames");
        double maxModeDelta=0.0;
        for(std::size_t i=0;i<sapSim.positions().size();++i)
            maxModeDelta=std::max(maxModeDelta,distance(sapSim.positions()[i],bruteSim.positions()[i]));
        require(maxModeDelta<1e-9,"SAP and brute modes produce matching physical positions");
        require(sapSim.stats().oracleMisses==0,"shared padded AABBs retain every near sphere contact");
        require(sapSim.stats().proximityCount>0,"cloth reaches the sphere proximity region");
        require(sapSim.stats().minGap>=-0.03,"sphere response keeps final cloth near or outside contact surface");
        require(std::isfinite(sapSim.stats().maxStrain) && sapSim.stats().maxStrain<0.5,"XPBD constraints remain finite and bounded");

        ClothSimulation rebuildSim, guardedSim;
        rebuildSim.setQueryRequest({0.05,0.04,UpdatePolicy::RebuildEveryIteration});
        std::size_t guardedBuilds=0,guardedReuses=0,rebuildBuilds=0,rebuildReuses=0;
        double policyDelta=0.0;
        for(int frame=0;frame<120;++frame) {
            rebuildSim.step(); guardedSim.step();
            const auto& guardedStats=guardedSim.stats();
            guardedBuilds+=guardedStats.queryBuilds; guardedReuses+=guardedStats.queryReuses;
            rebuildBuilds+=rebuildSim.stats().queryBuilds; rebuildReuses+=rebuildSim.stats().queryReuses;
            require(guardedStats.oracleMisses==0 && rebuildSim.stats().oracleMisses==0,
                    "both request policies retain every near contact");
            for(std::size_t i=0;i<guardedSim.positions().size();++i)
                policyDelta=std::max(policyDelta,distance(guardedSim.positions()[i],rebuildSim.positions()[i]));
        }
        require(policyDelta<1e-9,"guarded candidate reuse preserves solver positions");
        require(guardedBuilds>0 && guardedReuses>0,"guarded policy rebuilds and reuses candidate envelopes");
        require(rebuildBuilds>guardedBuilds && rebuildReuses==0,"rebuild policy queries every solver iteration and final diagnostic");
        require(guardedSim.stats().queryBounds.size()==guardedSim.triangles().size()+1,
                "stats expose the active query envelope bounds");

        guardedSim.setQueryRequest({0.02,0.04,UpdatePolicy::GuardedReuse});
        guardedSim.step();
        require(guardedSim.stats().queryBuilds>0 && guardedSim.stats().oracleMisses==0,
                "margin change invalidates the cached query envelope");
        guardedSim.setQueryRequest({0.10,0.04,UpdatePolicy::GuardedReuse});
        guardedSim.step();
        require(guardedSim.stats().queryBuilds>0 && guardedSim.stats().oracleMisses==0,
                "larger margin rebuilds candidates and retains proximity pairs");
        guardedSim.setQueryRequest({0.05,1e-8,UpdatePolicy::GuardedReuse});
        guardedSim.reset();
        guardedSim.step();
        guardedSim.step();
        require(guardedSim.stats().queryBuilds>0 && guardedSim.stats().oracleMisses==0,
                "leaving a tiny displacement envelope triggers a guarded rebuild");

        ClothSimulation noContact, withContact;
        noContact.setCollisions(false);
        withContact.setCollisions(true);
        bool passedSphere=false;
        const Vec3 center=noContact.sphereCenter();
        for(int frame=0;frame<120;++frame) {
            noContact.step(); withContact.step();
            for(const auto p:noContact.positions())
                if(distance(p,center)<noContact.sphereRadius()) passedSphere=true;
        }
        require(passedSphere,"collision-disabled cloth passes through sphere");
        bool stayedOut=true;
        const auto& p=withContact.positions(); const auto& tris=withContact.triangles();
        for(const auto& t:tris) {
            const Vec3 q{(p[t[0]].x+p[t[1]].x+p[t[2]].x)/3.0,
                         (p[t[0]].y+p[t[1]].y+p[t[2]].y)/3.0,
                         (p[t[0]].z+p[t[1]].z+p[t[2]].z)/3.0};
            if(distance(q,center)<withContact.sphereRadius()-0.03) { stayedOut=false; break; }
        }
        require(stayedOut,"collision-enabled cloth triangle centroids do not deeply penetrate sphere");

        ClothSimulation repeat;
        for(int frame=0;frame<120;++frame) repeat.step();
        double repeatDelta=0.0;
        for(std::size_t i=0;i<repeat.positions().size();++i)
            repeatDelta=std::max(repeatDelta,distance(repeat.positions()[i],sapSim.positions()[i]));
        require(repeatDelta<1e-12,"fixed-step cloth simulation is repeatable");

        ClothSimulation longRun;
        double lowestGap=std::numeric_limits<double>::infinity(),worstStrain=0.0;
        for(int frame=0;frame<1000;++frame) {
            longRun.step();
            const auto& s=longRun.stats();
            lowestGap=std::min(lowestGap,s.minGap);
            worstStrain=std::max(worstStrain,s.maxStrain);
            require(std::isfinite(s.minGap) && std::isfinite(s.maxStrain),"long run reports finite gap and strain");
            require(std::isfinite(s.stepMs) && s.stepMs>=0.0,"step timing includes a finite nonnegative full-step duration");
            require(s.oracleMisses==0,"long-run per-iteration and final proximity oracle has no misses");
            for(const auto v:longRun.positions())
                require(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z),"long-run particle positions remain finite");
        }
        require(longRun.stats().frame==1000,"long-run reaches 1000 frames");
        require(lowestGap>=-0.04,"long-run sphere penetration remains within tolerance");
        require(worstStrain<0.75,"long-run XPBD strain remains bounded");
        std::cout<<"live_cloth_tests: all checks passed (max mode delta "<<maxModeDelta
                 <<", guarded builds/reuses "<<guardedBuilds<<'/'<<guardedReuses
                 <<", rebuild-mode builds/reuses "<<rebuildBuilds<<'/'<<rebuildReuses
                 <<", policy position delta "<<policyDelta
                 <<", 1000-frame min gap "<<lowestGap<<", worst strain "<<worstStrain<<")\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"live_cloth_tests: FAILED: "<<e.what()<<'\n';
        return 1;
    }
}

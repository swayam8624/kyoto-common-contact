// RBC / Kyoto physical contact solver tournament, opt-in new experiment.
//
// Four REAL policies execute the SAME seeded XPBD-style solver with
// identical geometry, constraints, forcing and contact projection:
// rebuilt SAP, original global Guard, frozen V4 and NEW RBC indexed frontier.
//
// Every completed frame checks full sorted candidate IDs and physical
// particle state against independently stepped SAP. Optional strict mode
// checks EACH of RBC's five solver queries against an independent exact
// oracle (and incurs additional full-step verification overhead).
//
// Important: experiment runs interleaved but is NOT repeat-balanced ABBA;
// solver step_ms includes all constraints, narrow phase and optional oracle.
// Natural folded self-contact / CCD remain independent research gates.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using ccp::research::MultiCloth;
using ccp::research::Policy;
using Vec3=ccp::research::Vec3;
constexpr std::array<const char*,4> names{
    "sap_rebuild","global_guard","frozen_v4","rbc_indexed"
};
constexpr std::array<Policy,4> policies{
    Policy::RebuildSAP, Policy::GlobalGuard, Policy::FinalBounded,
    Policy::RBCSpatialCertified
};
int integer(const char* text,int min,int max){
    std::size_t pos=0;long long value=0;
    try{value=std::stoll(text,&pos);}
    catch(...){throw std::invalid_argument("invalid integer argument");}
    if(pos!=std::string(text).size()||value<min||value>max)
        throw std::invalid_argument("integer argument outside range");
    return int(value);
}
Vec3 pick(const MultiCloth& sim,int triangle) {
    const auto t=sim.triangles().at(std::size_t(triangle));
    const auto& p=sim.positions();
    return {(p[t[0]].x+p[t[1]].x+p[t[2]].x)/3.,
            (p[t[0]].y+p[t[1]].y+p[t[2]].y)/3.,
            (p[t[0]].z+p[t[1]].z+p[t[2]].z)/3.};
}
double distance(Vec3 a,Vec3 b){
    const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
    return std::sqrt(x*x+y*y+z*z);
}
void drive(MultiCloth& sim,const std::string& scenario,
           Vec3 origin,int frame,int total){
    if(scenario=="free")return;
    const double fraction=double(frame+1)/double(total);
    constexpr double pi=3.141592653589793;
    Vec3 target=origin;
    if(scenario=="pull"){
        target.x+=.26*std::sin(pi*fraction);
        target.y+=.09*std::sin(2*pi*fraction);
        target.z+=.36*std::sin(pi*fraction);
    }else if(scenario=="fold"){
        target.x-=1.35*fraction;
        target.y+=.46*std::sin(pi*fraction);
        target.z-=.55*fraction;
    }else throw std::invalid_argument("unknown coupled drive");
    sim.updateGrab(target);
}
struct RestQuality {
    std::vector<std::pair<int,int>> edges;
    std::vector<double> rest;
    explicit RestQuality(const MultiCloth& sim){
        const auto& p=sim.positions();
        std::set<std::pair<int,int>> seen;
        for(const auto& tri:sim.triangles())
            for(int i=0;i<3;++i)
                seen.emplace(std::min(tri[i],tri[(i+1)%3]),
                             std::max(tri[i],tri[(i+1)%3]));
        edges.assign(seen.begin(),seen.end());
        rest.reserve(edges.size());
        for(const auto [a,b]:edges)
            rest.push_back(distance(p[a],p[b]));
    }
    double maxStretch(const MultiCloth& sim) const {
        const auto& p=sim.positions();
        double peak=0.;
        for(std::size_t k=0;k<edges.size();++k) {
            if(rest[k]<=1e-12)continue;
            const auto [a,b]=edges[k];
            peak=std::max(peak,distance(p[a],p[b])/rest[k]);
        }
        if(!std::isfinite(peak))
            throw std::runtime_error("nonfinite measured physical stretch");
        return peak;
    }
};
void execute(const char* destination,int frames,
             const std::string& scenario,int geo,int topo,
             bool strict){
    if(std::filesystem::exists(destination))
        throw std::runtime_error(
            "refusing to overwrite original RBC solver experiment");
    std::ofstream out(destination);
    if(!out)throw std::runtime_error("cannot create RBC coupled CSV");
    out<<std::fixed<<std::setprecision(11);
    out<<"frame,policy,scenario,geometry_seed,topology_seed,"
          "strict_audit,execution_slot,step_ms,broad_ms,narrow_ms,"
          "candidate_pairs,precise_tests,self_contacts,sphere_contacts,"
          "self_crossings,velocity_clamps,max_stretch,max_position_diff_sap,"
          "oracle_misses,oracle_checked,"
          "rbc_queries,rbc_index_builds,rbc_local_repairs,"
          "rbc_potential_pairs_sum,rbc_index_comparisons,"
          "rbc_candidate_checks,rbc_last_event\n";
    std::vector<std::unique_ptr<MultiCloth>> sims;
    constexpr int tri=2*(5*12+5);
    for(Policy policy:policies) {
        auto s=std::make_unique<MultiCloth>(2,2,policy);
        if(geo||topo)
            s->setFixtureVariation(std::uint32_t(geo),std::uint32_t(topo));
        s->setSelfCollisions(true);
        s->setAuditing(strict);
        if(scenario!="free"){
            const Vec3 start=pick(*s,tri);
            if(!s->beginGrab(tri,{1./3,1./3,1./3},start))
                throw std::runtime_error("failed identical barycentric grab");
        }
        sims.push_back(std::move(s));
    }
    // Positions are first checked BEFORE advancing any independent arm.
    const auto& initial=sims[0]->positions();
    for(std::size_t i=1;i<sims.size();++i) {
        if(sims[i]->positions().size()!=initial.size())
            throw std::runtime_error("initial meshes have different topology");
        for(std::size_t j=0;j<initial.size();++j)
            if(distance(initial[j],sims[i]->positions()[j])>1e-12)
                throw std::runtime_error("initial physics states differ");
    }
    const Vec3 anchor=pick(*sims[0],tri);
    RestQuality rest(*sims[0]);
    std::array<double,4> broadTotal{},stepTotal{};
    std::array<std::size_t,4> solverContactTotal{},candidates{};
    std::size_t allRbcBuilds=0,allRbcRepairs=0,allRbcIndexTests=0;
    for(int frame=0;frame<frames;++frame){
        std::array<int,4> slots{};
        for(int slot=0;slot<4;++slot) {
            const int method=(frame+slot)%4;
            slots[std::size_t(method)]=slot;
            drive(*sims[std::size_t(method)],scenario,anchor,frame,frames);
            sims[std::size_t(method)]->step();
        }
        const auto& sap=*sims[0];
        const auto& sapMetric=sap.metrics();
        for(std::size_t m=0;m<sims.size();++m){
            const auto& sim=*sims[m];
            const auto& metric=sim.metrics();
            if(metric.candidatePairs!=sapMetric.candidatePairs)
                throw std::runtime_error(
                    "RBC physical SAME-STATE final-query candidate mismatch frame="+
                    std::to_string(frame)+" policy="+names[m]);
            if(metric.oracleMisses!=0)
                throw std::runtime_error("physical oracle reported missed pairs");
            if(metric.queryCount!=5)
                throw std::runtime_error("not all five contact iterations queried");
            const auto& p=sim.positions();
            if(p.size()!=sap.positions().size())
                throw std::runtime_error("physical vertex count changed");
            double worst=0.;
            for(std::size_t j=0;j<p.size();++j){
                worst=std::max(worst,distance(p[j],sap.positions()[j]));
                if(!std::isfinite(worst))
                    throw std::runtime_error("physical solver has NaN");
            }
            if(worst>1e-7)
                throw std::runtime_error(
                    "RBC physical state diverged from SAP frame="+
                    std::to_string(frame)+" policy="+names[m]+
                    " max_relative_diff="+std::to_string(worst));
            // The strict independent oracle is inside each RBC query.
            if(strict&&m==3&&!metric.oracleChecked)
                throw std::runtime_error(
                    "strict RBC frame lacked any independent SAP oracle check");
            const double maxStretch=rest.maxStretch(sim);
            stepTotal[m]+=metric.stepMs;
            broadTotal[m]+=metric.broadMs;
            solverContactTotal[m]+=metric.selfContacts+metric.sphereContacts;
            candidates[m]+=metric.pairs;
            if(m==3){
                allRbcBuilds+=metric.rbcGlobalBuilds;
                allRbcRepairs+=metric.rbcDirtyRepairs;
                allRbcIndexTests+=metric.rbcIndexComparisons;
            }
            out<<frame<<','<<names[m]<<','<<scenario<<','<<geo<<','<<topo
               <<','<<int(strict)<<','<<slots[m]<<','
               <<metric.stepMs<<','<<metric.broadMs<<','<<metric.narrowMs<<','
               <<metric.pairs<<','<<metric.preciseTests<<','
               <<metric.selfContacts<<','<<metric.sphereContacts<<','
               <<metric.selfCrossings<<','<<metric.velocityClamps<<','
               <<maxStretch<<','<<worst<<','<<metric.oracleMisses<<','
               <<int(metric.oracleChecked)<<','<<metric.rbcQueries<<','
               <<metric.rbcGlobalBuilds<<','<<metric.rbcDirtyRepairs<<','
               <<metric.rbcPotentialPairs<<','
               <<metric.rbcIndexComparisons<<','
               <<metric.rbcCandidateChecks<<','<<metric.lastEvent<<'\n';
        }
    }
    if(!out)throw std::runtime_error("coupled physical CSV write failed");
    std::cout<<std::fixed<<std::setprecision(5)
             <<"PASS RBC 4-policy ACTUAL coupled solver "
             <<scenario<<" frames="<<frames<<" 2 sheets 2 spheres "
             <<"geometry="<<geo<<" topology="<<topo
             <<" strict_every_rbc_solver_query="<<strict<<"\n";
    for(std::size_t m=0;m<sims.size();++m)
        std::cout<<names[m]<<" mean_full_step_ms="
                 <<stepTotal[m]/frames
                 <<" mean_full_broad_ms="<<broadTotal[m]/frames
                 <<" actual_contacts="<<solverContactTotal[m]
                 <<" reported_pairs_sum="<<candidates[m]<<"\n";
    std::cout<<"RBC total builds="<<allRbcBuilds
             <<" local repairs="<<allRbcRepairs
             <<" paid index comparisons="<<allRbcIndexTests<<"\n"
             <<"One execution per policy per scene; NO A/A, no confidence"
               " interval, no calibrated physical fidelity or CCD claim.\n";
}
} // namespace
int main(int argc,char** argv){
    try {
        if(argc!=7)
            throw std::invalid_argument(
                "usage: ccp_rbc_coupled_tournament NEW.csv FRAMES(8..160)"
                " free|pull|fold GEOMETRY_SEED TOPOLOGY_SEED"
                " STRICT_AUDIT(0|1)");
        const int frames=integer(argv[2],8,160);
        const std::string scenario(argv[3]);
        if(scenario!="free"&&scenario!="pull"&&scenario!="fold")
            throw std::invalid_argument("unsupported coupled scenario");
        const int geo=integer(argv[4],0,2000000000);
        const int topo=integer(argv[5],0,2000000000);
        const bool audit=integer(argv[6],0,1)!=0;
        execute(argv[1],frames,scenario,geo,topo,audit);
        return 0;
    } catch(const std::exception& e){
        std::cerr<<"FAIL RBC real physical solver tournament: "<<e.what()<<"\n";
        return 1;
    }
}

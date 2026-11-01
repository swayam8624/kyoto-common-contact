#include "ccp/live_cloth.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int kTrials=5;
constexpr int kFrames=300;
constexpr int kWarmupFrames=30;

struct Summary {
    std::vector<double> stepMs;
    std::size_t builds{}, reuses{};
};

double percentile95(std::vector<double> values) {
    if(values.empty()) return 0.0;
    std::sort(values.begin(),values.end());
    const auto index=static_cast<std::size_t>(std::ceil(0.95*values.size()))-1;
    return values[index];
}

void validateStep(const ccp::live::ClothSimulation& sim, const char* policy) {
    const auto& s=sim.stats();
    if(s.oracleChecked && s.oracleMisses!=0)
        throw std::runtime_error(std::string(policy)+" reported a proximity oracle miss");
    if(!std::isfinite(s.stepMs) || !std::isfinite(s.broadMs) || !std::isfinite(s.narrowMs) ||
       !std::isfinite(s.minGap))
        throw std::runtime_error(std::string(policy)+" reported a nonfinite diagnostic");
    for(const auto p:sim.positions())
        if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            throw std::runtime_error(std::string(policy)+" produced a nonfinite particle position");
}

double comparePositions(const ccp::live::ClothSimulation& a, const ccp::live::ClothSimulation& b) {
    if(a.positions().size()!=b.positions().size()) throw std::runtime_error("particle count mismatch");
    double maxError=0.0;
    for(std::size_t i=0;i<a.positions().size();++i) {
        const auto p=a.positions()[i], q=b.positions()[i];
        const double dx=p.x-q.x,dy=p.y-q.y,dz=p.z-q.z;
        maxError=std::max(maxError,std::sqrt(dx*dx+dy*dy+dz*dz));
    }
    return maxError;
}

void writeRow(std::ofstream& csv,int trial,int frame,const char* policy,
              const ccp::live::Stats& s) {
    csv<<trial<<','<<frame<<','<<policy<<','<<s.stepMs<<','<<s.broadMs<<','<<s.narrowMs<<','
       <<s.oracleMs<<','<<s.diagnosticMs<<','<<s.solverMs<<','
       <<s.queryBuilds<<','<<s.queryReuses<<','<<s.pairTests<<','<<s.candidateCount<<','
       <<s.proximityCount<<','<<s.preciseTests<<','<<s.diagnosticTests<<','
       <<s.minGap<<','<<(s.oracleChecked?std::to_string(s.oracleMisses):"NA")<<'\n';
}

void printSummary(const char* name,const Summary& s) {
    if(s.stepMs.empty()) throw std::runtime_error("no measured frames for summary");
    double sum=0.0,maximum=0.0;
    for(const double value:s.stepMs) { sum+=value; maximum=std::max(maximum,value); }
    std::cout<<name<<" meanStepMs="<<sum/s.stepMs.size()
             <<" maxStepMs="<<maximum
             <<" p95StepMs="<<percentile95(s.stepMs)
             <<" generatorCalls="<<s.builds<<" reuses="<<s.reuses<<'\n';
}
}

int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("usage: live_benchmark <output.csv>");
        std::ofstream csv(argv[1]);
        if(!csv) throw std::runtime_error(std::string("cannot open output CSV: ")+argv[1]);
        csv<<"trial,frame,policy,stepMs,broadMs,narrowMs,oracleMs,diagnosticMs,otherMs,builds,reuses,pairTests,candidates,proximity,preciseTests,diagnosticTests,minGap,oracleMisses\n";
        csv<<std::setprecision(12);

        // Fully checked preflight is outside the timed comparisons.
        // Disabling an oracle never implies zero misses.
        {
            ccp::live::ClothSimulation checkedRebuild,checkedReuse;
            checkedRebuild.setQueryRequest({0.05,0.04,ccp::live::UpdatePolicy::RebuildEveryIteration});
            checkedReuse.setQueryRequest({0.05,0.04,ccp::live::UpdatePolicy::GuardedReuse});
            for(int frame=0;frame<kFrames;++frame) {
                checkedRebuild.step(); checkedReuse.step();
                validateStep(checkedRebuild,"oracle_preflight_rebuild");
                validateStep(checkedReuse,"oracle_preflight_reuse");
                if(!checkedRebuild.stats().oracleChecked || !checkedReuse.stats().oracleChecked)
                    throw std::runtime_error("preflight did not execute exhaustive oracle");
                if(comparePositions(checkedRebuild,checkedReuse)>=1e-9)
                    throw std::runtime_error("preflight solver states diverged");
            }
            std::cout<<"Independent oracle preflight: "<<kFrames
                     <<" paired steps, zero required-pair misses.\n";
        }
        Summary baseline,guarded;
        baseline.stepMs.reserve(kTrials*(kFrames-kWarmupFrames));
        guarded.stepMs.reserve(kTrials*(kFrames-kWarmupFrames));
        double maxPositionError=0.0;
        const ccp::live::QueryRequest request{0.05,0.04,ccp::live::UpdatePolicy::GuardedReuse};
        for(int trial=0;trial<kTrials;++trial) {
            ccp::live::ClothSimulation rebuildSim,guardedSim;
            rebuildSim.setQueryRequest({request.margin,request.displacementBudget,
                                        ccp::live::UpdatePolicy::RebuildEveryIteration});
            guardedSim.setQueryRequest(request);
            rebuildSim.setOracleEnabled(false);
            guardedSim.setOracleEnabled(false);
            for(int frame=0;frame<kFrames;++frame) {
                if(trial%2==0) {
                    rebuildSim.step(); guardedSim.step();
                } else {
                    guardedSim.step(); rebuildSim.step();
                }
                validateStep(rebuildSim,"baseline_rebuild");
                validateStep(guardedSim,"guarded_reuse");
                maxPositionError=std::max(maxPositionError,comparePositions(rebuildSim,guardedSim));
                if(maxPositionError>=1e-9) throw std::runtime_error("paired physical positions diverged by at least 1e-9");
                if(frame<kWarmupFrames) continue;

                const auto& r=rebuildSim.stats();
                const auto& g=guardedSim.stats();
                baseline.stepMs.push_back(r.stepMs); baseline.builds+=r.queryBuilds; baseline.reuses+=r.queryReuses;
                guarded.stepMs.push_back(g.stepMs); guarded.builds+=g.queryBuilds; guarded.reuses+=g.queryReuses;
                writeRow(csv,trial,frame,"baseline_rebuild",r);
                writeRow(csv,trial,frame,"guarded_reuse",g);
            }
        }
        csv.close();
        if(!csv) throw std::runtime_error("failed while writing output CSV");
        std::cout<<"TIMING MODE: exhaustive oracle disabled; final global display diagnostics remain measured.\n";
        std::cout<<"Measured paired CPU cloth diagnostics; first "<<kWarmupFrames
                 <<" frames per trial were advanced but excluded from timing summaries and CSV.\n";
        printSummary("baseline_rebuild",baseline);
        printSummary("guarded_reuse",guarded);
        std::cout<<"maxPositionError="<<maxPositionError
                 <<" measuredFramesPerPolicy="<<baseline.stepMs.size()<<'\n';
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"live_benchmark: FAILED: "<<e.what()<<'\n';
        return 1;
    }
}

// Kyoto Common Contact: independent 3D broad-phase comparative benchmark.
// Reference library: the REAL Bullet Physics btDbvtBroadphase implementation.
// Research boundary: no narrow-phase or solver result is inferred from this
// broad-phase benchmark. In particular, the synthetic multi-sheet geometry
// contains no cloth-cloth contact response or physically simulated coupling.
#include "ccp/live_cloth.hpp"
#include <btBulletCollisionCommon.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::live::Pair;
using ccp::live::Vec3;
using Clock = std::chrono::steady_clock;
using BoxList = std::vector<Bounds>;
using Pairs = std::vector<Pair>;
constexpr double kMargin=0.05, kBudget=0.055;
constexpr int kSamples=32;
struct Observation {
    double milliseconds{};
    std::size_t candidates{}, extras{}, builds{}, reuses{};
};
struct Result {
    std::string scene, algorithm;
    int primitives{}, samples{};
    double meanMs{}, medianMs{}, p95Ms{}, avgCandidates{}, avgExtras{};
    std::size_t totalBuilds{}, totalReuses{}, totalMisses{};
};

bool overlap(const Bounds& a,const Bounds& b) {
    return a.min.x<=b.max.x && b.min.x<=a.max.x &&
           a.min.y<=b.max.y && b.min.y<=a.max.y &&
           a.min.z<=b.max.z && b.min.z<=a.max.z;
}
bool eligible(const Bounds& a,const Bounds& b) {
    return (a.mask&b.category)!=0 && (b.mask&a.category)!=0;
}
Bounds padded(Bounds b,double radius=kMargin*0.5) {
    b.min.x-=radius; b.min.y-=radius; b.min.z-=radius;
    b.max.x+=radius; b.max.y+=radius; b.max.z+=radius;
    return b;
}
BoxList pad(const BoxList& raw) {
    BoxList result; result.reserve(raw.size());
    for(auto b:raw) result.push_back(padded(b));
    return result;
}

// The same exact 3D AABB primitive bounds and eligibility filter are passed
// to all algorithms. Bullet is never replaced with an imitation BVH.
Pairs bulletPairs(const BoxList& alreadyPadded) {
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher(&config);
    btDbvtBroadphase broadphase;
    // Bullet's internal dynamic-tree implementation may use padded "fat"
    // leaves. We preserve any conservative extra pairs in its output.
    // Stable pointers directly to immutable input bounds: O(1) metadata
    // recovery per pair. A linear ID lookup here would unfairly add O(N)
    // per returned pair to Bullet's measured time in dense scenes.
    std::vector<btBroadphaseProxy*> proxies;
    proxies.reserve(alreadyPadded.size());
    for(std::size_t i=0;i<alreadyPadded.size();++i) {
        const auto& b=alreadyPadded[i];
        auto* proxy=broadphase.createProxy(
            btVector3(btScalar(b.min.x),btScalar(b.min.y),btScalar(b.min.z)),
            btVector3(btScalar(b.max.x),btScalar(b.max.y),btScalar(b.max.z)),
            BOX_SHAPE_PROXYTYPE,
            const_cast<Bounds*>(&alreadyPadded[i]),
            1,1,&dispatcher);
        if(!proxy) throw std::runtime_error("Bullet broadphase could not create proxy");
        proxies.push_back(proxy);
    }
    broadphase.calculateOverlappingPairs(&dispatcher);
    const auto& bullet=broadphase.getOverlappingPairCache()->getOverlappingPairArray();
    Pairs result;
    result.reserve(static_cast<std::size_t>(bullet.size()));
    for(int i=0;i<bullet.size();++i) {
        const auto* p=bullet[i].m_pProxy0;
        const auto* q=bullet[i].m_pProxy1;
        if(!p||!q||!p->m_clientObject||!q->m_clientObject)
            throw std::runtime_error("Bullet returned an invalid collision pair");
        const auto& ba=*static_cast<const Bounds*>(p->m_clientObject);
        const auto& bb=*static_cast<const Bounds*>(q->m_clientObject);
        if(!eligible(ba,bb)) continue;
        const int a=ba.id;
        const int b=bb.id;
        // We deliberately keep Bullet's conservative possible overlaps, even
        // if its internal fat-tree pair cache is broader than the exact AABB
        // reference; this is what the next method-specific stage receives.
        result.push_back({std::min(a,b),std::max(a,b)});
    }
    std::sort(result.begin(),result.end());
    result.erase(std::unique(result.begin(),result.end()),result.end());
    for(auto* proxy:proxies) broadphase.destroyProxy(proxy,&dispatcher);
    return result;
}

// Synthetic but deterministic 3D spatial distributions. No claims of
// physically coupled cloth are made by the three-sheet workload.
BoxList makeScene(const std::string& scene,int sample) {
    BoxList out;
    if(scene=="sparse256" || scene=="clustered512" || scene=="dense256") {
        const int n=scene=="clustered512"?512:256;
        out.reserve(n);
        std::mt19937_64 rng(0xCC0FFEE + 101*sample);
        std::uniform_real_distribution<double> uniform(-1.0,1.0);
        const double scale=scene=="sparse256"?20.0:(scene=="clustered512"?2.1:0.28);
        const double half=scene=="dense256"?0.24:0.13;
        for(int i=0;i<n;++i) {
            // Even sparse scenes include reproducible micro-motion, so
            // the guarded certificate is exercised across frames.
            const double x=scale*uniform(rng)+0.005*std::sin(.3*sample+i);
            const double y=scale*uniform(rng)+0.005*std::cos(.4*sample+i);
            const double z=scale*uniform(rng);
            out.push_back({i,{x-half,y-half,z-half},
                    {x+half,y+half,z+half},1u,1u,true});
        }
        return out;
    }
    if(scene!="sheets2" && scene!="sheets3")
        throw std::runtime_error("unknown broad-phase benchmark scene "+scene);
    const int layers=scene=="sheets2"?2:3;
    constexpr int side=13;
    const double t=double(sample)*0.037;
    out.reserve(std::size_t(layers)*2*(side-1)*(side-1)+2);
    for(int layer=0;layer<layers;++layer) {
        std::vector<Vec3> vertices;
        vertices.reserve(side*side);
        for(int j=0;j<side;++j) for(int i=0;i<side;++i) {
            const double u=(double(i)/(side-1)-.5)*2.5;
            const double v=(double(j)/(side-1)-.5)*2.5;
            const double height=1.05+layer*0.045+
                0.035*std::sin(1.5*u+2.0*v+t+layer*0.7);
            vertices.push_back({u+layer*0.08,v+layer*0.05,height});
        }
        const auto emit=[&](int i,int j,int k) {
            const auto a=vertices[i],b=vertices[j],c=vertices[k];
            const Vec3 lo{std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),
                          std::min({a.z,b.z,c.z})};
            const Vec3 hi{std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),
                          std::max({a.z,b.z,c.z})};
            out.push_back({int(out.size()),lo,hi,1u,3u,true});
        };
        for(int j=0;j<side-1;++j) for(int i=0;i<side-1;++i) {
            const int a=j*side+i,b=a+1,c=a+side,d=c+1;
            emit(a,b,d); emit(a,d,c);
        }
    }
    for(int ball=0;ball<2;++ball) {
        const double x=ball==0?-.36:.48;
        const double y=ball==0?0.0:.25;
        const double z=0.75;
        const double radius=ball==0?.65:.43;
        out.push_back({int(out.size()),{x-radius,y-radius,z-radius},
                            {x+radius,y+radius,z+radius},2u,1u,false});
    }
    return out;
}
std::vector<std::string> cases() {
    return {"sparse256","clustered512","dense256","sheets2","sheets3"};
}
double percentile(std::vector<double> values,double p) {
    std::sort(values.begin(),values.end());
    return values[std::min(values.size()-1,std::size_t(std::ceil(values.size()*p)-1))];
}
std::string backendName(int kind) {
    switch(kind) {
        case 0: return "exhaustive_aabb";
        case 1: return "sweep_prune";
        case 2: return "bullet_dbvt";
        case 3: return "guarded_sap";
        default: throw std::runtime_error("unknown algorithm");
    }
}
Result measure(const std::string& scene,int kind,const std::vector<BoxList>& samples,
               const std::vector<Pairs>& oracle) {
    ccp::live::BroadPhaseService cached;
    std::vector<double> elapsed;
    double totalCandidates=0,totalExtras=0;
    std::size_t builds=0,reuses=0;
    for(std::size_t i=0;i<samples.size();++i) {
        const auto& bounds=samples[i];
        ccp::live::QueryMetrics metrics;
        const auto begin=Clock::now();
        Pairs pairs;
        if(kind==0 || kind==1) pairs=ccp::live::candidates(pad(bounds),kind==1);
        if(kind==2) pairs=bulletPairs(pad(bounds));
        if(kind==3) pairs=cached.query(bounds,
                {kMargin,kBudget,ccp::live::UpdatePolicy::GuardedReuse},true,metrics);
        const double milliseconds=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
        if(!std::includes(pairs.begin(),pairs.end(),oracle[i].begin(),oracle[i].end()))
            throw std::runtime_error("REQUIRED PAIR OMITTED by "+backendName(kind)+" on "+scene+
                                     " sample "+std::to_string(i));
        const auto extra=pairs.size()-oracle[i].size();
        // Timing is a full callable broad-phase cost, including construction
        // and pair materialization. Truth calculation and validation excluded.
        elapsed.push_back(milliseconds);
        totalCandidates+=double(pairs.size());
        totalExtras+=double(extra);
        builds+=(kind==3?metrics.builds:1);
        reuses+=metrics.reuses;
    }
    const auto size=samples.size();
    return {scene,backendName(kind),int(samples[0].size()),int(size),
        std::accumulate(elapsed.begin(),elapsed.end(),0.0)/double(size),
        percentile(elapsed,0.5),percentile(elapsed,0.95),
        totalCandidates/double(size),totalExtras/double(size),
        builds,reuses,0};
}
void selfTest() {
    const BoxList raw={
        {10,{0,0,0},{1,1,1},1u,3u,true},
        {20,{0.5,0.5,0.5},{1.5,1.5,1.5},1u,3u,true},
        {30,{7,7,7},{8,8,8},2u,1u,true},
        {40,{.4,.4,.4},{.6,.6,.6},4u,4u,true}};
    const auto boxes=pad(raw);
    const auto oracle=ccp::live::candidates(boxes,false);
    const auto sap=ccp::live::candidates(boxes,true);
    const auto bullet=bulletPairs(boxes);
    if(oracle!=sap || !std::includes(bullet.begin(),bullet.end(),oracle.begin(),oracle.end()))
        throw std::runtime_error("basic Bullet/SAP/exhaustive inclusion failure");
    for(const auto& scene:cases()) {
        for(int i=0;i<3;++i) {
            const auto sample=pad(makeScene(scene,i));
            const auto expected=ccp::live::candidates(sample,false);
            const auto sweep=ccp::live::candidates(sample,true);
            const auto actual=bulletPairs(sample);
            if(expected!=sweep ||
               !std::includes(actual.begin(),actual.end(),expected.begin(),expected.end()))
                throw std::runtime_error("Bullet DBVT correctness error: "+scene);
        }
    }
    std::cout<<"Bullet dynamic BVH / SAP / exhaustive: conservative pair coverage verified\n";
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--self-test") {
            selfTest(); return 0;
        }
        if(argc!=2) throw std::runtime_error(
            "usage: ccp_broadphase_leaderboard OUTPUT.csv | --self-test");
        selfTest();
        std::filesystem::path file(argv[1]);
        if(!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path());
        std::ofstream csv(file);
        if(!csv) throw std::runtime_error("cannot create leaderboard CSV");
        csv<<"scene,algorithm,primitives,samples,meanMs,medianMs,p95Ms,"
              "meanCandidates,meanExtraCandidates,builds,reuses,misses\n";
        csv<<std::fixed<<std::setprecision(9);
        for(const auto& scene:cases()) {
            // Every algorithm receives identical geometric input states.
            // For the sparse/dense fixtures, sample-to-sample random layouts
            // deliberately stress cache invalidation. The multi-sheet fixture
            // is a smooth moving-bounds workload, NOT physical cloth response.
            std::vector<BoxList> samples;
            std::vector<Pairs> oracle;
            samples.reserve(kSamples); oracle.reserve(kSamples);
            for(int i=0;i<kSamples;++i) {
                auto raw=makeScene(scene,i);
                oracle.push_back(ccp::live::candidates(pad(raw),false));
                samples.push_back(std::move(raw));
            }
            for(int algorithm=0;algorithm<4;++algorithm) {
                const auto r=measure(scene,algorithm,samples,oracle);
                csv<<r.scene<<','<<r.algorithm<<','<<r.primitives<<','<<r.samples<<','
                   <<r.meanMs<<','<<r.medianMs<<','<<r.p95Ms<<','
                   <<r.avgCandidates<<','<<r.avgExtras<<','
                   <<r.totalBuilds<<','<<r.totalReuses<<','<<r.totalMisses<<'\n';
                std::cout<<r.scene<<" / "<<r.algorithm<<" meanMs="<<r.meanMs
                         <<" avgPairs="<<r.avgCandidates
                         <<" extra="<<r.avgExtras
                         <<" builds="<<r.totalBuilds
                         <<" reuses="<<r.totalReuses<<'\n';
            }
        }
        csv.flush();
        if(!csv) throw std::runtime_error("could not write leaderboard");
        std::cout<<"READ THIS: Bullet btDbvtBroadphase is an upstream 3D library baseline;"
                 <<" 'best published' is not a universal or verified ranking."
                 <<" These are CPU rebuild costs, not solver or CCD performance.\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Broad-phase comparison FAILED: "<<e.what()<<'\n';
        return 1;
    }
}

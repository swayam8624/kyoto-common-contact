// Kyoto RBC full-pair, same-source, native 5-policy experiment.
//
// Reads IMMUTABLE KYOTOM01 source AABBs (already padded by exporter).
// All candidate generators must produce EXACTLY the same current pair IDs
// as independent fresh SAP on EVERY frame, before any time is reported.
// Timing includes native RBC certificate building, dirty-frontier repair,
// bookkeeping and candidate enumeration, but excludes snapshot read,
// independent oracle check and CSV write. Native method order is rotated
// each frame (not a balanced multi-session A/A experiment).
//
// Methods: fresh SAP, fixed original Guard, frozen V4 (unchanged),
// persistent SAP, slow RBC node-set negative, NEW flat RBC frontier.
// This is genuine full pair emission, NOT count-only GPU/CPU workload.
// This is not a contact solver experiment: see rbc_coupled_tournament.
#include "ccp/rbc/certified_spatial_frontier.hpp"
#include "ccp/rbc/certified_flat_frontier.hpp"
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bounds=ccp::live::Bounds;
using Pair=ccp::live::Pair;
using Pairs=std::vector<Pair>;
using Clock=std::chrono::steady_clock;
static_assert(std::endian::native==std::endian::little,
              "KYOTOM01 replay requires little endian");
constexpr std::array<const char*,6> methods={
    "sap_rebuild","guard_fixed_exact","v4_frozen",
    "sap_persistent","rbc_indexed_repair","rbc_flat_repair"
};
int bounded(const char* text,int lo,int hi){
    const std::string value(text);
    std::size_t pos=0; long long v=0;
    try{v=std::stoll(value,&pos);}
    catch(...){throw std::invalid_argument("expected bounded integer");}
    if(pos!=value.size()||v<lo||v>hi)
        throw std::invalid_argument("integer out of range");
    return int(v);
}
struct Source{
    std::ifstream in;
    std::uint32_t n{},frames{},seed{},material{};
    explicit Source(const char* pathname):in(pathname,std::ios::binary){
        if(!in)throw std::runtime_error("cannot open immutable source");
        char magic[8]{};
        in.read(magic,8);
        if(!in||std::string(magic,8)!="KYOTOM01")
            throw std::runtime_error("source is not KYOTOM01");
        for(auto* f:{&n,&frames,&seed,&material})
            in.read(reinterpret_cast<char*>(f),4);
        if(!in||n<16||n>50000||frames<1||frames>10000||material>5)
            throw std::runtime_error("source header invalid");
        const auto at=in.tellg();
        in.seekg(0,std::ios::end);
        if((in.tellg()-at)!=std::streamoff(n)*std::streamoff(frames)*24)
            throw std::runtime_error("source exact frame size mismatch");
        in.seekg(at);
    }
    [[nodiscard]] std::vector<Bounds> next(){
        std::vector<Bounds> output; output.reserve(n);
        for(std::uint32_t i=0;i<n;++i){
            std::array<float,6> v{};
            in.read(reinterpret_cast<char*>(v.data()),sizeof(v));
            if(!in)throw std::runtime_error("truncated immutable source frame");
            for(float x:v)
                if(!std::isfinite(x))
                    throw std::runtime_error("nonfinite immutable AABB");
            if(v[0]>v[3]||v[1]>v[4]||v[2]>v[5])
                throw std::runtime_error("inverted immutable AABB");
            Bounds b{};
            b.id=static_cast<int>(i);
            b.min={v[0],v[1],v[2]};
            b.max={v[3],v[4],v[5]};
            b.category=1;b.mask=1;b.movable=true;
            output.push_back(b);
        }
        return output;
    }
};
std::uint64_t fingerprint(const Pairs& pairs){
    std::uint64_t hash=14695981039346656037ull;
    // Stable FNV-1a over exact unsigned 32-bit pair ID bytes, not a
    // cryptographic SHA. Original input SHA receipts are computed by Python.
    for(Pair p:pairs)for(int v:{p.a,p.b}){
        const std::uint32_t number=static_cast<std::uint32_t>(v);
        for(int byte=0;byte<4;++byte){
            hash^=(number>>(8*byte))&255u;
            hash*=1099511628211ull;
        }
    }
    return hash;
}
struct Method {
    Pairs pairs;
    double ms{};
    std::size_t potential{},localRepairs{},globalBuilds{},
                indexComparisons{},queryChecks{};
    std::string backend{"none"};
    bool trial{},switched{};
};
void replay(const char* sourcePath,const char* outputPath,
            int warmup,double skin){
    if(std::filesystem::exists(outputPath))
        throw std::runtime_error("refusing to overwrite native RBC evidence");
    Source source(sourcePath);
    if(warmup>=static_cast<int>(source.frames))
        throw std::invalid_argument("warmup must precede final source frame");
    std::ofstream csv(outputPath);
    if(!csv)throw std::runtime_error("cannot open new trace");
    csv<<"frame,method,query_ms,pairs,pair_fnv64,execution_slot,"
          "rbc_potential_pairs,rbc_local_repairs,rbc_full_build_delta,"
          "rbc_index_comparisons,rbc_query_pair_checks,"
          "v4_action,v4_trial,v4_switched,after_warmup\n";
    csv<<std::setprecision(12)<<std::fixed;
    ccp::live::BroadPhaseService guard;
    ccp::final_broadphase::GuardSAPBoundedTrial v4;
    ccp::final_broadphase::PersistentSAP persistent;
    ccp::rbc::CertifiedSpatialFrontier rbc;
    ccp::rbc::CertifiedFlatFrontier flat;
    std::vector<ccp::rbc::Reach> radius(
        source.n,ccp::rbc::Reach{0.,skin}); // input ALREADY padded
    std::array<double,6> sum{};
    std::array<std::size_t,6> count{};
    for(std::uint32_t frame=0;frame<source.frames;++frame){
        const auto current=source.next();
        std::array<Method,6> measurements{};
        std::array<int,6> slots{};
        for(int slot=0;slot<6;++slot){
            const int id=(int(frame)+slot)%6;
            slots[std::size_t(id)]=slot;
            Method& m=measurements[std::size_t(id)];
            const auto begin=Clock::now();
            if(id==0){
                m.pairs=ccp::live::candidates(current,true);
            }else if(id==1){
                ccp::live::QueryMetrics metric;
                const auto& fat=guard.query(current,
                    {0.,.045,ccp::live::UpdatePolicy::GuardedReuse},
                    true,metric);
                m.pairs=ccp::final_broadphase::exactGuard(fat,current);
            }else if(id==2){
                auto outcome=v4.query(current);
                m.pairs=std::move(outcome.pairs);
                m.backend=outcome.action;
                m.trial=outcome.trialFrame;
                m.switched=outcome.switched;
            }else if(id==3){
                m.pairs=persistent.query(current);
            }else if(id==4){
                const auto before=rbc.global_builds();
                if(!rbc.initialized())
                    rbc.initialize(current,radius);
                auto result=rbc.query(current);
                m.pairs=std::move(result.pairs);
                m.potential=result.potential_pairs;
                m.localRepairs=result.local_repairs;
                m.globalBuilds=rbc.global_builds()-before;
                m.indexComparisons=result.build_comparisons;
                m.queryChecks=result.query_pair_checks;
            }else{
                const auto before=flat.global_builds();
                if(!flat.initialized())
                    flat.initialize(current,radius);
                auto result=flat.query(current);
                m.pairs=std::move(result.pairs);
                m.potential=result.potential_pairs;
                m.localRepairs=result.local_repairs;
                m.globalBuilds=flat.global_builds()-before;
                m.indexComparisons=result.build_comparisons;
                m.queryChecks=result.query_pair_checks;
            }
            m.ms=std::chrono::duration<double,std::milli>(
                Clock::now()-begin).count();
        }
        // No counting/hash shortcuts. Full sorted pair IDs from each
        // algorithm are compared against fresh independent SAP output.
        const auto& oracle=measurements[0].pairs;
        for(int i=0;i<6;++i){
            const auto& m=measurements[std::size_t(i)];
            if(m.pairs!=oracle)
                throw std::runtime_error(
                    std::string("EXACT PAIR ORACLE FAIL: frame ")+
                    std::to_string(frame)+" method "+methods[std::size_t(i)]+
                    " expected "+std::to_string(oracle.size())+
                    " observed "+std::to_string(m.pairs.size()));
            if(frame>=std::uint32_t(warmup)){
                sum[std::size_t(i)]+=m.ms;
                ++count[std::size_t(i)];
            }
            csv<<frame<<','<<methods[std::size_t(i)]<<','<<m.ms<<','
               <<m.pairs.size()<<','<<fingerprint(m.pairs)<<','
               <<slots[std::size_t(i)]<<','
               <<m.potential<<','<<m.localRepairs<<','<<m.globalBuilds<<','
               <<m.indexComparisons<<','<<m.queryChecks<<','
               <<m.backend<<','<<int(m.trial)<<','<<int(m.switched)
               <<','<<int(frame>=std::uint32_t(warmup))<<'\n';
        }
    }
    if(!csv)throw std::runtime_error("failed flushing native RBC replay");
    std::cout<<std::fixed<<std::setprecision(5)
             <<"PASS RBC exact 6-arm material replay source "<<sourcePath
             <<" frames "<<source.frames<<" N "<<source.n
             <<" all candidate pairs equal on EVERY source frame\n";
    for(int i=0;i<6;++i)
        std::cout<<methods[std::size_t(i)]<<" mean_query_ms="
                 <<sum[std::size_t(i)]/double(count[std::size_t(i)])
                 <<" guard_over_method="<<
                     (sum[1]/double(count[1]))/
                     (sum[std::size_t(i)]/double(count[std::size_t(i)]))
                 <<"\n";
    std::cout<<"These are single-session, rotated-order CPU candidate-only"
               " timings; no A/A, population CI, or physical solver speedup.\n";
}
} // namespace
int main(int argc,char** argv){
    try{
        if(argc<3||argc>5)
            throw std::invalid_argument(
                "usage: ccp_rbc_material_tournament ORIGINAL.kmcb NEW.csv [WARMUP=8] [SKIN=0.045]");
        const int warmup=argc>=4?bounded(argv[3],0,9999):8;
        const double skin=argc==5?std::stod(argv[4]):.045;
        if(!std::isfinite(skin)||skin<0||skin>10)
            throw std::invalid_argument("skin must be finite in 0..10");
        replay(argv[1],argv[2],warmup,skin);
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"RBC NATIVE REPLAY FAIL: "<<e.what()<<'\n';
        return 1;
    }
}

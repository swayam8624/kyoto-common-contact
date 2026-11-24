// Pass 28: three-arm fully paid candidate query ablation on real FEM geometry.
// Artificial collision policy assignments are intentional controlled workloads;
// they do not modify or recouple the underlying physical simulation.
#include "kyoto/fem_feature_snapshots.hpp"
#include "kyoto/refit_feature_hierarchy.hpp"
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace f=kyoto::contact::features;
namespace s=kyoto::solid;
namespace fem=kyoto::contact::fem;
using kyoto::contact::atlas::RefitHierarchy;
using Clock=std::chrono::steady_clock;
struct Source{int scene,step;std::size_t contact;f::Mesh mesh;};
static std::vector<Source> generate(){
    std::vector<Source> out;
    const auto rigid=s::corrugated_contact_patch();
    for(int scene=0;scene<3;++scene){
        s::Material mat; mat.young=17500.+scene*2250.;
        auto body=s::toroidal_solid(12+6*scene,3,3,mat);
        for(auto& n:body.nodes)n.v={.12-.1*scene,-.05+.04*scene,0.};
        for(int step=1;step<=240;++step){
            auto before=body;
            const auto phys=s::step_time_consistent_rigid_triangle_mesh(body,rigid,.00075);
            if(phys.accepted_leaves<1)throw std::runtime_error("unaccepted FEM");
            if(step%12==0)out.push_back({scene,step,phys.face_impulse_updates,
                fem::feature_snapshot(before,body,rigid)});
        }
    }
    if(out.size()!=60)throw std::runtime_error("missing physical source");
    return out;
}
static void policy(f::Mesh& m,int regime,int step){
    auto assign=[&](auto& record,std::uint64_t group){
        const unsigned i=unsigned(group%8);
        if(regime==0){record.category=1;record.mask=~std::uint64_t{0};}
        else if(regime==1){record.category=std::uint64_t{1}<<i;record.mask=record.category;}
        else if(regime==2){record.category=std::uint64_t{1}<<i;record.mask=std::uint64_t{1}<<((i+1)%8);}
        else if(regime==3){record.category=std::uint64_t{1}<<i;
            record.mask=(std::uint64_t{1}<<((i+1)%8))|(std::uint64_t{1}<<((i+7)%8));}
        else {record.category=std::uint64_t{1}<<(i+(step/36)%2*56);
            record.mask=record.category;}
    };
    // Coherent groups use vertex id/object partition; edge/face policies
    // use the first vertex's ID. Policy labels are not physics events.
    for(auto& v:m.vertices)assign(v,v.id/12);
    for(auto& e:m.edges)assign(e,e.vertices[0]/12);
    for(auto& face:m.faces)assign(face,face.vertices[0]/12);
}
int main(int argc,char**argv){
 try{
    if(argc!=2)throw std::invalid_argument("usage: pass28 OUTPUT_DIR");
    std::filesystem::create_directories(argv[1]);
    std::ofstream csv(std::filesystem::path(argv[1])/"pass28_fem_policy.csv");
    if(!csv)throw std::runtime_error("cannot write receipt");
    csv<<"regime,scene,step,trial,order,cold,source_contact_updates,oracle_pairs,"
          "base_ns,union_ns,relation_ns,union_prunes,relation_prunes,"
          "union_bytes,relation_bytes\n";
    const auto sources=generate();
    constexpr std::array<std::array<int,3>,6> orders{{
        {{0,1,2}},{{0,2,1}},{{1,0,2}},{{1,2,0}},{{2,0,1}},{{2,1,0}}}};
    for(int regime=0;regime<5;++regime){
        // Generate oracle once per unique geometry/policy state, not
        // inside the measured method clocks or repeated every trial.
        struct Item{f::Mesh mesh;std::vector<f::Candidate> truth;bool cold;};
        std::vector<Item> frames;
        for(std::size_t i=0;i<sources.size();++i){
            auto m=sources[i].mesh;
            m.topology_epoch=std::uint64_t(sources[i].scene+1);
            policy(m,regime,sources[i].step);
            const bool cold=i==0||sources[i].scene!=sources[i-1].scene||
              (regime==4&&sources[i].step/36!=sources[i-1].step/36);
            auto truth=f::exhaustive(m).pairs;
            frames.push_back({std::move(m),std::move(truth),cold});
        }
        for(int trial=0;trial<6;++trial){
            RefitHierarchy base(true,false,false,4,true);
            RefitHierarchy uni(true,false,false,4,true,false,false,false,false,true);
            RefitHierarchy rel(true,false,false,4,true,false,false,false,true,false);
            std::array<RefitHierarchy*,3> methods{&base,&uni,&rel};
            for(std::size_t i=0;i<frames.size();++i){
                const auto& frame=frames[i];const auto& src=sources[i];
                const int order=int((i+trial)%6);
                std::array<long long,3> times{};
                for(int method:orders[order]){
                    const auto before=methods[method]->rebuilds();
                    const auto t0=Clock::now();
                    auto actual=methods[method]->query(frame.mesh);
                    times[method]=std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now()-t0).count();
                    if(times[method]<=0||actual.pairs!=frame.truth||
                        (methods[method]->rebuilds()!=before)!=frame.cold)
                        throw std::runtime_error("candidate oracle or cold invalidation mismatch");
                }
                if(base.policy_allocated_bytes()!=0)
                    throw std::runtime_error("baseline paid policy memory");
                csv<<regime<<','<<src.scene<<','<<src.step<<','<<trial<<','<<order<<','
                   <<frame.cold<<','<<src.contact<<','<<frame.truth.size()<<','
                   <<times[0]<<','<<times[1]<<','<<times[2]<<','
                   <<uni.policy_prunes()<<','<<rel.policy_prunes()<<','
                   <<uni.policy_allocated_bytes()<<','<<rel.policy_allocated_bytes()<<'\n';
            }
        }
    }
    if(!csv.good())throw std::runtime_error("incomplete receipt");
    std::cout<<"PASS28: 300 geometry/policy states, 1800 rows, 5400 exact paid queries\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

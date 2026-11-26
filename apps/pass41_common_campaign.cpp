// A single fair broad-phase campaign. All arms receive exactly the same
// swept boxes/mesh topology, and pay build/refit, filtering and canonical
// output. Physical integration and snapshot assembly are outside timings.
// Stock IPC is optional at compile time; no serial dependency shim in the
// official comparison. Its outward float superset is normalized to the
// same binary64 candidate contract before measurement ends.
#include "kyoto/fused_axis_sap.hpp"
#include "kyoto/cached_axis_sap.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#include "kyoto/physical_scaling_snapshot.hpp"
#ifdef KYOTO_PASS41_IPC
#include "kyoto/official_ipc_candidate_producer.hpp"
#endif
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
namespace f=kyoto::contact::features;
namespace p=kyoto::contact::persistent;
namespace s=kyoto::solid;
namespace at=kyoto::contact::atlas;
using Clock=std::chrono::steady_clock;
using Point=std::array<double,3>;
using Cell=std::array<std::size_t,4>;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
#ifdef KYOTO_PASS41_IPC
constexpr std::array<const char*,12> names{"exhaustive","rebuilt_sap","rebuilt_bvh","retained_refit","retained_frontier","persistent_x","adaptive_axis","persistent_axis","cached_axis","spatial_first","fused","ipc_lbvh"};
#else
constexpr std::array<const char*,11> names{"exhaustive","rebuilt_sap","rebuilt_bvh","retained_refit","retained_frontier","persistent_x","adaptive_axis","persistent_axis","cached_axis","spatial_first","fused"};
#endif
struct Arms {
    at::RefitHierarchy refit,frontier{true,true,false,4,true,true};
    p::OrderedSweep x,axis{true};p::CachedAxisSweep cache{false},spatial{true};p::FusedAxisSweep fused;
    f::Result query(std::size_t i,const f::Mesh& m){
        switch(i){
        case 0:return f::exhaustive(m);case 1:return f::sweep_and_prune(m);
        case 2:{at::RefitHierarchy fresh;return fresh.query(m);}
        case 3:return refit.query(m);case 4:return frontier.query(m);case 5:return x.query(m);
        case 6:return f::adaptive_sweep_and_prune(m);case 7:return axis.query(m);case 8:return cache.query(m);
        case 9:return spatial.query(m);case 10:return fused.query(m);
#ifdef KYOTO_PASS41_IPC
        case 11:return at::official_ipc_candidate_producer(m);
#endif
        default:throw std::logic_error("unknown benchmark arm");}
    }
};
// Reconstruct the exterior triangle mesh of the actual independent
// tetrahedra, including disjoint bodies. Dense feature IDs are required
// by the IPC adapter and used by every arm, not just the external one.
static f::Mesh surface(const std::vector<Point>& x,const std::vector<Point>& y,const std::vector<Cell>& cells){
    std::map<std::array<std::size_t,3>,unsigned> counts;
    for(const auto& c:cells)for(int omitted=0;omitted<4;++omitted){std::array<std::size_t,3> t{};int k=0;for(int j=0;j<4;++j)if(j!=omitted)t[k++]=c[j];std::sort(t.begin(),t.end());++counts[t];}
    std::set<std::size_t> ids;for(const auto& [t,n]:counts){need(n==1||n==2,"nonmanifold independent volume");if(n==1)ids.insert(t.begin(),t.end());}
    f::Mesh m;m.thickness=.0005;std::map<std::size_t,std::uint64_t> dense;std::set<std::array<std::uint64_t,2>> edges;
    for(auto i:ids){const auto a=x.at(i),b=y.at(i);const kyoto::contact::Vec3 u{a[0],a[1],a[2]},v{b[0],b[1],b[2]};dense.emplace(i,m.vertices.size());m.vertices.push_back({m.vertices.size(),{{u,u,v,v}}});}
    for(const auto& [t,n]:counts)if(n==1){const std::array<std::uint64_t,3> face{dense.at(t[0]),dense.at(t[1]),dense.at(t[2])};m.faces.push_back({m.faces.size(),face});for(int k=0;k<3;++k)edges.insert({std::min(face[k],face[(k+1)%3]),std::max(face[k],face[(k+1)%3])});}
    for(auto e:edges)m.edges.push_back({m.edges.size(),e});
    return m;
}
static void run(const std::string& scene,unsigned width,const std::string& regime,const std::vector<f::Mesh>& motion){
    need(!motion.empty(),"empty benchmark trajectory");std::vector<std::vector<f::Candidate>> truth;
    for(const auto& m:motion)truth.push_back(f::exhaustive(m).pairs);
    constexpr int n=int(names.size());
    // Every backend occupies every execution slot once per matched frame.
    // Cold first frame and warm subsequent frames are paid in all arms.
    for(int block=-1;block<n;++block){Arms arms;
        for(std::size_t leaf=0;leaf<motion.size();++leaf)for(int slot=0;slot<n;++slot){const int arm=(slot+block+n)%n;const auto& m=motion[leaf];
            const auto begin=Clock::now();const auto r=arms.query(std::size_t(arm),m);const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();
            need(ns>0&&r.pairs==truth[leaf],"matched common candidate parity failure");
            if(block>=0)std::cout<<scene<<','<<width<<','<<regime<<','<<block<<','<<leaf<<','<<slot<<','<<names[arm]<<','<<ns<<','<<r.pairs.size()<<','<<m.vertices.size()<<','<<m.edges.size()<<','<<m.faces.size()<<'\n';
        }
    }std::cerr<<"PASS "<<scene<<" leaves="<<motion.size()<<" arms="<<n<<'\n';
}
static void independent(const char* path,const std::string& family,bool concave){
    std::ifstream in(path);need(bool(in),"trajectory missing");
    for(int scene=0;scene<4;++scene){int id,n,t,steps,nu=0;need(bool(in>>id>>n>>t>>steps)&&id==scene&&n>0&&t>0&&steps==32,"bad independent header");
        if(concave)need(bool(in>>nu)&&nu>0&&nu<n,"bad U ownership header");
        std::vector<Cell> cells(t);for(auto& c:cells)for(auto& i:c)need(bool(in>>i)&&i<std::size_t(n),"invalid cell index");
        std::vector<Point> x(n),y(n);auto read=[&](auto& a){for(auto& p:a)for(auto& v:p)need(bool(in>>v)&&std::isfinite(v),"invalid position");};read(x);
        std::vector<f::Mesh> motion;for(int i=0;i<steps;++i){read(y);motion.push_back(surface(x,y,cells));x.swap(y);}
        run(family+"_"+std::to_string(scene),0,"independent",motion);
    }std::string trailing;need(!(in>>trailing),"trailing independent data");
}
int main(int argc,char** argv){try{
    need(argc==5,"usage: adaptive.txt fixed.txt concave.txt maximum-width");const auto max=unsigned(std::stoul(argv[4]));need(max==1||max==4||max==16||max==32||max==64,"invalid width");
    std::cout<<"scene,width,regime,block,leaf,slot,arm,ns,candidates,vertices,edges,faces\n";
    independent(argv[1],"adaptive",false);independent(argv[2],"fixed",false);independent(argv[3],"nonconvex",true);
    for(unsigned w:{1u,4u,16u,32u,64u})if(w<=max)for(int near:{0,1}){
        auto a=s::fixtures::u_solid(w,near?.0025:.02,.1),b=s::fixtures::remote();s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;s::CoupledFEMContext ctx;std::vector<f::Mesh> motion;std::size_t impulses=0;
        for(int step=0;step<4;++step){std::vector<s::CoupledFEMLeaf> leaves;const auto r=s::step_coupled_fem(a,b,.00025,cfg,&leaves,&ctx);need(!leaves.empty()&&r.accepted_leaves==leaves.size(),"native source trace");impulses+=r.self_a.impulse_updates;for(const auto& l:leaves)motion.push_back(kyoto::contact::physical_scaling::snapshot(l.before_a,l.after_a,cfg.skin));}
        need(near?impulses>0:impulses==0,"nonrepresentative native regime");run("native_"+std::to_string(w)+"_"+std::to_string(near),w,near?"near":"separated",motion);
    }
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

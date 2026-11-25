#include "kyoto/fused_axis_sap.hpp"
#include "kyoto/cached_axis_sap.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#include "kyoto/physical_scaling_snapshot.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
namespace f=kyoto::contact::features;namespace p=kyoto::contact::persistent;namespace s=kyoto::solid;
using Clock=std::chrono::steady_clock;using Point=std::array<double,3>;using Cell=std::array<std::size_t,4>;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
// Recover exactly the exterior triangles from the independent tetrahedra.
static f::Mesh make_mesh(const std::vector<Point>& x,const std::vector<Point>& y,const std::vector<Cell>& cells){
    std::map<std::array<std::size_t,3>,unsigned> counts;
    for(const auto& c:cells)for(int omitted=0;omitted<4;++omitted){std::array<std::size_t,3> t{};int k=0;for(int j=0;j<4;++j)if(j!=omitted)t[k++]=c[j];std::sort(t.begin(),t.end());++counts[t];}
    std::set<std::size_t> ids;for(const auto& [t,n]:counts){need(n==1||n==2,"nonmanifold input");if(n==1)ids.insert(t.begin(),t.end());}
    f::Mesh m;m.thickness=.0005;std::set<std::array<std::uint64_t,2>> edges;
    for(auto i:ids){const auto a=x.at(i),b=y.at(i);const kyoto::contact::Vec3 u{a[0],a[1],a[2]},v{b[0],b[1],b[2]};m.vertices.push_back({i,{{u,u,v,v}}});}
    for(const auto& [t,n]:counts)if(n==1){m.faces.push_back({m.faces.size(),t});for(int k=0;k<3;++k)edges.insert({std::min(t[k],t[(k+1)%3]),std::max(t[k],t[(k+1)%3])});}
    for(auto e:edges)m.edges.push_back({m.edges.size(),e});
    return m;
}
static void run(int scene,const std::vector<f::Mesh>& motion){
    std::vector<std::vector<f::Candidate>> truth;for(const auto& m:motion)truth.push_back(f::exhaustive(m).pairs);
    for(int block=-1;block<6;++block){p::OrderedSweep base{true};p::CachedAxisSweep spatial{true};p::FusedAxisSweep fused;
        for(std::size_t i=0;i<motion.size();++i)for(int slot=0;slot<3;++slot){const int arm=(slot+block+3)%3;
            const auto begin=Clock::now();const auto r=arm==0?base.query(motion[i]):arm==1?spatial.query(motion[i]):fused.query(motion[i]);const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();
            need(r.pairs==truth[i],"candidate parity");if(block>=0)std::cout<<scene<<','<<block<<','<<i<<','<<slot<<','<<(arm==0?"persistent_axis":arm==1?"spatial_first":"fused")<<','<<ns<<','<<r.pairs.size()<<','<<r.candidate_aabb_tests<<'\n';
        }
    }std::cerr<<"PASS "<<scene<<" leaves="<<motion.size()<<'\n';
}
int main(int argc,char** argv){
    need(argc==3,"usage: adaptive-trajectory fixed-trajectory");std::cout<<"scene,block,leaf,slot,arm,ns,candidates,box_tests\n";
    for(int family=0;family<2;++family){std::ifstream in(argv[1+family]);need(bool(in),"input");for(int scene=0;scene<4;++scene){int id,n,t,steps;need(bool(in>>id>>n>>t>>steps)&&id==scene&&n>0&&t>0&&steps==32,"header");std::vector<Cell> cells(t);for(auto& c:cells)for(auto& i:c)need(bool(in>>i)&&i<std::size_t(n),"index");std::vector<Point> x(n),y(n);auto read=[&](auto& a){for(auto& p:a)for(auto& v:p)need(bool(in>>v)&&std::isfinite(v),"position");};read(x);std::vector<f::Mesh> motion;for(int i=0;i<steps;++i){read(y);motion.push_back(make_mesh(x,y,cells));x.swap(y);}run(family*10+scene,motion);}std::string extra;need(!(in>>extra),"trailing input");}
    for(unsigned w:{1u,4u,16u,32u,64u})for(int near:{0,1}){auto a=s::fixtures::u_solid(w,near?.0025:.02,.1),b=s::fixtures::remote();s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;s::CoupledFEMContext ctx;std::vector<f::Mesh> motion;std::size_t impulses=0;
        for(int step=0;step<4;++step){std::vector<s::CoupledFEMLeaf> leaves;const auto r=s::step_coupled_fem(a,b,.00025,cfg,&leaves,&ctx);need(!leaves.empty()&&r.accepted_leaves==leaves.size(),"native trace");impulses+=r.self_a.impulse_updates;for(const auto& l:leaves)motion.push_back(kyoto::contact::physical_scaling::snapshot(l.before_a,l.after_a,cfg.skin));}need(near?impulses>0:impulses==0,"nonrepresentative regime");run(100+int(w)*2+near,motion);}
}

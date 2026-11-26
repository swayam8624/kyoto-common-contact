// Independent exact-volume and direct upstream CCD audit of automatically
// decomposed deformable-body trajectories. No upstream LBVH timing claim.
#include "kyoto/fused_axis_sap.hpp"
#include "kyoto/exact_swept_volume_audit.hpp"
#include <tight_inclusion/ccd.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <set>
namespace f=kyoto::contact::features;
namespace p=kyoto::contact::persistent;
namespace g=kyoto::geometry;
using V=Eigen::Vector3d;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Motion{f::Mesh mesh;std::vector<V> x,y;std::vector<int> owner;};
struct Receipt{std::size_t candidates{},vf{},ee{},self_hits{},cross_hits{},misses{};};
static Receipt audit(const Motion& m,double skin,p::FusedAxisSweep& fused){
    Receipt r;
    const auto truth=f::exhaustive(m.mesh).pairs;
    need(truth==fused.query(m.mesh).pairs,"fused candidate mismatch");
    r.candidates=truth.size();
    const auto err=Eigen::Array3d::Constant(-1.);constexpr double tolerance=1e-7;constexpr long iterations=100000;
    auto event=[&](f::Candidate key,bool self,double toi){
        need(std::isfinite(toi)&&toi>=0.&&toi<=1.,"invalid direct CCD TOI");
        if(self)++r.self_hits;else ++r.cross_hits;
        r.misses+=!std::binary_search(truth.begin(),truth.end(),key);
    };
    for(std::size_t i=0;i<m.x.size();++i)for(const auto& face:m.mesh.faces){
        const auto t=face.vertices;if(std::find(t.begin(),t.end(),i)!=t.end())continue;
        ++r.vf;double toi=1.,out_tol=tolerance;
        if(ticcd::vertexFaceCCD(m.x[i],m.x[t[0]],m.x[t[1]],m.x[t[2]],m.y[i],m.y[t[0]],m.y[t[1]],m.y[t[2]],err,skin,toi,tolerance,1.,iterations,out_tol,false))
            event({f::Kind::VertexFace,i,face.id},m.owner[i]==m.owner[t[0]],toi);
    }
    for(std::size_t i=0;i<m.mesh.edges.size();++i)for(std::size_t j=i+1;j<m.mesh.edges.size();++j){
        const auto a=m.mesh.edges[i].vertices,b=m.mesh.edges[j].vertices;
        if(a[0]==b[0]||a[0]==b[1]||a[1]==b[0]||a[1]==b[1])continue;
        ++r.ee;double toi=1.,out_tol=tolerance;
        if(ticcd::edgeEdgeCCD(m.x[a[0]],m.x[a[1]],m.x[b[0]],m.x[b[1]],m.y[a[0]],m.y[a[1]],m.y[b[0]],m.y[b[1]],err,skin,toi,tolerance,1.,iterations,out_tol,false))
            event({f::Kind::EdgeEdge,i,j},m.owner[a[0]]==m.owner[b[0]],toi);
    }
    return r;
}

static Motion prepare(const std::vector<g::Point>& x,const std::vector<g::Point>& y,const std::vector<g::ExactTetIds>& cells,double skin,std::size_t owner_boundary){
    std::map<std::array<std::size_t,3>,unsigned> count;
    for(const auto& c:cells)for(int j=0;j<4;++j){std::array<std::size_t,3> face{};int k=0;for(int i=0;i<4;++i)if(i!=j)face[k++]=c[i];std::sort(face.begin(),face.end());++count[face];}
    std::set<std::size_t> used;for(const auto& [face,n]:count){need(n==1||n==2,"nonmanifold face");if(n==1)used.insert(face.begin(),face.end());}
    Motion m;m.mesh.thickness=skin;std::map<std::size_t,std::size_t> index;std::set<std::array<std::uint64_t,2>> edges;
    for(auto i:used){index[i]=m.x.size();m.x.emplace_back(x[i][0],x[i][1],x[i][2]);m.y.emplace_back(y[i][0],y[i][1],y[i][2]);m.owner.push_back(int(i>=owner_boundary));
        const kyoto::contact::Vec3 a{x[i][0],x[i][1],x[i][2]},b{y[i][0],y[i][1],y[i][2]};m.mesh.vertices.push_back({m.mesh.vertices.size(),{{a,a,b,b}}});}
    for(const auto& [face,n]:count)if(n==1){const std::array<std::uint64_t,3> t{index.at(face[0]),index.at(face[1]),index.at(face[2])};m.mesh.faces.push_back({m.mesh.faces.size(),t});for(int i=0;i<3;++i)edges.insert({std::min(t[i],t[(i+1)%3]),std::max(t[i],t[(i+1)%3])});}
    for(const auto& e:edges)m.mesh.edges.push_back({m.mesh.edges.size(),e});
    return m;
}
int main(int argc,char** argv){
    need(argc==2,"usage: trajectory.txt");std::ifstream in(argv[1]);need(bool(in),"input open failed");
    std::cout<<std::setprecision(17)<<"scene,step,skin,cells,possible_pairs,exact_pairs,certified,candidates,vf,ee,self_hits,cross_hits,missing\n";
    for(int sid=0;sid<4;++sid){int id,n,t,steps,owner_boundary;need(bool(in>>id>>n>>t>>steps>>owner_boundary)&&id==sid&&n>0&&owner_boundary>0&&owner_boundary<n&&t>0&&steps==32,"bad trajectory header");
        std::vector<g::ExactTetIds> cells(t);for(auto& c:cells)for(auto& i:c)need(bool(in>>i)&&i<std::size_t(n),"invalid cell index");
        std::vector<g::Point> x(n),y(n);auto read=[&](auto& a){for(auto& p:a)for(auto& v:p)need(bool(in>>v)&&std::isfinite(v),"bad position");};read(x);
        p::FusedAxisSweep zero,positive;
        for(int step=0;step<steps;++step){read(y);const auto cert=g::audit_exact_swept_volume(x,y,cells,6,false,true,true);need(cert.certified,"unresolved external trajectory volume");
            for(double skin:{0.,.0005}){const auto m=prepare(x,y,cells,skin,owner_boundary);need(f::sweep_and_prune(m.mesh).pairs==f::exhaustive(m.mesh).pairs,"SAP mismatch");const auto r=audit(m,skin,skin==0.?zero:positive);need(!r.self_hits&&!r.cross_hits&&!r.misses,"upstream event on external trajectory");
                std::cout<<sid<<','<<step<<','<<skin<<','<<t<<','<<cert.pair_tests<<','<<std::accumulate(cert.tested.begin(),cert.tested.end(),std::size_t{})<<','<<cert.certified<<','<<r.candidates<<','<<r.vf<<','<<r.ee<<','<<r.self_hits<<','<<r.cross_hits<<','<<r.misses<<'\n';}
            x.swap(y);
        }std::cerr<<"PASS independent trajectory "<<sid<<'\n';
    }std::string extra;need(!(in>>extra),"trailing input");
}

// Independent external implicit contact solver trajectory replay.
#include "kyoto/official_ipc_candidate_producer.hpp"
#include "kyoto/exact_swept_volume_audit.hpp"
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <set>
namespace f=kyoto::contact::features;
namespace at=kyoto::contact::atlas;
namespace g=kyoto::geometry;
using V=Eigen::Vector3d;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Motion{f::Mesh mesh;std::vector<V> x,y;std::vector<int> owner;};
struct Receipt{std::size_t candidates{},vf{},ee{},self_hits{},cross_hits{},misses{};};
static Receipt audit(const Motion& m,double skin,bool exhaustive){
    Receipt r;
    const auto truth=f::exhaustive(m.mesh).pairs;
    const auto upstream=at::official_ipc_candidate_producer(m.mesh).pairs;
    need(truth==upstream,"normalized upstream LBVH candidate mismatch");
    r.candidates=upstream.size();
    Eigen::MatrixXd x(m.x.size(),3),y(m.y.size(),3);
    Eigen::MatrixXi edges(m.mesh.edges.size(),2),faces(m.mesh.faces.size(),3);
    for(std::size_t i=0;i<m.x.size();++i){x.row(i)=m.x[i].transpose();y.row(i)=m.y[i].transpose();}
    for(std::size_t i=0;i<m.mesh.edges.size();++i)for(int j=0;j<2;++j)edges(i,j)=int(m.mesh.edges[i].vertices[j]);
    for(std::size_t i=0;i<m.mesh.faces.size();++i)for(int j=0;j<3;++j)faces(i,j)=int(m.mesh.faces[i].vertices[j]);
    auto test=[&](const f::Candidate& k){
        double toi=1.;bool hit=false,self=false;
        if(k.kind==f::Kind::VertexFace){++r.vf;ipc::FaceVertexCandidate p(k.second,k.first);
            hit=p.ccd(p.dof(x,edges,faces),p.dof(y,edges,faces),toi,skin);
            self=m.owner[k.first]==m.owner[m.mesh.faces[k.second].vertices[0]];
        }else{++r.ee;ipc::EdgeEdgeCandidate p(k.first,k.second);
            hit=p.ccd(p.dof(x,edges,faces),p.dof(y,edges,faces),toi,skin);
            self=m.owner[m.mesh.edges[k.first].vertices[0]]==m.owner[m.mesh.edges[k.second].vertices[0]];}
        if(hit){need(std::isfinite(toi)&&toi>=0.&&toi<=1.,"invalid upstream safe prefix");if(self)++r.self_hits;else ++r.cross_hits;
            r.misses+=!std::binary_search(upstream.begin(),upstream.end(),k);}
    };
    if(exhaustive){
        for(std::size_t i=0;i<m.x.size();++i)for(const auto& face:m.mesh.faces){const auto t=face.vertices;if(std::find(t.begin(),t.end(),i)==t.end())test({f::Kind::VertexFace,i,face.id});}
        for(std::size_t i=0;i<m.mesh.edges.size();++i)for(std::size_t j=i+1;j<m.mesh.edges.size();++j){const auto a=m.mesh.edges[i].vertices,b=m.mesh.edges[j].vertices;if(a[0]!=b[0]&&a[0]!=b[1]&&a[1]!=b[0]&&a[1]!=b[1])test({f::Kind::EdgeEdge,i,j});}
    }else for(const auto& k:upstream)test(k);
    return r;
}

static Motion prepare(const std::vector<g::Point>& x,const std::vector<g::Point>& y,const std::vector<g::ExactTetIds>& cells,double skin){
    std::map<std::array<std::size_t,3>,unsigned> count;
    for(const auto& c:cells)for(int j=0;j<4;++j){std::array<std::size_t,3> face{};int k=0;for(int i=0;i<4;++i)if(i!=j)face[k++]=c[i];std::sort(face.begin(),face.end());++count[face];}
    std::set<std::size_t> used;for(const auto& [face,n]:count){need(n==1||n==2,"nonmanifold face");if(n==1)used.insert(face.begin(),face.end());}
    Motion m;m.mesh.thickness=skin;std::map<std::size_t,std::size_t> index;std::set<std::array<std::uint64_t,2>> edges;
    for(auto i:used){index[i]=m.x.size();m.x.emplace_back(x[i][0],x[i][1],x[i][2]);m.y.emplace_back(y[i][0],y[i][1],y[i][2]);m.owner.push_back(int(i/(x.size()/3)));
        const kyoto::contact::Vec3 a{x[i][0],x[i][1],x[i][2]},b{y[i][0],y[i][1],y[i][2]};m.mesh.vertices.push_back({m.mesh.vertices.size(),{{a,a,b,b}}});}
    for(const auto& [face,n]:count)if(n==1){const std::array<std::uint64_t,3> t{index.at(face[0]),index.at(face[1]),index.at(face[2])};m.mesh.faces.push_back({m.mesh.faces.size(),t});for(int i=0;i<3;++i)edges.insert({std::min(t[i],t[(i+1)%3]),std::max(t[i],t[(i+1)%3])});}
    for(const auto& e:edges)m.mesh.edges.push_back({m.mesh.edges.size(),e});
    return m;
}
int main(int argc,char** argv){
    need(argc==2,"usage: trajectory.txt");std::ifstream in(argv[1]);need(bool(in),"input open failed");
    std::cout<<std::setprecision(17)<<"scene,step,skin,cells,possible_pairs,exact_pairs,certified,candidates,vf,ee,self_hits,cross_hits,missing\n";
    for(int sid=0;sid<4;++sid){int id,n,t,steps;need(bool(in>>id>>n>>t>>steps)&&id==sid&&n>0&&n%3==0&&t>0&&steps==32,"bad trajectory header");
        std::vector<g::ExactTetIds> cells(t);for(auto& c:cells)for(auto& i:c)need(bool(in>>i)&&i<std::size_t(n),"invalid cell index");
        std::vector<g::Point> x(n),y(n);auto read=[&](auto& a){for(auto& p:a)for(auto& v:p)need(bool(in>>v)&&std::isfinite(v),"bad position");};read(x);
        for(int step=0;step<steps;++step){read(y);const auto cert=g::audit_exact_swept_volume(x,y,cells,6,false,true,true);need(cert.certified,"unresolved external trajectory volume");
            for(double skin:{0.,.0005}){const auto m=prepare(x,y,cells,skin);need(f::sweep_and_prune(m.mesh).pairs==f::exhaustive(m.mesh).pairs,"SAP mismatch");const auto r=audit(m,skin,true);need(!r.self_hits&&!r.cross_hits&&!r.misses,"upstream event on external trajectory");
                std::cout<<sid<<','<<step<<','<<skin<<','<<t<<','<<cert.pair_tests<<','<<std::accumulate(cert.tested.begin(),cert.tested.end(),std::size_t{})<<','<<cert.certified<<','<<r.candidates<<','<<r.vf<<','<<r.ee<<','<<r.self_hits<<','<<r.cross_hits<<','<<r.misses<<'\n';}
            x.swap(y);
        }std::cerr<<"PASS independent trajectory "<<sid<<'\n';
    }std::string extra;need(!(in>>extra),"trailing input");
}

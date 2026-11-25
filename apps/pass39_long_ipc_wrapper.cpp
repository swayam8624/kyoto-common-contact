// Pinned upstream IPC LBVH and public narrow-phase wrapper on actual long native leaves.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/official_ipc_candidate_producer.hpp"
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
using namespace kyoto::solid;
namespace at=kyoto::contact::atlas;
namespace f=kyoto::contact::features;
using V=Eigen::Vector3d;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static void same(const Body& a,const Body& b){
    need(a.nodes.size()==b.nodes.size()&&a.time_seconds==b.time_seconds&&a.normal_impulse_step==b.normal_impulse_step&&a.contact_count_step==b.contact_count_step,"metadata diverged");
    for(std::size_t i=0;i<a.nodes.size();++i){const auto& x=a.nodes[i];const auto& y=b.nodes[i];need(x.x.x==y.x.x&&x.x.y==y.x.y&&x.x.z==y.x.z&&x.v.x==y.v.x&&x.v.y==y.v.y&&x.v.z==y.v.z&&x.last_normal_impulse==y.last_normal_impulse,"nodal state diverged");}
}
static V3 rotate(V3 x){return {(.6*x.x-.8*x.z)*.8-.6*x.y,(.6*x.x-.8*x.z)*.6+.8*x.y,.8*x.x+.6*x.z};}
struct Scene{Body a,b;double dt;int steps;bool typed;};
static Scene scene(int id){
    if(id>=4){auto a=fixtures::u_solid(id==4?4:16,.0025,.1),b=fixtures::remote();if(id==5)for(auto* p:{&a,&b})for(auto& n:p->nodes){n.x=rotate(n.x);n.v=rotate(n.v);}return {a,b,.00025,20,true};}
    Material ma,mb;ma.gravity=mb.gravity=ma.damping=mb.damping=0.;ma.density=360;mb.density=480;
    auto a=toroidal_solid(id==2?20:8,2,2,ma),b=toroidal_solid(id==2?20:8,2,2,mb);
    for(auto& n:a.nodes)n.v={id==1?.18:0.,0,.5};
    for(auto& n:b.nodes){n.x.z+=id==3?.44:.366;n.x.x+=id==1?.13:0.;n.v={id==1?-.12:0.,0,-.5};}
    if(id==1||id==2)for(auto* p:{&a,&b})for(auto& n:p->nodes){n.x=rotate(n.x);n.v=rotate(n.v);}
    return {a,b,.0005,24,false};
}
struct Motion{f::Mesh mesh;std::vector<V> x,y;std::vector<int> owner;};
static Motion prepare(const Body& a,const Body& b,const Body& aa,const Body& bb,double padding){
    Motion m;m.mesh.thickness=padding;std::set<std::array<std::uint64_t,2>> edges;
    auto append=[&](const Body& u,const Body& v,int owner){
        need(u.surface_faces==v.surface_faces&&u.nodes.size()==v.nodes.size(),"source topology changed");
        std::map<std::size_t,std::uint64_t> index;
        std::set<std::size_t> used;for(const auto& face:u.surface_faces)for(auto i:face)used.insert(i);
        for(auto i:used){const auto x=u.nodes.at(i).x,y=v.nodes.at(i).x;index[i]=m.x.size();m.x.emplace_back(x.x,x.y,x.z);m.y.emplace_back(y.x,y.y,y.z);m.owner.push_back(owner);
            const kyoto::contact::Vec3 p{x.x,x.y,x.z},q{y.x,y.y,y.z};m.mesh.vertices.push_back({m.mesh.vertices.size(),{{p,p,q,q}}});}
        for(const auto& face:u.surface_faces){const std::array<std::uint64_t,3> t{index.at(face[0]),index.at(face[1]),index.at(face[2])};m.mesh.faces.push_back({m.mesh.faces.size(),t});
            for(int k=0;k<3;++k)edges.insert({std::min(t[k],t[(k+1)%3]),std::max(t[k],t[(k+1)%3])});}
    };
    append(a,aa,0);append(b,bb,1);for(auto e:edges)m.mesh.edges.push_back({m.mesh.edges.size(),e});return m;
}
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
static std::size_t external_calls=0;
static f::Result producer(const f::Mesh& m){++external_calls;return at::official_ipc_candidate_producer(m);}
int main(int argc,char** argv){
    const int first=argc>1?std::stoi(argv[1]):0,last=argc>2?std::stoi(argv[2]):6;
    need(first>=0&&first<last&&last<=6,"invalid scene range");
    std::cout<<std::setprecision(17)<<"scene,step,leaf,dt,skin,exhaustive,candidates,vf,ee,self_hits,cross_hits,missing,external_calls\n";
    std::size_t failures=0,leaves=0;
    for(int id=first;id<last;++id){auto a=scene(id),b=a;CoupledFEMContext ca,cb;CoupledFEMSettings c;c.hierarchy=true;c.typed_self_contact=a.typed;c.reuse_cross_geometry=true;auto opt=c;if(a.typed)opt.external_self_candidates=producer;
        for(int step=0;step<a.steps;++step){std::vector<CoupledFEMLeaf> x,y;const auto r=step_coupled_fem(a.a,a.b,a.dt,c,&x,&ca);const auto q=step_coupled_fem(b.a,b.b,b.dt,opt,&y,&cb);
            same(a.a,b.a);same(a.b,b.b);need(x.size()==r.accepted_leaves&&y.size()==q.accepted_leaves&&!x.empty()&&x.size()==y.size(),"leaf count mismatch");
            need(r.rejected_geometry==q.rejected_geometry&&r.rejected_tets==q.rejected_tets&&r.self_a.impulse_updates==q.self_a.impulse_updates&&r.self_b.impulse_updates==q.self_b.impulse_updates&&r.self_a.normal_impulse==q.self_a.normal_impulse&&r.self_b.normal_impulse==q.self_b.normal_impulse&&r.certified_min_j==q.certified_min_j,"physical decision mismatch");
            for(std::size_t i=0;i<x.size();++i){const auto& l=x[i];const auto& k=y[i];same(l.before_a,k.before_a);same(l.before_b,k.before_b);same(l.after_a,k.after_a);same(l.after_b,k.after_b);need(l.dt==k.dt&&l.depth==k.depth,"leaf time mismatch");
                ++leaves;
                for(double skin:{0.,c.skin}){
                    // Full wrapper enumeration on first and final macrostep of each
                    // trajectory; normalized upstream candidates on every leaf.
                    const bool full=step==0||step==a.steps-1;
                    const auto z=audit(prepare(l.before_a,l.before_b,l.after_a,l.after_b,skin),skin,full);
                    failures+=z.self_hits||z.cross_hits||z.misses;
                    std::cout<<id<<','<<step<<','<<i<<','<<l.dt<<','<<skin<<','<<full<<','<<z.candidates<<','<<z.vf<<','<<z.ee<<','<<z.self_hits<<','<<z.cross_hits<<','<<z.misses<<','<<external_calls<<'\n';
                    if(z.self_hits||z.cross_hits||z.misses)std::cerr<<"UPSTREAM EVENT scene="<<id<<" step="<<step<<" leaf="<<i<<" skin="<<skin<<" self="<<z.self_hits<<" cross="<<z.cross_hits<<" missing="<<z.misses<<'\n';
                }
            }
        }
        std::cerr<<"DONE scene="<<id<<" leaves="<<leaves<<" failures="<<failures<<" external_calls="<<external_calls<<'\n';
    }
    return failures?1:0;
}

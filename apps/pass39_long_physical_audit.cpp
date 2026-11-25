// Audit actual long native trajectories, comparing reference and reused witnesses.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/exact_swept_volume_audit.hpp"
#include <tight_inclusion/ccd.hpp>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
using namespace kyoto::solid;
namespace g=kyoto::geometry;
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
struct Result{std::size_t vf{},ee{},self_hits{},cross_hits{},vf_hits{},ee_hits{},missing{};double first{1.};};
static Result check(const Motion& m,double skin){
    Result r;const auto truth=f::exhaustive(m.mesh).pairs;const auto sweep=f::sweep_and_prune(m.mesh).pairs;need(truth==sweep,"candidate parity");
    const auto err=Eigen::Array3d::Constant(-1.);constexpr double tolerance=1e-7;constexpr long iterations=100000;
    auto event=[&](f::Candidate p,bool self,double toi){need(std::isfinite(toi)&&toi>=0.&&toi<=1.,"invalid upstream TOI");
        if(self)++r.self_hits;else ++r.cross_hits;if(p.kind==f::Kind::VertexFace)++r.vf_hits;else ++r.ee_hits;
        r.missing+=!std::binary_search(truth.begin(),truth.end(),p);r.first=std::min(r.first,toi);};
    for(std::size_t i=0;i<m.x.size();++i)for(const auto& face:m.mesh.faces){const auto t=face.vertices;if(std::find(t.begin(),t.end(),i)!=t.end())continue;++r.vf;
        double toi=1.,out_tol=tolerance;
        if(ticcd::vertexFaceCCD(m.x[i],m.x[t[0]],m.x[t[1]],m.x[t[2]],m.y[i],m.y[t[0]],m.y[t[1]],m.y[t[2]],err,skin,toi,tolerance,1.,iterations,out_tol,false))
            event({f::Kind::VertexFace,i,face.id},m.owner[i]==m.owner[t[0]],toi);}
    for(std::size_t i=0;i<m.mesh.edges.size();++i)for(std::size_t j=i+1;j<m.mesh.edges.size();++j){const auto a=m.mesh.edges[i].vertices,b=m.mesh.edges[j].vertices;
        if(a[0]==b[0]||a[0]==b[1]||a[1]==b[0]||a[1]==b[1])continue;
        ++r.ee;double toi=1.,out_tol=tolerance;
        if(ticcd::edgeEdgeCCD(m.x[a[0]],m.x[a[1]],m.x[b[0]],m.x[b[1]],m.y[a[0]],m.y[a[1]],m.y[b[0]],m.y[b[1]],err,skin,toi,tolerance,1.,iterations,out_tol,false))
            event({f::Kind::EdgeEdge,i,j},m.owner[a[0]]==m.owner[b[0]],toi);}
    return r;
}
static void append(const Body& a,const Body& b,std::vector<g::Point>& x,std::vector<g::Point>& y,std::vector<g::ExactTetIds>& cells){
    if(a.nodes.size()!=b.nodes.size()||a.tets.size()!=b.tets.size())throw std::runtime_error("changed topology");
    const auto off=x.size();
    for(std::size_t i=0;i<a.nodes.size();++i){const auto p=a.nodes[i].x,q=b.nodes[i].x;x.push_back({p.x,p.y,p.z});y.push_back({q.x,q.y,q.z});}
    for(std::size_t i=0;i<a.tets.size();++i){auto t=a.tets[i].n;if(t!=b.tets[i].n)throw std::runtime_error("changed cell order");for(auto& j:t)j+=off;cells.push_back(t);}
}

int main(int argc,char** argv){
    const int first=argc>1?std::stoi(argv[1]):0,last=argc>2?std::stoi(argv[2]):6;
    need(first>=0&&first<last&&last<=6,"invalid scene range");
    std::cout<<std::setprecision(17)<<"scene,step,leaf,dt,cells,pairs,overlap_tests,orientation_unknown,test0,test1,test2,test3,unknown0,unknown1,unknown2,unknown3,certified,vf0,ee0,self0,cross0,missing0,vfskin,eeskin,selfskin,crossskin,missingskin\n";
    std::size_t failures=0,leaves=0;
    for(int id=first;id<last;++id){auto a=scene(id),b=a;CoupledFEMContext ca,cb;CoupledFEMSettings c;c.hierarchy=true;c.typed_self_contact=a.typed;auto opt=c;opt.reuse_cross_geometry=true;
        for(int step=0;step<a.steps;++step){std::vector<CoupledFEMLeaf> x,y;const auto r=step_coupled_fem(a.a,a.b,a.dt,c,&x,&ca);const auto q=step_coupled_fem(b.a,b.b,b.dt,opt,&y,&cb);
            same(a.a,b.a);same(a.b,b.b);need(x.size()==r.accepted_leaves&&y.size()==q.accepted_leaves&&!x.empty()&&x.size()==y.size(),"leaf count mismatch");
            for(std::size_t i=0;i<x.size();++i){const auto& l=x[i];const auto& k=y[i];same(l.before_a,k.before_a);same(l.before_b,k.before_b);same(l.after_a,k.after_a);same(l.after_b,k.after_b);need(l.dt==k.dt&&l.depth==k.depth,"leaf time mismatch");
                std::vector<g::Point> start,end;std::vector<g::ExactTetIds> cells;append(l.before_a,l.after_a,start,end,cells);append(l.before_b,l.after_b,start,end,cells);
                const auto v=g::audit_exact_swept_volume(start,end,cells,6,false,true,true);
                const auto z=check(prepare(l.before_a,l.before_b,l.after_a,l.after_b,0.),0.);
                const auto h=check(prepare(l.before_a,l.before_b,l.after_a,l.after_b,c.skin),c.skin);
                const bool ok=v.certified&&!z.self_hits&&!z.cross_hits&&!z.missing&&!h.self_hits&&!h.cross_hits&&!h.missing;
                failures+=!ok;++leaves;
                std::cout<<id<<','<<step<<','<<i<<','<<l.dt<<','<<v.cells<<','<<v.pair_tests<<','<<v.overlap_tests<<','<<v.orientation_unknown;
                for(auto n:v.tested)std::cout<<','<<n;
                for(auto n:v.unknown)std::cout<<','<<n;
                std::cout<<','<<v.certified<<','<<z.vf<<','<<z.ee<<','<<z.self_hits<<','<<z.cross_hits<<','<<z.missing<<','<<h.vf<<','<<h.ee<<','<<h.self_hits<<','<<h.cross_hits<<','<<h.missing<<'\n'<<std::flush;
                if(!ok)std::cerr<<"UNRESOLVED scene="<<id<<" step="<<step<<" leaf="<<i<<" pair="<<v.first_unknown[0]<<','<<v.first_unknown[1]<<'\n';
            }
        }
        std::cerr<<"DONE scene="<<id<<" leaves="<<leaves<<" failures="<<failures<<'\n';
    }
    return failures?1:0;
}

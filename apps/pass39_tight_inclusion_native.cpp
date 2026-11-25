// Independent CCD using the exact Tight Inclusion 1.0.6 dependency pinned
// by IPC Toolkit. Direct numerical API, not the IPC wrapper or its LBVH.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/feature_broadphase.hpp"
#include <tight_inclusion/ccd.hpp>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
using namespace kyoto::solid;
namespace f=kyoto::contact::features;
using V=Eigen::Vector3d;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
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
static void audit(const char* scene,int step,const Body& a,const Body& b,const Body& aa,const Body& bb,int expected){
    for(double skin:{0.,.0005}){
        const auto m=prepare(a,b,aa,bb,skin);const auto r=check(m,skin);
        std::cout<<scene<<','<<step<<','<<skin<<','<<r.vf<<','<<r.ee<<','<<r.self_hits<<','<<r.cross_hits<<','<<r.vf_hits<<','<<r.ee_hits<<','<<r.missing<<','<<r.first<<'\n'<<std::flush;
        need(r.missing==0,"upstream event absent from conservative candidates");
        if(expected==0)need(r.self_hits==0&&r.cross_hits==0,"upstream possible collision on accepted leaf");
        if(expected==1)need(r.self_hits>0,"self positive control not detected");
        if(expected==2)need(r.cross_hits>0,"cross positive control not detected");
    }
}
static Body tetra(double plane,double direction,double density){
    Body b;b.material.gravity=b.material.damping=0.;b.material.young=12000.;b.material.density=density;
    for(V3 p:{V3{plane,0,0},V3{plane,.2,0},V3{plane,0,.2},V3{plane+.2*direction,0,0}})b.nodes.push_back({p,p,{-direction,0,0},0,0});
    b.add_tet({0,1,2,3});b.surface_faces={{0,1,2},{0,3,1},{1,3,2},{2,3,0}};return b;
}
int main(){
    std::cout<<std::setprecision(17)<<"scene,step,skin,vf_tests,ee_tests,self_hits,cross_hits,vf_hits,ee_hits,missing,earliest\n";
    CoupledFEMSettings c;c.hierarchy=c.typed_self_contact=true;
    for(unsigned width:{1u,4u}){
        auto a=fixtures::u_solid(width),b=fixtures::remote();CoupledFEMContext context;std::size_t impulses=0;
        for(int step=0;step<4;++step){std::vector<CoupledFEMLeaf> trace;const auto r=step_coupled_fem(a,b,.00025,c,&trace,&context);impulses+=r.self_a.impulse_updates;
            need(!trace.empty()&&trace.size()==r.accepted_leaves,"missing native leaf");for(const auto& l:trace)audit(width==1?"U1":"U4",step,l.before_a,l.before_b,l.after_a,l.after_b,0);}
        need(impulses>0,"vacuous self-contact source");
    }
    Material ma,mb;ma.gravity=mb.gravity=ma.damping=mb.damping=0.;ma.density=360;mb.density=480;
    auto a=toroidal_solid(8,2,2,ma),b=toroidal_solid(8,2,2,mb);for(auto& n:a.nodes)n.v={0,0,.5};for(auto& n:b.nodes){n.x.z+=.366;n.v={0,0,-.5};}
    c.typed_self_contact=false;std::size_t impulses=0;
    for(int step=0;step<12;++step){std::vector<CoupledFEMLeaf> trace;const auto r=step_coupled_fem(a,b,.0005,c,&trace);impulses+=r.impulse_updates;
        for(const auto& l:trace)audit("cross_torus",step,l.before_a,l.before_b,l.after_a,l.after_b,0);}
    need(impulses>0,"vacuous cross source");
    c.activation_width=0.;c.max_bisection=0;c.typed_self_contact=true;
    a=fixtures::u_solid();b=fixtures::remote();auto aa=a,bb=b;CoupledFEMStats scratch;TypedSelfAuditor ta,tb;
    coupled_detail::propose_drift(aa,bb,.02,c,scratch,ta,tb);audit("rejected_self",0,a,b,aa,bb,1);
    bool refused=false;try{(void)step_coupled_fem(a,b,.02,c);}catch(const std::runtime_error&){refused=true;}need(refused&&a.time_seconds==0.&&b.time_seconds==0.,"self rollback failed");
    a=tetra(0.,-1.,360.);b=tetra(.006,1.,480.);aa=a;bb=b;c.typed_self_contact=false;scratch={};
    coupled_detail::propose_drift(aa,bb,.004,c,scratch,ta,tb);audit("rejected_cross",0,a,b,aa,bb,2);
    refused=false;try{(void)step_coupled_fem(a,b,.004,c);}catch(const std::runtime_error&){refused=true;}need(refused&&a.time_seconds==0.&&b.time_seconds==0.,"cross rollback failed");
    std::cerr<<"PASS actual native accepted/rejected leaves against direct Tight Inclusion 1.0.6\n";
}

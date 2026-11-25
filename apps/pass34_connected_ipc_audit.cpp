// Pass 34A: independently enumerate ALL admissible surface VF/EE
// stencils of actual connected FEM accepted leaves and a native rejected
// drift proposal, using the pinned official IPC Toolkit narrow phase.
// No IPC solver dynamics are implied. Default IPC CCD tests zero skin;
// the native positive-skin certificate is a separate stronger condition.
#include "kyoto/connected_self_contact_fixture.hpp"
#include <ipc/broad_phase/lbvh.hpp>
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <Eigen/Core>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
namespace s=kyoto::solid;
namespace f=kyoto::contact::features;
namespace at=kyoto::contact::atlas;
using kyoto::contact::Vec3;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Mesh {
    Eigen::MatrixXd x0,x1;
    Eigen::MatrixXi edges,faces;
    std::vector<Vec3> start,end;
    std::vector<at::IndexedEdge> e;
    std::vector<at::IndexedFace> t;
    std::vector<int> owner;
};
static Mesh prepare(const s::Body& a,const s::Body& b,
                    const s::Body& c,const s::Body& d){
    Mesh m;std::set<at::IndexedEdge> edges;
    auto append=[&](const s::Body& before,const s::Body& after,int object){
        need(before.surface_faces==after.surface_faces&&before.nodes.size()==after.nodes.size(),
             "source topology changed");
        std::set<std::size_t> ids;
        for(const auto& face:before.surface_faces)for(auto i:face)ids.insert(i);
        std::map<std::size_t,std::size_t> index;
        for(auto i:ids){
            index[i]=m.start.size();
            const auto p=before.nodes.at(i).x,q=after.nodes.at(i).x;
            m.start.push_back({p.x,p.y,p.z});m.end.push_back({q.x,q.y,q.z});
            m.owner.push_back(object);
        }
        for(const auto& f:before.surface_faces){
            at::IndexedFace t{index.at(f[0]),index.at(f[1]),index.at(f[2])};
            m.t.push_back(t);
            for(int k=0;k<3;++k){
                auto x=t[k],y=t[(k+1)%3];
                edges.insert({std::min(x,y),std::max(x,y)});
            }
        }
    };
    append(a,b,0);append(c,d,1);m.e.assign(edges.begin(),edges.end());
    m.x0.resize(m.start.size(),3);m.x1.resize(m.end.size(),3);
    m.edges.resize(m.e.size(),2);m.faces.resize(m.t.size(),3);
    for(std::size_t i=0;i<m.start.size();++i){
        const auto p=m.start[i],q=m.end[i];
        m.x0.row(i)<<p.x,p.y,p.z;m.x1.row(i)<<q.x,q.y,q.z;
    }
    for(std::size_t i=0;i<m.e.size();++i)for(int j=0;j<2;++j)m.edges(i,j)=int(m.e[i][j]);
    for(std::size_t i=0;i<m.t.size();++i)for(int j=0;j<3;++j)m.faces(i,j)=int(m.t[i][j]);
    return m;
}
struct Receipt {
    std::size_t self_vf{},self_ee{},cross_vf{},cross_ee{},self_hits{},cross_hits{},
                vf_hits{},ee_hits{},misses{},sca_candidates{},ipc_candidates{};
    double earliest{1.};
};
static Receipt audit(const Mesh& m,at::LinearCandidateAdapter& adapter){
    const auto truth=adapter.query(m.start,m.end,m.e,m.t,1,0.,true).pairs;
    const auto sca=adapter.query(m.start,m.end,m.e,m.t).pairs;
    need(truth==sca,"native exhaustive/retained typed candidate mismatch");
    ipc::LBVH bp;bp.can_vertices_collide=[](auto,auto){return true;};
    bp.build(m.x0,m.x1,m.edges,m.faces,0.);
    std::vector<ipc::FaceVertexCandidate> vf;
    std::vector<ipc::EdgeEdgeCandidate> ee;
    bp.detect_face_vertex_candidates(vf);bp.detect_edge_edge_candidates(ee);
    std::vector<f::Candidate> official;
    for(const auto& p:vf)official.push_back({f::Kind::VertexFace,
        std::uint64_t(p.vertex_id),std::uint64_t(p.face_id)});
    for(const auto& p:ee)official.push_back({f::Kind::EdgeEdge,
        std::uint64_t(std::min(p.edge0_id,p.edge1_id)),
        std::uint64_t(std::max(p.edge0_id,p.edge1_id))});
    std::sort(official.begin(),official.end());
    official.erase(std::unique(official.begin(),official.end()),official.end());
    Receipt r;r.sca_candidates=sca.size();r.ipc_candidates=official.size();
    auto event=[&](f::Candidate key,bool self,double toi){
        need(std::isfinite(toi)&&toi>=0.&&toi<=1.,"invalid IPC safe prefix");
        if(self)++r.self_hits;else ++r.cross_hits;
        if(key.kind==f::Kind::VertexFace)++r.vf_hits;else ++r.ee_hits;
        r.earliest=std::min(r.earliest,toi);
        r.misses+=!std::binary_search(sca.begin(),sca.end(),key);
        r.misses+=!std::binary_search(official.begin(),official.end(),key);
    };
    // No broad-phase culling in this event enumeration.
    for(std::size_t v=0;v<m.start.size();++v)for(std::size_t j=0;j<m.t.size();++j){
        const auto& face=m.t[j];
        if(std::find(face.begin(),face.end(),v)!=face.end())continue;
        const bool self=m.owner[v]==m.owner[face[0]];
        if(self)++r.self_vf;else ++r.cross_vf;
        ipc::FaceVertexCandidate p(j,v);double toi=1.;
        if(p.ccd(p.dof(m.x0,m.edges,m.faces),p.dof(m.x1,m.edges,m.faces),toi))
            event({f::Kind::VertexFace,v,j},self,toi);
    }
    for(std::size_t i=0;i<m.e.size();++i)for(std::size_t j=i+1;j<m.e.size();++j){
        const auto a=m.e[i],b=m.e[j];
        if(a[0]==b[0]||a[0]==b[1]||a[1]==b[0]||a[1]==b[1])continue;
        const bool self=m.owner[a[0]]==m.owner[b[0]];
        if(self)++r.self_ee;else ++r.cross_ee;
        ipc::EdgeEdgeCandidate p(i,j);double toi=1.;
        if(p.ccd(p.dof(m.x0,m.edges,m.faces),p.dof(m.x1,m.edges,m.faces),toi))
            event({f::Kind::EdgeEdge,i,j},self,toi);
    }
    return r;
}
static void write(std::ostream& o,const char* type,std::size_t id,double t,double dt,
                  const Receipt& r){
    o<<type<<','<<id<<','<<t<<','<<dt<<','<<r.self_vf<<','<<r.self_ee<<','
     <<r.cross_vf<<','<<r.cross_ee<<','<<r.self_hits<<','<<r.cross_hits<<','
     <<r.vf_hits<<','<<r.ee_hits<<','<<r.misses<<','<<r.sca_candidates<<','
     <<r.ipc_candidates<<','<<r.earliest<<'\n';
}
int main(int argc,char** argv){
    try{
        if(argc!=2)throw std::invalid_argument("usage: pass34 audit OUTPUT_DIR");
        const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
        std::ofstream csv(dir/"connected_native_ipc_event_audit.csv");
        csv<<std::setprecision(17)
           <<"type,leaf,start_s,dt_s,self_vf,self_ee,cross_vf,cross_ee,self_hits,cross_hits,vf_hits,ee_hits,candidate_misses,sca_candidates,ipc_candidates,earliest_safe_prefix\n";
        s::Body a=s::fixtures::u_solid(),b=s::fixtures::remote();
        s::CoupledFEMSettings cfg;cfg.typed_self_contact=true;cfg.hierarchy=true;
        at::LinearCandidateAdapter adapter;
        std::size_t count=0,impulses=0,stencils=0;
        for(int i=0;i<8;++i){
            std::vector<s::CoupledFEMLeaf> leaves;
            const auto step=s::step_coupled_fem(a,b,.00025,cfg,&leaves);
            need(leaves.size()==step.accepted_leaves,"incomplete native leaf trace");
            impulses+=step.self_a.impulse_updates;
            for(const auto& leaf:leaves){
                const auto r=audit(prepare(leaf.before_a,leaf.after_a,
                                           leaf.before_b,leaf.after_b),adapter);
                write(csv,"accepted",count++,leaf.before_a.time_seconds,leaf.dt,r);
                need(r.self_hits==0&&r.cross_hits==0&&r.misses==0,
                     "official IPC reported event on accepted native motion");
                stencils+=r.self_vf+r.self_ee+r.cross_vf+r.cross_ee;
            }
        }
        need(count>=8&&impulses>0&&stencils>0,"accepted audit lacked physical contact or stencil coverage");
        // Same actual native kick/impulse/drift path, with activation
        // deliberately disabled outside the positive skin. The top arms
        // cross under a large full trial; no accepted state is fabricated.
        a=s::fixtures::u_solid();b=s::fixtures::remote();
        cfg.activation_width=0.;cfg.max_bisection=0;
        constexpr double dt=.02;
        auto proposed_a=a,proposed_b=b;
        s::CoupledFEMStats scratch;s::TypedSelfAuditor ta,tb;
        s::coupled_detail::propose_drift(proposed_a,proposed_b,dt,cfg,scratch,ta,tb);
        need(!ta.audit(a,proposed_a,cfg.skin).separated,"positive trial did not violate native typed safety");
        bool rejected=false;
        try{(void)s::step_coupled_fem(a,b,dt,cfg);}
        catch(const std::runtime_error&){rejected=true;}
        need(rejected&&a.time_seconds==0.&&b.time_seconds==0.,"positive trial not rejected atomically");
        const auto pos=audit(prepare(a,proposed_a,b,proposed_b),adapter);
        write(csv,"rejected_positive",count,0.,dt,pos);
        need(pos.self_hits>0&&pos.misses==0,"rejected native positive CCD event missing");
        need(csv.good(),"IPC receipt write failed");
        std::cout<<"PASS34 native accepted leaves="<<count<<" exhaustive stencil tests="<<stencils
                 <<" native self impulses="<<impulses<<" rejected positive self events="<<pos.self_hits
                 <<" candidate misses="<<pos.misses<<" upstream earliest prefix="<<pos.earliest<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

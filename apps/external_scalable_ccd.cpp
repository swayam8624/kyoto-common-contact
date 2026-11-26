// External sample data: frozen Kyoto FusedAxisSweep versus upstream Scalable CCD.
#include "kyoto/fused_axis_sap.hpp"
#include <scalable_ccd/broad_phase/sort_and_sweep.hpp>
#include <tbb/global_control.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <iomanip>
namespace f=kyoto::contact::features;
namespace sc=scalable_ccd;
using Clock=std::chrono::steady_clock;
using Vec=kyoto::contact::Vec3;
static void need(bool x,const char* msg){if(!x)throw std::runtime_error(msg);}
struct Input{std::vector<std::vector<Vec>> frames;std::vector<f::Edge> edges;std::vector<f::Face> faces;};
static Input load(const char* p){
    std::ifstream in(p);std::size_t n,e,g,s;need(bool(in>>n>>e>>g>>s)&&n&&e&&g&&s>=2,"header");
    Input x;x.edges.resize(e);x.faces.resize(g);x.frames.resize(s,std::vector<Vec>(n));
    for(std::size_t i=0;i<e;++i){x.edges[i].id=i;for(auto& k:x.edges[i].vertices)need(bool(in>>k)&&k<n,"edge");}
    for(std::size_t i=0;i<g;++i){x.faces[i].id=i;for(auto& k:x.faces[i].vertices)need(bool(in>>k)&&k<n,"face");}
    for(auto& a:x.frames)for(auto& v:a)need(bool(in>>v.x>>v.y>>v.z)&&kyoto::contact::finite(v),"vertex");
    std::string t;need(!(in>>t),"trailing input");return x;
}
static f::Mesh make(const Input& x,std::size_t k){
    f::Mesh m;m.edges=x.edges;m.faces=x.faces;
    for(std::size_t i=0;i<x.frames[k].size();++i){auto a=x.frames[k][i],b=x.frames[k+1][i];m.vertices.push_back({i,{{a,a,b,b}}});}
    return m;
}
static std::vector<sc::AABB> convert(const std::vector<f::Record>& rs){
    std::vector<sc::AABB> out;out.reserve(rs.size());
    for(auto& r:rs){
        sc::ArrayMax3 l(3),h(3);l<<r.bounds.min.x,r.bounds.min.y,r.bounds.min.z;h<<r.bounds.max.x,r.bounds.max.y,r.bounds.max.z;
        sc::AABB a(l,h);a.element_id=static_cast<long>(r.id);
        for(unsigned k=0;k<3;++k)a.vertex_ids[k]=static_cast<long>(r.ids[k<r.arity?k:0]);
        out.push_back(a);
    }return out;
}
struct Upstream{
    int ee=0,vf=0;
    f::Result finish(std::vector<sc::AABB> v,std::vector<sc::AABB> e,std::vector<sc::AABB> faces){
        std::vector<std::pair<int,int>> p,q;
        sc::sort_and_sweep(std::move(e),ee,p);
        sc::sort_and_sweep(std::move(v),std::move(faces),vf,q);
        f::Result r;for(auto [a,b]:p)r.pairs.push_back({f::Kind::EdgeEdge,std::uint64_t(a),std::uint64_t(b)});
        for(auto [a,b]:q)r.pairs.push_back({f::Kind::VertexFace,std::uint64_t(a),std::uint64_t(b)});
        f::normalize(r);return r;
    }
    f::Result common(const f::Mesh& m){
        auto b=f::build(m);return finish(convert(b.vertices),convert(b.edges),convert(b.faces));
    }
    f::Result stock(const f::Mesh& m){
        Eigen::MatrixXd a(m.vertices.size(),3),b(m.vertices.size(),3);
        Eigen::MatrixXi e(m.edges.size(),2),g(m.faces.size(),3);
        for(std::size_t i=0;i<m.vertices.size();++i){auto x=m.vertices[i].path.control[0],y=m.vertices[i].path.control[3];a.row(i)<<x.x,x.y,x.z;b.row(i)<<y.x,y.y,y.z;}
        for(std::size_t i=0;i<m.edges.size();++i)for(int j=0;j<2;++j)e(i,j)=int(m.edges[i].vertices[j]);
        for(std::size_t i=0;i<m.faces.size();++i)for(int j=0;j<3;++j)g(i,j)=int(m.faces[i].vertices[j]);
        std::vector<sc::AABB> v,es,fs;sc::build_vertex_boxes(a,b,v,0);sc::build_edge_boxes(v,e,es);sc::build_face_boxes(v,g,fs);
        return finish(std::move(v),std::move(es),std::move(fs));
    }
};
int main(int argc,char** argv){try{
    need(argc==4,"usage: external input patch|full threads");const auto x=load(argv[1]);const bool patch=std::string(argv[2])=="patch";need(patch||std::string(argv[2])=="full","scope");
    const auto threads=std::stoul(argv[3]);need(threads>0,"threads");
    tbb::global_control cap(tbb::global_control::max_allowed_parallelism,threads);
    std::vector<f::Mesh> m;for(std::size_t i=0;i+1<x.frames.size();++i)m.push_back(make(x,i));
    std::vector<f::Result> ref;
    for(auto& a:m)ref.push_back(patch?f::exhaustive(a):f::sweep_and_prune(a));
    std::cout<<"block,frame,slot,arm,ns,candidates,missing,extra,vertices,edges,faces,threads\n";
    // Warm up all paths; each measured block begins with cold persistent state.
    for(int b=-1;b<6;++b){
        kyoto::contact::persistent::FusedAxisSweep fused;Upstream common,stock;
        for(std::size_t i=0;i<m.size();++i)for(int slot=0;slot<3;++slot){
            const int arm=(slot+b+3)%3;auto start=Clock::now();
            f::Result z=arm==0?fused.query(m[i]):arm==1?common.common(m[i]):stock.stock(m[i]);
            auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
            std::vector<f::Candidate> miss,extra;
            std::set_difference(ref[i].pairs.begin(),ref[i].pairs.end(),z.pairs.begin(),z.pairs.end(),std::back_inserter(miss));
            std::set_difference(z.pairs.begin(),z.pairs.end(),ref[i].pairs.begin(),ref[i].pairs.end(),std::back_inserter(extra));
            // Stock AABB inflation is independent; record differences, do not silently normalize away.
            need(arm==2|| (miss.empty()&&extra.empty()),"common contract candidate mismatch");
            if(b>=0)std::cout<<b<<','<<i<<','<<slot<<','<<(arm==0?"fused":arm==1?"scalable_common":"scalable_stock")<<','<<ns<<','<<z.pairs.size()<<','<<miss.size()<<','<<extra.size()<<','<<m[i].vertices.size()<<','<<m[i].edges.size()<<','<<m[i].faces.size()<<','<<threads<<'\n';
        }
    }
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

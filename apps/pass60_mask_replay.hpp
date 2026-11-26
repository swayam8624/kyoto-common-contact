#pragma once
// Exact same archived physical inputs as Pass 59. Compare raw device
// masks before host binary64 filtering, then canonical candidate vectors.
#include "kyoto/tiled_gpu_mask.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace f=kyoto::contact::features;
namespace g=kyoto::contact::gpu_mask;
namespace t=kyoto::contact::gpu_tiles;
using Point=std::array<double,3>;
static void need(bool b,const char* message){if(!b)throw std::runtime_error(message);}
struct ReplayFrame {
    std::vector<Point> a,b;
    std::vector<std::array<std::size_t,2>> edges;
    std::vector<std::array<std::size_t,3>> faces;
};
static std::vector<ReplayFrame> replay_load(const std::string& path){
    std::ifstream in(path);std::size_t n,e,f,s;
    need(bool(in>>n>>e>>f>>s)&&n&&s>=2,"bad replay header");
    ReplayFrame frame;frame.a.resize(n);frame.b.resize(n);
    frame.edges.resize(e);frame.faces.resize(f);
    for(auto& x:frame.edges)for(auto& i:x)need(bool(in>>i)&&i<n,"bad edge");
    for(auto& x:frame.faces)for(auto& i:x)need(bool(in>>i)&&i<n,"bad face");
    auto read=[&](auto& x){for(auto& p:x)for(auto& v:p)need(bool(in>>v)&&std::isfinite(v),"bad point");};
    read(frame.a);std::vector<ReplayFrame> out;
    for(std::size_t i=1;i<s;++i){read(frame.b);out.push_back(frame);frame.a=frame.b;}
    std::string trailing;need(!(in>>trailing),"trailing replay data");return out;
}
static f::Mesh replay_mesh(const ReplayFrame& frame,double skin){
    f::Mesh m;m.thickness=skin;
    for(std::size_t i=0;i<frame.a.size();++i){
        auto a=frame.a[i],b=frame.b[i];
        kyoto::contact::Vec3 u{a[0],a[1],a[2]},v{b[0],b[1],b[2]};
        m.vertices.push_back({i,{{u,u,v,v}}});
    }
    for(auto e:frame.edges)m.edges.push_back({m.edges.size(),{e[0],e[1]}});
    for(auto f:frame.faces)m.faces.push_back({m.faces.size(),{f[0],f[1],f[2]}});
    return m;
}
static ReplayFrame replay_replicate(const ReplayFrame& m,unsigned copies,double pitch){
    ReplayFrame r;
    for(unsigned k=0;k<copies;++k){
        auto off=r.a.size();
        for(std::size_t i=0;i<m.a.size();++i){
            auto a=m.a[i],b=m.b[i];a[0]+=k*pitch;b[0]+=k*pitch;
            r.a.push_back(a);r.b.push_back(b);
        }
        for(auto e:m.edges){for(auto& i:e)i+=off;r.edges.push_back(e);}
        for(auto f:m.faces){for(auto& i:f)i+=off;r.faces.push_back(f);}
    }
    return r;
}

struct ScalarMask {
    std::vector<std::uint32_t> packed(const std::vector<g::Box>& a,const std::vector<g::Box>& b,g::Params p){
        std::vector<std::uint32_t> out(g::words(p.count));
        for(std::size_t i=0;i<out.size();++i)out[i]=g::packed_word(a.data(),b.data(),p,std::uint32_t(i));
        return out;
    }
    std::vector<std::uint32_t> tiled(const std::vector<g::Box>& a,const std::vector<g::Box>& b,const std::vector<t::Job>& jobs){
        std::vector<std::uint32_t> out(jobs.size()*32);
        for(std::size_t i=0;i<out.size();++i)out[i]=t::word(a.data(),b.data(),jobs.data(),std::uint32_t(jobs.size()),std::uint32_t(i));
        return out;
    }
};
template<class Device>
static void campaign(Device& device,const std::string& root,std::ostream& csv){
    ScalarMask scalar;
    csv<<"scene,width,regime,leaf,candidates,packed_dispatches,tiled_dispatches,packed_words,tiled_words\n";
    std::size_t frames=0;
    auto check=[&](const std::string& name,unsigned width,const std::string& regime,std::size_t i,const f::Mesh& mesh){
        std::uint64_t pc=0,tc=0,pw=0,tw=0;
        auto p=g::query(mesh,[&](const auto& a,const auto& b,g::Params p){
            auto raw=device.packed(a,b,p);need(raw==scalar.packed(a,b,p),"packed raw mask mismatch");++pc;pw+=raw.size();return raw;
        });
        t::Work work;
        auto q=t::query(mesh,[&](const auto& a,const auto& b,const auto& jobs){
            auto raw=device.tiled(a,b,jobs);need(raw==scalar.tiled(a,b,jobs),"tiled raw mask mismatch");++tc;tw+=raw.size();return raw;
        },work);
        auto truth=f::exhaustive(mesh).pairs;
        need(p.pairs==truth&&q.pairs==truth,"canonical candidate mismatch");
        need(pc>0&&pw>0&&tw==work.mask_words,"vacuous or inconsistent dispatch");
        csv<<name<<','<<width<<','<<regime<<','<<i<<','<<truth.size()<<','<<pc<<','<<tc<<','<<pw<<','<<tw<<'\n';++frames;
    };
    auto replay=[&](const std::string& name,unsigned width,const std::vector<ReplayFrame>& input,double skin){
        for(std::size_t i=0;i<input.size();++i)check(name,width,skin==0.?"zero_skin":"half_mm",i,replay_mesh(input[i],skin));
    };
    // Precision controls also used by the original physical Metal benchmark.
    for(double scale:{1.,16777216.,1e-40}){
        f::Mesh m;const double dx=scale==16777216.?.25:scale*.125;
        for(const auto p:{kyoto::contact::Vec3{scale,0,0},{scale+dx,0,0},{scale,dx,0},{scale+dx*.5,dx*.5,0},{scale,0,dx},{scale+dx,dx,0}})
            m.vertices.push_back({m.vertices.size(),{{p,p,p,p}}});
        m.faces={{0,{0,1,2}}};m.edges={{0,{0,2}},{1,{1,3}},{2,{4,5}}};
        need(!f::exhaustive(m).pairs.empty(),"vacuous precision case");check("precision",1,"control",frames,m);
    }
    std::vector<ReplayFrame> base;
    for(int i=0;i<5;++i){auto m=replay_load(root+"/scene"+std::to_string(i)+".txt");if(i==2)base=m;for(double skin:{0.,.0005})replay("physical"+std::to_string(i),1,m,skin);}
    need(base.size()>=22,"short comb trajectory");
    for(unsigned copies:{2u,4u,8u})for(double pitch:{.25,.015}){
        std::vector<ReplayFrame> q;for(unsigned i=0;i<8;++i)q.push_back(replay_replicate(base.at(i*3),copies,pitch));
        replay("replica"+std::to_string(copies)+(pitch>.1?"_sparse":"_dense"),copies,q,.0005);
    }
    std::vector<ReplayFrame> q;
    for(unsigned i=0;i<12;++i){auto m=base.at(i);if(i%2){for(auto& p:m.a)p[0]=-p[0];for(auto& p:m.b)p[0]=-p[0];}if(i%3==0){std::reverse(m.edges.begin(),m.edges.end());std::reverse(m.faces.begin(),m.faces.end());}q.push_back(m);}
    replay("reversal_topology",1,q,.0005);
    need(frames==303&&csv.good(),"incomplete parity replay");std::cerr<<"PASS 303 raw-mask and canonical candidate frames\n";
}

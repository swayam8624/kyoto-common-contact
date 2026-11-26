// Reuse original Pass 41 production arms and paid-query protocol, not
// the independent Pass 47 reference implementations. Compile with the
// same include/dependency flags as Pass 41. Stock IPC is optional.
#define main pass41_original_main
#include "pass41_common_campaign.cpp"
#undef main
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
static void replay_run(const std::string& name,unsigned copies,const std::vector<ReplayFrame>& input,double skin){
    std::vector<f::Mesh> motion;for(const auto& x:input)motion.push_back(replay_mesh(x,skin));
    run(name,copies,skin==0.?"zero_skin":"half_mm",motion);
}
int main(int argc,char** argv){try{
    need(argc==2,"usage: pass47_repository_replay inputdir");
    std::cout<<"scene,width,regime,block,leaf,slot,arm,ns,candidates,vertices,edges,faces\n";
    std::vector<ReplayFrame> base;
    for(int i=0;i<5;++i){
        auto m=replay_load(std::string(argv[1])+"/scene"+std::to_string(i)+".txt");
        if(i==2)base=m;
        for(double skin:{0.,.0005})replay_run("physical"+std::to_string(i),1,m,skin);
    }
    need(base.size()>=22,"comb trajectory too short");
    for(unsigned copies:{2u,4u,8u})for(double pitch:{.25,.015}){
        std::vector<ReplayFrame> q;
        for(unsigned i=0;i<8;++i)q.push_back(replay_replicate(base.at(i*3),copies,pitch));
        replay_run("replica"+std::to_string(copies)+(pitch>.1?"_sparse":"_dense"),copies,q,.0005);
    }
    std::vector<ReplayFrame> q;
    for(unsigned i=0;i<12;++i){
        auto m=base.at(i);
        if(i%2){for(auto& p:m.a)p[0]=-p[0];for(auto& p:m.b)p[0]=-p[0];}
        if(i%3==0){std::reverse(m.edges.begin(),m.edges.end());std::reverse(m.faces.begin(),m.faces.end());}
        q.push_back(m);
    }
    replay_run("reversal_topology",1,q,.0005);return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

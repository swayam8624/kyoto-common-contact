// Full native solver comparison, identical geometry and impulse order.
#include "kyoto/physical_scaling_snapshot.hpp"
#include "kyoto/cached_axis_sap.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
using namespace kyoto::solid;
using Clock=std::chrono::steady_clock;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
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

namespace f=kyoto::contact::features;
using kyoto::contact::physical_scaling::snapshot;
static f::Mesh combined(const CoupledFEMLeaf& l,double skin){
    auto a=snapshot(l.before_a,l.after_a,skin),b=snapshot(l.before_b,l.after_b,skin);
    const auto off=a.vertices.size();for(auto v:b.vertices){v.id+=off;v.object=1;a.vertices.push_back(v);}
    for(auto e:b.edges){e.id=a.edges.size();for(auto& i:e.vertices)i+=off;a.edges.push_back(e);}
    for(auto face:b.faces){face.id=a.faces.size();for(auto& i:face.vertices)i+=off;a.faces.push_back(face);}
    return a;
}
static void campaign(int id,Scene sc){
    CoupledFEMSettings c;c.hierarchy=true;c.typed_self_contact=sc.typed;c.reuse_cross_geometry=true;CoupledFEMContext ctx;std::vector<f::Mesh> meshes;
    std::size_t impulses=0;
    for(int i=0;i<sc.steps;++i){std::vector<CoupledFEMLeaf> trace;const auto r=step_coupled_fem(sc.a,sc.b,sc.dt,c,&trace,&ctx);need(!trace.empty()&&trace.size()==r.accepted_leaves,"missing leaves");impulses+=r.impulse_updates+r.self_a.impulse_updates+r.self_b.impulse_updates;
        for(const auto& l:trace)meshes.push_back(combined(l,c.skin));}
    std::vector<std::vector<f::Candidate>> refs;for(const auto& m:meshes)refs.push_back(f::exhaustive(m).pairs);
    for(int block=-1;block<6;++block){kyoto::contact::persistent::OrderedSweep sap{true};kyoto::contact::persistent::CachedAxisSweep cache,fast{true};
        for(std::size_t i=0;i<meshes.size();++i)for(int slot=0;slot<3;++slot){const int arm=(slot+block+3)%3;
            const auto t0=Clock::now();const auto r=arm==0?sap.query(meshes[i]):arm==1?cache.query(meshes[i]):fast.query(meshes[i]);const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t0).count();
            need(r.pairs==refs[i],"candidate mismatch");if(block>=0)std::cout<<id<<','<<block<<','<<i<<','<<slot<<','<<(arm==0?"persistent_axis":arm==1?"cached_axis":"spatial_first")<<','<<ns<<','<<r.pairs.size()<<','<<r.candidate_aabb_tests<<','<<r.incident_rejections<<','<<r.mask_rejections<<'\n';}
    }
    std::cerr<<"PASS scene="<<id<<" leaves="<<meshes.size()<<" impulses="<<impulses<<'\n';
}
int main(int argc,char** argv){
    const int only=argc==2?std::stoi(argv[1]):-1;
    if(argc>2)throw std::invalid_argument("usage: campaign [minimum scene ID]");std::cout<<"scene,block,leaf,slot,arm,ns,candidates,box_tests,incident_rejections,mask_rejections\n";
    for(int id=0;id<6;++id)if(id>=only)campaign(id,scene(id));
    for(unsigned w:{1u,4u,16u,32u,64u})for(int near:{0,1})if(100+int(w)*2+near>=only)campaign(100+int(w)*2+near,{fixtures::u_solid(w,near?.0025:.02,.1),fixtures::remote(),.00025,4,true});
}

// Full native solver comparison, identical geometry and impulse order.
#include "kyoto/connected_self_contact_fixture.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
using namespace kyoto::solid;
using Clock=std::chrono::steady_clock;
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
struct Run{Scene s;CoupledFEMContext context;double ns{};std::size_t leaves{},cross{},self{},rejected{},boxes{},certs{};double min_j{1};};
int main(){
    std::cout<<std::setprecision(17)<<"scene,block,slot,arm,steps,elapsed_ns,leaves,cross_impulses,self_impulses,rejected,box_tests,certificate_tests,min_j\n";
    for(int id=0;id<6;++id){const auto source=scene(id);
        for(int block=-1;block<3;++block){
            std::vector<Run> runs;for(int slot=0;slot<4;++slot)runs.push_back({source,{},0,0,0,0,0,0,0,1});
            for(int step=0;step<source.steps;++step){
                for(int slot=0;slot<4;++slot){auto& r=runs[slot];CoupledFEMSettings c;c.hierarchy=true;c.typed_self_contact=source.typed;c.reuse_cross_geometry=(slot==0||slot==3)==(block%2==0);
                    const auto begin=Clock::now();const auto z=step_coupled_fem(r.s.a,r.s.b,source.dt,c,nullptr,&r.context);r.ns+=std::chrono::duration<double,std::nano>(Clock::now()-begin).count();
                    r.leaves+=z.accepted_leaves;r.cross+=z.impulse_updates;r.self+=z.self_a.impulse_updates+z.self_b.impulse_updates;r.rejected+=z.rejected_geometry+z.rejected_tets;r.boxes+=z.box_tests;r.certs+=z.certificate_tests;r.min_j=std::min(r.min_j,z.certified_min_j);
                }
                for(int i=1;i<4;++i){const auto& a=runs[0];const auto& b=runs[i];same(a.s.a,b.s.a);same(a.s.b,b.s.b);need(a.leaves==b.leaves&&a.cross==b.cross&&a.self==b.self&&a.rejected==b.rejected&&a.certs==b.certs&&a.min_j==b.min_j,"decision receipt diverged");}
            }
            for(int slot=0;slot<4;++slot){const auto& r=runs[slot];need(id<3?r.cross>0:id==3?r.cross==0:r.self>0,"vacuous workload");if(block>=0)std::cout<<id<<','<<block<<','<<slot<<','<<(((slot==0||slot==3)==(block%2==0))?"reuse":"reference")<<','<<source.steps<<','<<r.ns<<','<<r.leaves<<','<<r.cross<<','<<r.self<<','<<r.rejected<<','<<r.boxes<<','<<r.certs<<','<<r.min_j<<'\n'<<std::flush;}
        }
        std::cerr<<"PASS scene="<<id<<" tets="<<source.a.tets.size()+source.b.tets.size()<<" steps="<<source.steps<<'\n';
    }
}

#include "kyoto/cached_axis_sap.hpp"
#include <iostream>
#include <random>
namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
using kyoto::contact::persistent::CachedAxisSweep;
static void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static void same(CachedAxisSweep& s,const f::Mesh& m){need(s.query(m).pairs==f::exhaustive(m).pairs,"cached sweep parity failure");}
int main(){
    std::mt19937_64 rng(39251126);std::uniform_int_distribution<int> d(-64,64);
    std::size_t checked=0;
    for(int trial=0;trial<500;++trial){
        CachedAxisSweep s;f::Mesh m;
        auto pos=[&](){return Vec3{d(rng)/16.,d(rng)/16.,d(rng)/16.};};
        for(unsigned i=0;i<24;++i){const auto p=pos(),q=pos();m.vertices.push_back({100+3*i,{{p,p,q,q}}});}
        for(unsigned i=0;i<24;i+=3)m.faces.push_back({20+i,{100+3*i,103+3*i,106+3*i}});
        for(unsigned i=0;i<24;i+=2)m.edges.push_back({50+i,{100+3*i,103+3*i}});
        for(int frame=0;frame<8;++frame){
            for(auto& v:m.vertices){const auto p=pos(),q=pos();v.path={{p,p,q,q}};}
            if(frame==2)m.thickness=.125;
            if(frame==3)m.vertices[0].mask=0;
            if(frame==4)std::reverse(m.faces.begin(),m.faces.end());
            if(frame==5)++m.topology_epoch;
            if(frame==6)m.edges[0].vertices[0]=m.vertices[3].id;
            if(frame==7)std::reverse(m.vertices.begin(),m.vertices.end());
            same(s,m);++checked;
        }
        need(s.rebuilds()==6&&s.warm_frames()==2,"cache invalidation counts mismatch");
        auto broken=m;broken.vertices.back().path.control[0].x=std::numeric_limits<double>::quiet_NaN();
        bool rejected=false;try{(void)s.query(broken);}catch(const std::invalid_argument&){rejected=true;}need(rejected,"nonfinite control accepted");same(s,m);
    }
    // Severe reversal on a long sparse mesh triggers bounded repair fallback.
    CachedAxisSweep s;f::Mesh m;
    for(unsigned i=0;i<512;++i){Vec3 p{double(i),0.,0.};m.vertices.push_back({i,{{p,p,p,p}}});}
    for(unsigned i=0;i<512;i+=2)m.edges.push_back({i/2,{i,i+1}});
    for(unsigned i=0;i+2<512;i+=3)m.faces.push_back({i/3,{i,i+1,i+2}});
    same(s,m);for(auto& v:m.vertices)for(auto& p:v.path.control)p.x=-p.x;same(s,m);
    need(s.fallback_sorts()>0,"reversal failed to exercise bounded repair fallback");
    CachedAxisSweep empty;f::Mesh z;same(empty,z);z.thickness=-1;bool rejected=false;try{(void)empty.query(z);}catch(const std::invalid_argument&){rejected=true;}need(rejected,"empty invalid thickness accepted");
    std::cout<<"PASS "<<checked<<" changing meshes, 500 malformed/recovery controls, reversal fallback="<<s.fallback_sorts()<<"\n";
}

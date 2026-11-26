#pragma once
// Shared original connected U-solid source fixture for native and upstream
// CCD validation. Constructed volumetric geometry, not a benchmark dataset.
#include "kyoto/coupled_fem_contact.hpp"
#include <map>
#include <set>
#include <stdexcept>
namespace kyoto::solid::fixtures {
inline void fixture_require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
inline Body u_solid(unsigned width_cells=1,double gap=.0025,double speed=.1){
    if(width_cells<1||width_cells>64||!std::isfinite(gap)||gap<=0.||!std::isfinite(speed)||speed<0.)
        throw std::invalid_argument("invalid connected U scale fixture");
    Body b;b.material.gravity=0.;b.material.damping=0.;b.material.young=12000.;
    const double xs[]={0.,.1,.1+gap,.2+gap},zs[]={0.,.1,.3};
    std::map<std::array<int,3>,std::size_t> ids;
    auto node=[&](int i,int j,int k){
        const std::array<int,3> key{i,j,k};
        const auto found=ids.find(key);
        if(found!=ids.end())return found->second;
        const auto id=b.nodes.size();ids.emplace(key,id);
        const V3 p{xs[i],.1*double(j),zs[k]};
        const V3 v{k==2?(i<=1?speed:-speed):0.,0.,0.};
        b.nodes.push_back({p,p,v,0.,0.});return id;
    };
    for(int k=0;k<2;++k)for(int j=0;j<int(width_cells);++j)for(int i=0;i<3;++i){
        if(k==1&&i==1)continue;
        const auto a=node(i,j,k),c=node(i,j+1,k),d=node(i+1,j+1,k),e=node(i,j,k+1),
                   f=node(i+1,j,k+1),g=node(i,j+1,k+1),h=node(i+1,j+1,k+1),
                   q=node(i+1,j,k);
        b.add_tet({a,q,d,h});b.add_tet({a,d,c,h});b.add_tet({a,c,g,h});
        b.add_tet({a,g,e,h});b.add_tet({a,e,f,h});b.add_tet({a,f,q,h});
    }
    std::map<SelfFace,int> incidence;
    for(const auto& t:b.tets)for(int omit=0;omit<4;++omit){
        SelfFace face{};int at=0;
        for(int j=0;j<4;++j)if(j!=omit)face[at++]=t.n[j];
        std::sort(face.begin(),face.end());++incidence[face];
    }
    for(const auto& [f,n]:incidence){
        fixture_require(n==1||n==2,"nonmanifold cell face");
        if(n==1)b.surface_faces.push_back(f);
    }
    std::set<std::size_t> seen{0};
    bool change=true;
    while(change){
        const auto old=seen.size();
        for(const auto& t:b.tets){
            bool connected=false;for(auto n:t.n)connected|=seen.contains(n);
            if(connected)for(auto n:t.n)seen.insert(n);
        }
        change=seen.size()!=old;
    }
    fixture_require(seen.size()==b.nodes.size()&&b.tets.size()==30*width_cells,"fixture not connected");
    return b;
}
inline Body remote(){
    auto b=toroidal_solid(8,2,2);
    b.material.gravity=b.material.damping=0.;
    for(auto& n:b.nodes)n.x.z+=10.;
    return b;
}
} // namespace kyoto::solid::fixtures

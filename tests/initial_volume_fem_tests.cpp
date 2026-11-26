#include "kyoto/coupled_fem_contact.hpp"
#include <iostream>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Body tet(V3 offset,double side){
    Body b;b.material.gravity=b.material.damping=b.material.young=0.;
    for(V3 p:{V3{0,0,0},V3{1,0,0},V3{0,1,0},V3{0,0,1}}){
        p=offset+p*side;b.nodes.push_back({p,p,{},0.,0.});
    }
    b.add_tet({0,1,2,3});b.surface_faces={{0,1,2},{0,1,3},{0,2,3},{1,2,3}};
    return b;
}
static void run(bool sweep){
    Body a=tet({},1.),b=tet({.1,.1,.1},.1);
    CoupledFEMSettings cfg;cfg.initial_cross_sweep=sweep;CoupledFEMStats s;
    // The surfaces are distinct. The old surface-only check clears them.
    need(coupled_detail::swept_safe(a,a,b,b,cfg,s),"nested surfaces should be geometrically separated");
    need(!coupled_detail::initial_cross_volume_safe(a,b,s,sweep)&&s.volume_certificates>0,
         "nested volumes escaped initial guard");
    const auto before_a=a,before_b=b;
    std::vector<CoupledFEMLeaf> trace(1);trace.front().dt=-1.;
    bool refused=false;
    try{(void)step_coupled_fem(a,b,.00025,cfg,&trace);}
    catch(const std::runtime_error& e){
        refused=std::string(e.what()).find("initial coupled volumes")!=std::string::npos;
    }
    need(refused&&trace.size()==1&&trace.front().dt==-1.,
         "nested volume transaction or trace guard failed");
    need(a.time_seconds==before_a.time_seconds&&b.time_seconds==before_b.time_seconds,
         "nested bodies advanced time");
    for(std::size_t i=0;i<a.nodes.size();++i)
        need(norm(a.nodes[i].x-before_a.nodes[i].x)==0.&&norm(b.nodes[i].x-before_b.nodes[i].x)==0.,
             "nested body geometry changed");
    // These boxes overlap but the actual tetrahedra do not.
    b=tet({.5,.5,.5},1.);
    s={};
    need(coupled_detail::initial_cross_volume_safe(a,b,s,sweep)&&s.volume_certificates==1&&s.volume_projections>0,
         "valid oblique separation failed");
    const auto valid=step_coupled_fem(a,b,.00025,cfg);
    need(valid.accepted_leaves>0&&valid.volume_certificates==1&&a.time_seconds==.00025,
         "valid oblique physical step failed");
    b=tet({10.,0,0},1.);
    s={};
    need(coupled_detail::initial_cross_volume_safe(a,b,s,sweep)&&s.volume_box_tests==1&&s.volume_certificates==0,
         "disjoint body fast path failed");
    // A missing boundary triangle must be rejected before geometry
    // certification, even if the remaining surface appears separated.
    a=tet({},1.);b=tet({10.,0,0},1.);
    a.surface_faces.pop_back();
    bool missing_rejected=false;
    try{(void)step_coupled_fem(a,b,.00025,cfg);}
    catch(const std::invalid_argument& e){
        missing_rejected=std::string(e.what()).find("supplied boundary differs")!=std::string::npos;
    }
    need(missing_rejected&&a.time_seconds==0.&&b.time_seconds==0.,
         "incomplete collision boundary advanced physical state");
    std::cout<<"PASS38A initial cross-volume FEM integration and rollback sweep="<<sweep<<'\n';
}

int main(){run(false);run(true);}

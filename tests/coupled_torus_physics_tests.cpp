// Pass 31B: two connected volumetric toroidal bodies, not isolated tets.
// Optional outputs are derived from accepted native physical states.
// Both arms use the same rounded surface and orientation certificates.
#include "kyoto/coupled_fem_contact.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace kyoto::solid;
static void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static V3 momentum(const Body& b){
    V3 p{};for(const auto& n:b.nodes)p+=n.v*n.mass;return p;
}
static void same(const Body& a,const Body& b){
    need(a.nodes.size()==b.nodes.size()&&a.time_seconds==b.time_seconds,
         "paired torus topology or clock mismatch");
    for(std::size_t i=0;i<a.nodes.size();++i){
        need(norm(a.nodes[i].x-b.nodes[i].x)==0.&&norm(a.nodes[i].v-b.nodes[i].v)==0.&&
             a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,
             "paired torus physical state diverged");
    }
}
static double gap(const Body& a,const Body& b){
    double g=std::numeric_limits<double>::infinity();
    for(const auto& f:a.surface_faces)for(const auto& h:b.surface_faces)
        g=std::min(g,triangle_triangle_closest(
            coupled_detail::positions(a,f),coupled_detail::positions(b,h)).distance);
    return g;
}
static void vtk(const std::filesystem::path& path,const Body& a,const Body& b){
    std::ofstream o(path);
    if(!o)throw std::runtime_error("cannot export native coupled torus state");
    o<<std::setprecision(17)<<"# vtk DataFile Version 3.0\n"
      <<"Kyoto actual accepted coupled FEM state\nASCII\nDATASET UNSTRUCTURED_GRID\n";
    const auto nv=a.nodes.size()+b.nodes.size(),nt=a.tets.size()+b.tets.size();
    o<<"POINTS "<<nv<<" double\n";
    for(const auto* body:{&a,&b})for(const auto& n:body->nodes)
        o<<n.x.x<<' '<<n.x.y<<' '<<n.x.z<<'\n';
    o<<"CELLS "<<nt<<' '<<5*nt<<'\n';
    for(const auto& t:a.tets)o<<"4 "<<t.n[0]<<' '<<t.n[1]<<' '<<t.n[2]<<' '<<t.n[3]<<'\n';
    const auto off=a.nodes.size();
    for(const auto& t:b.tets)o<<"4 "<<off+t.n[0]<<' '<<off+t.n[1]<<' '<<off+t.n[2]<<' '<<off+t.n[3]<<'\n';
    o<<"CELL_TYPES "<<nt<<'\n';for(std::size_t i=0;i<nt;++i)o<<"10\n";
    o<<"POINT_DATA "<<nv<<"\nSCALARS body_id int 1\nLOOKUP_TABLE default\n";
    for(std::size_t i=0;i<a.nodes.size();++i)o<<"0\n";
    for(std::size_t i=0;i<b.nodes.size();++i)o<<"1\n";
    o<<"VECTORS velocity double\n";
    for(const auto* body:{&a,&b})for(const auto& n:body->nodes)
        o<<n.v.x<<' '<<n.v.y<<' '<<n.v.z<<'\n';
    o<<"SCALARS von_mises_pa double 1\nLOOKUP_TABLE default\n";
    for(const auto* body:{&a,&b})for(double stress:body->evaluate().nodal_von_mises_pa)o<<stress<<'\n';
    o<<"SCALARS normal_impulse_ns double 1\nLOOKUP_TABLE default\n";
    for(const auto* body:{&a,&b})for(const auto& n:body->nodes)o<<n.last_normal_impulse<<'\n';
    if(!o)throw std::runtime_error("failed writing native coupled torus VTK");
}
int main(int argc,char** argv){
    try{
        if(argc>2)throw std::invalid_argument("usage: coupled_torus_test [output_dir]");
        Material ma,mb;ma.gravity=mb.gravity=0.;ma.damping=mb.damping=0.;
        ma.density=360.;mb.density=480.;
        Body a=toroidal_solid(8,2,2,ma),b=toroidal_solid(8,2,2,mb);
        const double initial_gap=.006;
        for(auto& n:a.nodes)n.v={0,0,.5};
        for(auto& n:b.nodes){n.x.z+=.36+initial_gap;n.v={0,0,-.5};}
        need(a.tets.size()>1&&b.tets.size()>1&&gap(a,b)>.0059,"invalid initial disjoint toroidal geometry");
        Body aa=a,bb=b;
        CoupledFEMSettings flat,tree;tree.hierarchy=true;
        const auto pa0=momentum(a),pb0=momentum(b);
        V3 contact{};std::size_t impulses=0,leaves=0;
        double min_gap=gap(a,b),min_j=1.;
        std::filesystem::path out;
        std::ofstream csv;
        if(argc==2){
            out=argv[1];std::filesystem::create_directories(out);
            csv.open(out/"coupled_torus_history.csv");csv<<std::setprecision(17);
            csv<<"step,time_s,accepted_leaves,rejected_geometry,rejected_tets,normal_impulse_ns,contact_work_j,minimum_cross_surface_gap_m,certified_min_j,flat_box_tests,tree_box_tests,rounded_projections\n";
            vtk(out/"state_0.vtk",a,b);
        }
        constexpr double dt=.0005;constexpr int steps=12;
        for(int i=0;i<steps;++i){
            const auto x=step_coupled_fem(a,b,dt,flat);
            const auto y=step_coupled_fem(aa,bb,dt,tree);
            same(a,aa);same(b,bb);
            need(x.accepted_leaves==y.accepted_leaves&&x.rejected_geometry==y.rejected_geometry&&
                 x.rejected_tets==y.rejected_tets&&x.normal_impulse==y.normal_impulse&&
                 x.contact_kinetic_change==y.contact_kinetic_change&&
                 x.certified_min_j==y.certified_min_j&&x.rounded_projections==y.rounded_projections,
                 "paired torus receipts diverged");
            const auto g=gap(a,b);
            need(g>flat.skin&&x.certified_min_j>1e-6,"torus leaf lacked separation or orientation");
            need(std::abs(a.time_seconds-(i+1)*dt)<1e-12&&a.time_seconds==b.time_seconds,
                 "paired torus clock mismatch");
            min_gap=std::min(min_gap,g);min_j=std::min(min_j,x.certified_min_j);
            impulses+=x.impulse_updates;leaves+=x.accepted_leaves;contact+=x.impulse_on_a;
            if(argc==2){
                csv<<i+1<<','<<a.time_seconds<<','<<x.accepted_leaves<<','<<x.rejected_geometry
                   <<','<<x.rejected_tets<<','<<x.normal_impulse<<','<<x.contact_kinetic_change
                   <<','<<g<<','<<x.certified_min_j<<','<<x.box_tests<<','<<y.box_tests
                   <<','<<x.rounded_projections<<'\n';
                vtk(out/("state_"+std::to_string(i+1)+".vtk"),a,b);
            }
        }
        need(impulses>0&&norm(contact)>0.,"connected toroidal bodies never exchanged contact impulses");
        const auto ra=norm(momentum(a)-pa0-contact),rb=norm(momentum(b)-pb0+contact);
        need(ra<1e-8&&rb<1e-8,"toroidal finite-mass impulse balance mismatch");
        if(argc==2)need(csv.good(),"coupled torus history export failed");
        std::cout<<std::setprecision(17)<<"COUPLED_TORUS steps="<<steps
                 <<" total_tets="<<a.tets.size()+b.tets.size()
                 <<" total_nodes="<<a.nodes.size()+b.nodes.size()
                 <<" impulses="<<impulses<<" leaves="<<leaves
                 <<" min_gap="<<min_gap<<" certified_min_j="<<min_j
                 <<" momentum_residual_a="<<ra<<" momentum_residual_b="<<rb<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

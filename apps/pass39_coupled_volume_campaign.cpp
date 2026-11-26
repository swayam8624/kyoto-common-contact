// Exact independent whole-complex audit of actual accepted native leaves,
// including near-contact width scaling and cross-body torus interaction.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/exact_swept_volume_audit.hpp"
#include <iomanip>
#include <iostream>
using namespace kyoto::solid;
namespace g=kyoto::geometry;
static void append(const Body& a,const Body& b,std::vector<g::Point>& x,std::vector<g::Point>& y,std::vector<g::ExactTetIds>& cells){
    if(a.nodes.size()!=b.nodes.size()||a.tets.size()!=b.tets.size())throw std::runtime_error("changed topology");
    const auto off=x.size();
    for(std::size_t i=0;i<a.nodes.size();++i){const auto p=a.nodes[i].x,q=b.nodes[i].x;x.push_back({p.x,p.y,p.z});y.push_back({q.x,q.y,q.z});}
    for(std::size_t i=0;i<a.tets.size();++i){auto t=a.tets[i].n;if(t!=b.tets[i].n)throw std::runtime_error("changed cell order");for(auto& j:t)j+=off;cells.push_back(t);}
}
static bool audit(const char* scene,unsigned width,int step,int leaf,const Body& a,const Body& b,const Body& aa,const Body& bb,double dt){
    std::vector<g::Point> x,y;std::vector<g::ExactTetIds> cells;append(a,aa,x,y,cells);append(b,bb,x,y,cells);
    const auto r=g::audit_exact_swept_volume(x,y,cells,6,false,true,true);
    std::cout<<scene<<','<<width<<','<<step<<','<<leaf<<','<<dt<<','<<r.cells<<','<<r.pair_tests<<','<<r.overlap_tests<<','<<r.orientation_unknown;
    for(auto n:r.tested)std::cout<<','<<n;
    for(auto n:r.unknown)std::cout<<','<<n;
    std::cout<<','<<r.certified<<'\n'<<std::flush;
    if(!r.certified)std::cerr<<"unresolved "<<scene<<" width="<<width<<" step="<<step<<" pair="<<r.first_unknown[0]<<','<<r.first_unknown[1]<<'\n';
    return r.certified;
}
static Body tetra(double plane,double direction,double density){
    Body b;b.material.gravity=b.material.damping=0.;b.material.young=12000.;b.material.density=density;
    for(V3 p:{V3{plane,0,0},V3{plane,.2,0},V3{plane,0,.2},V3{plane+.2*direction,0,0}})b.nodes.push_back({p,p,{-direction,0,0},0,0});
    b.add_tet({0,1,2,3});b.surface_faces={{0,1,2},{0,3,1},{1,3,2},{2,3,0}};return b;
}
int main(int argc,char** argv){
    const unsigned maximum=argc==2?unsigned(std::stoul(argv[1])):64;
    std::cout<<std::setprecision(17)<<"scene,width,step,leaf,dt,cells,pairs,overlap_tests,orientation_unknown,test0,test1,test2,test3,unknown0,unknown1,unknown2,unknown3,certified\n";
    std::size_t unknown=0,leaves=0;
    for(unsigned w:{1u,4u,16u,32u,64u})if(w<=maximum)for(int near:{0,1}){
        auto a=fixtures::u_solid(w,near?.0025:.02,.1),b=fixtures::remote();CoupledFEMContext context;CoupledFEMSettings c;c.hierarchy=c.typed_self_contact=true;
        std::size_t impacts=0;
        for(int step=0;step<4;++step){std::vector<CoupledFEMLeaf> trace;const auto r=step_coupled_fem(a,b,.00025,c,&trace,&context);impacts+=r.self_a.impulse_updates;if(trace.size()!=r.accepted_leaves||trace.empty())throw std::runtime_error("missing leaves");
            for(std::size_t i=0;i<trace.size();++i){const auto& l=trace[i];unknown+=!audit(near?"U_near":"U_separated",w,step,int(i),l.before_a,l.before_b,l.after_a,l.after_b,l.dt);++leaves;}}
        if((near&&impacts==0)||(!near&&impacts!=0))throw std::runtime_error("nonrepresentative contact regime");
        std::cerr<<"U width="<<w<<" near="<<near<<" impulses="<<impacts<<'\n';
    }
    Material ma,mb;ma.gravity=mb.gravity=ma.damping=mb.damping=0.;ma.density=360;mb.density=480;
    auto a=toroidal_solid(8,2,2,ma),b=toroidal_solid(8,2,2,mb);for(auto& n:a.nodes)n.v={0,0,.5};for(auto& n:b.nodes){n.x.z+=.366;n.v={0,0,-.5};}
    CoupledFEMSettings c;c.hierarchy=true;std::size_t impulses=0;
    for(int step=0;step<12;++step){std::vector<CoupledFEMLeaf> trace;const auto r=step_coupled_fem(a,b,.0005,c,&trace);impulses+=r.impulse_updates;
        for(std::size_t i=0;i<trace.size();++i){const auto& l=trace[i];unknown+=!audit("cross_torus",8,step,int(i),l.before_a,l.before_b,l.after_a,l.after_b,l.dt);++leaves;}}
    if(!impulses)throw std::runtime_error("no torus cross impulses");
    std::cerr<<"torus impulses="<<impulses<<'\n';
    a=tetra(0.,-1.,360.);b=tetra(.006,1.,480.);auto aa=a,bb=b;c.activation_width=0.;c.max_bisection=0;
    CoupledFEMStats scratch;TypedSelfAuditor ta,tb;coupled_detail::propose_drift(aa,bb,.004,c,scratch,ta,tb);
    if(audit("rejected_cross",1,0,0,a,b,aa,bb,.004))throw std::runtime_error("crossing certified as safe");
    // Independent exact endpoint oracle must find forbidden overlap, not merely an unknown.
    auto as=[&](const Body& body){g::Tetrahedron t;for(int i=0;i<4;++i){const auto p=body.nodes[body.tets[0].n[i]].x;t[i]={p.x,p.y,p.z};}return t;};
    if(g::exact_static_tet_intersection(as(aa),{0,1,2,3},as(bb),{4,5,6,7}).matches_shared_simplex)throw std::runtime_error("cross positive not overlapping at endpoint");
    bool rejected=false;try{(void)step_coupled_fem(a,b,.004,c);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected||a.time_seconds!=0||b.time_seconds!=0)throw std::runtime_error("rejected trial committed");
    std::cerr<<"accepted leaves="<<leaves<<" unresolved="<<unknown<<" positive rejected and exact static overlap confirmed\n";return unknown?1:0;
}

// Held-out reference-gated physical campaign and explicit false-acceptance
// controls. Results describe the cost of a stronger safety contract, not
// an acceleration of the ordinary native solver.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/exact_coupled_transaction.hpp"
#include "kyoto/tiled_solver_bridge_campaign.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
namespace s=kyoto::solid;
namespace e=s::exact_transaction;
namespace c=s::device_bridge;
using Clock=std::chrono::steady_clock;
using s::V3;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void tet(s::Body& b,double plane,double direction,double speed,double side=.2){
    const auto offset=b.nodes.size();
    for(V3 p:{V3{plane,0,0},V3{plane,side,0},V3{plane,0,side},V3{plane+side*direction,0,0}})
        b.nodes.push_back({p,p,{-direction*speed,0,0},0,0});
    b.add_tet({offset,offset+1,offset+2,offset+3});
    for(const auto& face:std::array<s::SelfFace,4>{{{0,1,2},{0,3,1},{1,3,2},{2,3,0}}}){
        auto f=face;for(auto& i:f)i+=offset;b.surface_faces.push_back(f);
    }
}
static void controls(){
    // Deliberately disable native self-contact. Two disjoint tetrahedra
    // belonging to one Body then cross in a single native accepted drift.
    // Exact gate must detect the missing contract, not just a native error.
    s::Body a;a.material.gravity=a.material.damping=0.;a.material.young=12000.;
    tet(a,0.,-1.,1.);tet(a,.006,1.,1.);
    auto b=s::fixtures::remote();s::CoupledFEMSettings cfg;cfg.hierarchy=true;
    auto raw_a=a,raw_b=b;
    const auto raw=s::step_coupled_fem(raw_a,raw_b,.004,cfg);
    require(raw.accepted_leaves>0&&raw_a.time_seconds==.004,"native control did not accept");
    require(!e::audit(a,b,raw_a,raw_b,6).certified,"crossing control is not unsafe");
    std::vector<s::CoupledFEMLeaf> trace(1);trace[0].dt=-1.;s::CoupledFEMContext ctx;
    ctx.bind(nullptr);bool reject=false;
    try{(void)e::step(a,b,.004,cfg,&trace,&ctx);}catch(const std::runtime_error& ex){reject=std::string_view(ex.what())=="exact accepted leaf volume uncertified";}
    require(reject&&trace.size()==1&&trace[0].dt==-1.&&ctx.mapping_rebuilds()==0,"exact post-drift rollback failed");
    require(a.time_seconds==0.&&b.time_seconds==0.,"exact failure advanced clock");
    for(const auto& n:a.nodes)require(c::equal(n.x,n.rest),"exact failure changed geometry");
    // Separate contained tetrahedron with an initially overlapping solid.
    s::Body nested;nested.material=a.material;
    tet(nested,0.,1.,0.,1.);tet(nested,.1,1.,0.,.05);
    require(!e::audit(nested,b,nested,b,6).certified,"invalid initial volume control not exercised");
    const auto before=nested;reject=false;
    try{(void)e::step(nested,b,.00025,cfg,&trace,&ctx);}catch(const std::runtime_error& ex){reject=std::string_view(ex.what())=="exact initial volume uncertified";}
    require(reject&&trace[0].dt==-1.,"exact initial overlap rollback failed");c::same(nested,before);
    // A valid short drift after rejection must remain usable.
    const auto accepted=e::step(a,b,.00025,cfg,&trace,&ctx);
    require(accepted.audited_leaves>0&&a.time_seconds==.00025,"exact recovery failed");
    std::cerr<<"PASS exact-only post-drift rejection, initial overlap and recovery\n";
}
int main(int argc,char** argv){try{
    const auto maxw=argc==2?unsigned(std::stoul(argv[1])):64u;
    require(argc<=2&&(maxw==1||maxw==4||maxw==16||maxw==32||maxw==64),"invalid width");
    std::cout<<std::setprecision(17)<<"scene,width,near,rotated,step,cells,leaves,exact_pairs,possible_pairs,overlap_tests,initial_pairs,native_ns,guarded_ns,self_impulses,cross_impulses,certified_min_j\n";
    s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=cfg.cached_self_sap=cfg.spatial_first_self_sap=cfg.fused_self_sap=true;
    cfg.reuse_cross_geometry=true;
    auto scene=[&](const char* name,unsigned w,bool near,bool rotated,s::Body a,s::Body b,int steps,double dt){
        // Rotate the world coordinates and velocities, leaving the stored
        // material reference and inverse-rest matrices untouched.
        if(rotated)for(auto* body:{&a,&b})for(auto& n:body->nodes){n.x=c::rotate(n.x);n.v=c::rotate(n.v);}
        auto aa=a,bb=b;s::CoupledFEMContext cr,ce;std::size_t impacts=0;
        for(int step=0;step<steps;++step){
            // Alternate execution order across steps. These are diagnostic
            // costs (one trajectory each), not a randomized speedup claim.
            s::CoupledFEMStats r;e::Receipt q;std::vector<s::CoupledFEMLeaf> tr,te;
            long long native_ns=0,guarded_ns=0;
            auto native=[&](){auto t=Clock::now();r=s::step_coupled_fem(a,b,dt,cfg,&tr,&cr);native_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();};
            auto exact=[&](){auto t=Clock::now();q=e::step(aa,bb,dt,cfg,&te,&ce);guarded_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();};
            if(step%2){exact();native();}else{native();exact();}
            c::same(a,aa);c::same(b,bb);c::same_receipt(r,q.physics);
            require(tr.size()==te.size()&&tr.size()==q.audited_leaves&&q.initial.certified,"missing exact leaf");
            for(std::size_t i=0;i<tr.size();++i){require(tr[i].dt==te[i].dt&&tr[i].depth==te[i].depth,"leaf split changed");c::same(tr[i].before_a,te[i].before_a);c::same(tr[i].before_b,te[i].before_b);c::same(tr[i].after_a,te[i].after_a);c::same(tr[i].after_b,te[i].after_b);}
            const auto self=r.self_a.impulse_updates+r.self_b.impulse_updates;impacts+=self+r.impulse_updates;
            std::cout<<name<<','<<w<<','<<near<<','<<rotated<<','<<step<<','<<q.initial.cells<<','<<q.audited_leaves<<','<<q.exact_pairs<<','<<q.possible_pairs<<','<<q.overlap_tests<<','<<std::accumulate(q.initial.tested.begin(),q.initial.tested.end(),std::size_t{})<<','<<native_ns<<','<<guarded_ns<<','<<self<<','<<r.impulse_updates<<','<<r.certified_min_j<<'\n';
        }
        require(near?impacts>0:impacts==0,"vacuous exact campaign regime");std::cerr<<"PASS "<<name<<" width="<<w<<" near="<<near<<" rotated="<<rotated<<" impulses="<<impacts<<'\n';
    };
    for(unsigned w:{1u,4u,16u,32u,64u})if(w<=maxw)for(bool near:{false,true})
        scene("connected_U",w,near,false,s::fixtures::u_solid(w,near?.0025:.02,.1),s::fixtures::remote(),4,.00025);
    if(maxw>=4)for(bool near:{false,true})scene("rotated_U",4,near,true,s::fixtures::u_solid(4,near?.0025:.02,.1),s::fixtures::remote(),8,.00025);
    s::Material ma,mb;ma.gravity=mb.gravity=ma.damping=mb.damping=0.;ma.density=360.;mb.density=480.;
    auto a=s::toroidal_solid(8,2,2,ma),b=s::toroidal_solid(8,2,2,mb);
    for(auto& n:a.nodes)n.v={0,0,.5};
    for(auto& n:b.nodes){n.x.z+=.366;n.v={0,0,-.5};}
    scene("cross_torus",8,true,false,a,b,12,.0005);
    controls();return 0;
}catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}}

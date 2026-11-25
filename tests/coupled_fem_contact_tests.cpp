// Pass 29B: actual volumetric two-body FEM, not prescribed trajectories.
#include "kyoto/coupled_fem_contact.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace kyoto::solid;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Body tetra(double plane,double direction,double speed,double density){
    Body b;b.material.gravity=0.;b.material.damping=0.;b.material.density=density;
    b.material.young=12000.;
    for(V3 p:{V3{plane,0,0},V3{plane,.2,0},V3{plane,0,.2},
              V3{plane+.2*direction,0,0}})
        b.nodes.push_back({p,p,{-direction*speed,0,0},0,0});
    b.add_tet({0,1,2,3});
    b.surface_faces={{0,1,2},{0,3,1},{1,3,2},{2,3,0}};
    return b;
}
struct Ledger{V3 p{},l{};double kinetic{},elastic{};};
static Ledger measure(const Body& a,const Body& b){
    Ledger r;
    for(const auto* body:{&a,&b}){
        r.elastic+=body->evaluate().elastic_joule;
        for(const auto& n:body->nodes){
            const auto p=n.v*n.mass;r.p+=p;r.l+=cross(n.x,p);
            r.kinetic+=.5*n.mass*dot(n.v,n.v);
        }
    }
    return r;
}
static V3 momentum(const Body& b){V3 p{};for(const auto& n:b.nodes)p+=n.v*n.mass;return p;}
static void same(const Body& a,const Body& b){
    require(a.time_seconds==b.time_seconds&&a.contact_count_step==b.contact_count_step&&
            a.normal_impulse_step==b.normal_impulse_step,"body metadata diverged");
    for(std::size_t i=0;i<a.nodes.size();++i){
        require(norm(a.nodes[i].x-b.nodes[i].x)==0.&&norm(a.nodes[i].v-b.nodes[i].v)==0.&&
                a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,
                "body state diverged");
    }
}
static double minimum_gap(const Body& a,const Body& b){
    double gap=std::numeric_limits<double>::infinity();
    for(const auto& f:a.surface_faces)for(const auto& g:b.surface_faces)
        gap=std::min(gap,triangle_triangle_closest(
            coupled_detail::positions(a,f),coupled_detail::positions(b,g)).distance);
    return gap;
}
static void run(bool fast){
    const double gap=fast?.006:.01;
    Body a=tetra(0.,-1.,1.,360.),b=tetra(gap,1.,1.,480.);
    Body aa=a,bb=b;
    CoupledFEMSettings flat,tree;tree.hierarchy=true;
    const auto initial=measure(a,b);
    const auto p0a=momentum(a),p0b=momentum(b);
    V3 impulse{};
    double min_gap=gap,min_j=1,certified_j=1,contact_work=0;
    std::size_t updates=0,rejections=0,leaves=0;
    const int steps=fast?1:30;
    const double dt=fast?.004:.0005;
    for(int i=0;i<steps;++i){
        std::vector<CoupledFEMLeaf> trace;
        const auto x=step_coupled_fem(a,b,dt,flat,&trace);
        require(trace.size()==x.accepted_leaves,"missing actual physical leaves");
        double elapsed=0.;
        for(std::size_t k=0;k<trace.size();++k){
            const auto& leaf=trace[k];
            require(leaf.dt>0.&&leaf.before_a.time_seconds==leaf.before_b.time_seconds&&
                    leaf.after_a.time_seconds==leaf.after_b.time_seconds&&
                    std::abs(leaf.after_a.time_seconds-leaf.before_a.time_seconds-leaf.dt)<1e-12,
                    "leaf physical time is inconsistent");
            if(k>0){
                const auto& prev=trace[k-1];
                require(prev.after_a.time_seconds==leaf.before_a.time_seconds,
                        "leaf time gap");
                for(std::size_t j=0;j<leaf.before_a.nodes.size();++j)
                    require(norm(prev.after_a.nodes[j].x-leaf.before_a.nodes[j].x)==0.,
                            "first body leaf geometry gap");
                for(std::size_t j=0;j<leaf.before_b.nodes.size();++j)
                    require(norm(prev.after_b.nodes[j].x-leaf.before_b.nodes[j].x)==0.,
                            "second body leaf geometry gap");
            }
            // A separate exhaustive dense sample audit: regression, not
            // a proof of absence of all continuous collisions.
            for(int t=0;t<=32;++t){
                const double u=double(t)/32.;
                for(const auto& f:leaf.before_a.surface_faces)
                    for(const auto& g:leaf.before_b.surface_faces){
                        const auto x0=coupled_detail::positions(leaf.before_a,f);
                        const auto x1=coupled_detail::positions(leaf.after_a,f);
                        const auto y0=coupled_detail::positions(leaf.before_b,g);
                        const auto y1=coupled_detail::positions(leaf.after_b,g);
                        require(triangle_triangle_closest(lerp_vertices(x0,x1,u),
                                  lerp_vertices(y0,y1,u)).distance>flat.skin,
                                "sampled accepted physical trajectory crossed skin");
                    }
            }
            elapsed+=leaf.dt;
        }
        require(std::abs(elapsed-dt)<1e-12,"accepted leaf durations do not sum");
        const auto y=step_coupled_fem(aa,bb,dt,tree);
        same(a,aa);same(b,bb);
        require(x.impulse_updates==y.impulse_updates&&x.accepted_leaves==y.accepted_leaves&&
                x.rejected_geometry==y.rejected_geometry&&x.rejected_tets==y.rejected_tets&&
                x.certificate_tests==y.certificate_tests&&x.interval_nodes==y.interval_nodes&&
                x.orientation_nodes==y.orientation_nodes&&x.certified_min_j==y.certified_min_j&&
                x.normal_impulse==y.normal_impulse&&x.contact_kinetic_change==y.contact_kinetic_change,
                "hierarchical contact decisions diverged");
        require(std::abs(a.time_seconds-(i+1)*dt)<1e-12&&a.time_seconds==b.time_seconds,
                "two physical clocks diverged");
        const double g=minimum_gap(a,b);
        require(g>flat.skin&&x.min_j>1e-6&&x.certified_min_j>1e-6&&x.orientation_nodes>0,
                "accepted coupled state lacks continuous orientation proof");
        min_gap=std::min(g,min_gap);min_j=std::min(min_j,x.min_j);
        certified_j=std::min(certified_j,x.certified_min_j);
        double ia=0,ib=0;
        for(const auto& n:a.nodes)ia+=n.last_normal_impulse;
        for(const auto& n:b.nodes)ib+=n.last_normal_impulse;
        require(std::abs(ia-x.normal_impulse)<1e-9&&std::abs(ib-x.normal_impulse)<1e-9,
                "accepted nodal impulse receipts incomplete");
        updates+=x.impulse_updates;rejections+=x.rejected_geometry;leaves+=x.accepted_leaves;
        impulse+=x.impulse_on_a;contact_work+=x.contact_kinetic_change;
    }
    const auto final=measure(a,b);
    require(updates>0&&norm(impulse)>0,"no true finite-mass contact response");
    require(norm(final.p-initial.p)<1e-9&&norm(final.l-initial.l)<1e-9,
            "isolated two-body total momentum balance failed");
    require(norm(momentum(a)-p0a-impulse)<1e-9&&norm(momentum(b)-p0b+impulse)<1e-9,
            "equal/opposite body impulse accounting failed");
    require(contact_work<=0.&&std::isfinite(final.kinetic+final.elastic),
            "invalid contact work or mechanical energy");
    if(fast)require(rejections>0&&leaves>1,"fast coupled fixture did not exercise subdivision");
    std::cout<<std::setprecision(17)<<"COUPLED_FEM fast="<<fast<<" steps="<<steps
             <<" updates="<<updates<<" geom_rejections="<<rejections<<" leaves="<<leaves
             <<" min_gap_m="<<min_gap<<" min_j="<<min_j<<" certified_continuous_min_j="<<certified_j
             <<" momentum_residual="<<norm(final.p-initial.p)
             <<" angular_residual="<<norm(final.l-initial.l)
             <<" contact_work_j="<<contact_work
             <<" mechanical_change_j="<<final.kinetic+final.elastic-initial.kinetic-initial.elastic
             <<'\n';
}
int main(){
    try{
        run(false);run(true);
        Body a=tetra(0.,-1.,1.,360.),b=tetra(.006,1.,1.,480.);
        const Body aa=a,bb=b;
        CoupledFEMSettings c;c.max_bisection=0;
        bool rejected=false;
        std::vector<CoupledFEMLeaf> trace(1);trace[0].dt=-1.;
        try{(void)step_coupled_fem(a,b,.004,c,&trace);}
        catch(const std::runtime_error&){rejected=true;}
        require(rejected,"unsafe full trial accepted under zero subdivision budget");
        same(a,aa);same(b,bb);
        require(trace.size()==1&&trace[0].dt==-1.,"failed step mutated caller trace");
        std::cout<<"PASS coupled two-volume FEM physical time, momentum and rollback\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

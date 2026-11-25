// Pass 32A: same Body storage with two disconnected tetrahedral
// components. This tests the restricted nonincident response, not
// yet a connected folded-solid self-contact benchmark.
#include "kyoto/coupled_fem_contact.hpp"
#include <iostream>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static void append(Body& b,double plane,double dir,double speed){
    const auto base=b.nodes.size();
    for(V3 p:{V3{plane,0,0},V3{plane,.2,0},V3{plane,0,.2},V3{plane+.2*dir,0,0}})
        b.nodes.push_back({p,p,{-dir*speed,0,0},0,0});
    b.add_tet({base,base+1,base+2,base+3});
    for(const auto& f:std::array<SelfFace,4>{{{0,1,2},{0,3,1},{1,3,2},{2,3,0}}})
        b.surface_faces.push_back({base+f[0],base+f[1],base+f[2]});
}
static Body fixture(){
    Body a;a.material.gravity=0.;a.material.damping=0.;
    append(a,0.,-1.,1.);append(a,.006,1.,1.);return a;
}
static Body distant(){
    Body b;b.material.gravity=0.;b.material.damping=0.;
    append(b,10.,1.,0.);return b;
}
static V3 p(const Body& b){V3 r{};for(const auto& n:b.nodes)r+=n.v*n.mass;return r;}
static V3 l(const Body& b){V3 r{};for(const auto& n:b.nodes)r+=cross(n.x,n.v*n.mass);return r;}
static double k(const Body& b){double r=0.;for(const auto& n:b.nodes)r+=.5*n.mass*dot(n.v,n.v);return r;}
static void same(const Body& a,const Body& b){
    need(a.time_seconds==b.time_seconds&&a.nodes.size()==b.nodes.size(),"clock/topology changed");
    for(std::size_t i=0;i<a.nodes.size();++i)
        need(norm(a.nodes[i].x-b.nodes[i].x)==0.&&norm(a.nodes[i].v-b.nodes[i].v)==0.&&
             a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,"state changed");
}
static double min_gap(const Body& a){
    double gap=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<a.surface_faces.size();++i)
        for(std::size_t j=i+1;j<a.surface_faces.size();++j){
            const auto& f=a.surface_faces[i];const auto& g=a.surface_faces[j];
            if(!self_incident(f,g))gap=std::min(gap,triangle_triangle_closest(
                self_positions(a,f),self_positions(a,g)).distance);
        }
    return gap;
}
int main(){
    try{
        Body a=fixture(),b=distant(),aa=a,bb=b;
        const auto initial_p=p(a),initial_l=l(a);
        CoupledFEMSettings flat,tree;
        flat.nonincident_self_contact=tree.nonincident_self_contact=true;tree.hierarchy=true;
        std::vector<CoupledFEMLeaf> trace;
        const auto x=step_coupled_fem(a,b,.004,flat,&trace);
        const auto y=step_coupled_fem(aa,bb,.004,tree);
        same(a,aa);same(b,bb);
        need(x.self_a.impulse_updates>0&&x.self_a.normal_impulse>0&&x.impulse_updates==0&&
             x.self_b.impulse_updates==0,"same-body impulse attribution failed");
        need(x.rejected_geometry>0&&x.accepted_leaves>1&&trace.size()==x.accepted_leaves,
             "same-body fast collision did not exercise actual subdivision");
        need(x.self_a.impulse_updates==y.self_a.impulse_updates&&
             x.self_a.normal_impulse==y.self_a.normal_impulse&&
             x.self_a.certificate_tests==y.self_a.certificate_tests&&
             x.self_a.projections==y.self_a.projections&&
             x.accepted_leaves==y.accepted_leaves,"same-body broad-phase parity failed");
        need(norm(p(a)-initial_p)<1e-9&&norm(l(a)-initial_l)<1e-9,
             "same-body contact changed total momentum");
        double nodal=0;for(const auto& n:a.nodes)nodal+=n.last_normal_impulse;
        need(std::abs(nodal-2*x.self_a.normal_impulse)<1e-9&&
             std::abs(a.normal_impulse_step-nodal)<1e-9,"same-body nodal impulse ledger mismatch");
        need(min_gap(a)>flat.skin&&x.certified_min_j>1e-6&&
             std::abs(a.time_seconds-.004)<1e-12,"accepted same-body geometry invalid");
        for(const auto& leaf:trace){
            const auto& before=leaf.before_a;const auto& after=leaf.after_a;
            // Independent endpoint-state kinetic accounting using force
            // kick reconstructed from the source leaf, not contact code.
            auto kicked=before;coupled_detail::kick(kicked,leaf.dt);
            need(std::abs(k(after)-k(kicked)-leaf.receipt.self_a.contact_work)<1e-9,
                 "same-body kinetic work mismatch");
            for(int s=0;s<=32;++s){
                const double t=double(s)/32.;
                for(std::size_t i=0;i<before.surface_faces.size();++i)
                    for(std::size_t j=i+1;j<before.surface_faces.size();++j){
                        const auto& f=before.surface_faces[i];const auto& g=before.surface_faces[j];
                        if(self_incident(f,g))continue;
                        need(triangle_triangle_closest(
                            lerp_vertices(self_positions(before,f),self_positions(after,f),t),
                            lerp_vertices(self_positions(before,g),self_positions(after,g),t)).distance>flat.skin,
                            "sampled accepted same-body path violates clearance");
                    }
            }
        }
        // Incident faces share edges: never impose a positive skin on
        // their mandatory geometric intersection. Single tetra control.
        Body single;single.material.gravity=single.material.damping=0.;
        append(single,0.,1.,0.);
        SelfContactStats s;
        need(certify_nonincident_self_sweep(single,single,flat.skin,12,false,s)&&
             s.certificate_tests==0&&s.incident_exclusions>0,
             "incident topology filter rejected a valid tetrahedron");
        Body fail=fixture(),other=distant();const Body initial=fail,initial_other=other;
        flat.max_bisection=0;bool rejected=false;
        try{(void)step_coupled_fem(fail,other,.004,flat);}
        catch(const std::runtime_error&){rejected=true;}
        need(rejected,"unsafe same-body trial passed zero subdivision budget");
        same(fail,initial);same(other,initial_other);
        std::cout<<"PASS restricted same-body contact updates="<<x.self_a.impulse_updates
                 <<" leaves="<<x.accepted_leaves<<" rejects="<<x.rejected_geometry
                 <<" gap="<<min_gap(a)<<" internal_work="<<x.self_a.contact_work<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

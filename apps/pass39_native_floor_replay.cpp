// Actual native FEM floor/free trajectories; paid matched surface candidate replay.
// The floor solver is not the coupled mesh-mesh solver; candidate generation
// does not influence this integrator. No GPU or whole-solver speedup is claimed.
#include "kyoto/solid_dynamics.hpp"
#include "kyoto/feature_broadphase.hpp"
#include "kyoto/exact_swept_volume_audit.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
namespace s=kyoto::solid;
namespace f=kyoto::contact::features;
namespace g=kyoto::geometry;
using Clock=std::chrono::steady_clock;
static void need(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
static double pz(const s::Body& b){double p=0.;for(const auto& n:b.nodes)p+=n.mass*n.v.z;return p;}
static f::Mesh snapshot(const s::Body& a,const s::Body& b){
    f::Mesh m;m.thickness=.0005;
    for(std::size_t i=0;i<a.nodes.size();++i){
        const auto x=a.nodes[i].x,y=b.nodes[i].x;
        const kyoto::contact::Vec3 p{x.x,x.y,x.z},q{y.x,y.y,y.z};
        m.vertices.push_back({i,{{p,p,q,q}}});
    }
    for(const auto& e:a.surface_edges)m.edges.push_back({m.edges.size(),e});
    for(const auto& t:a.surface_faces)m.faces.push_back({m.faces.size(),t});
    return m;
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("usage: OUTPUT_DIR");
    const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
    std::ofstream topo(dir/"topology.csv"),nodes(dir/"states.csv"),steps(dir/"steps.csv"),timings(dir/"cpu_candidates.csv"),audits(dir/"exact_chords.csv"),pairs(dir/"candidates.csv");
    for(auto* o:{&topo,&nodes,&steps,&timings,&audits})*o<<std::setprecision(17);
    topo<<"size,kind,id,i,j,k,l\n";
    nodes<<"size,regime,step,time,node,rx,ry,rz,x,y,z,vx,vy,vz,mass,fx,fy,fz,stress,impulse\n";
    steps<<"size,regime,step,time,dt,step_ns,kinetic,elastic,gravity,min_j,min_height,impacts,impulse,pz_residual\n";
    timings<<"size,regime,step,block,slot,arm,ns,candidates,aabb_tests,incident_rejections\n";
    audits<<"size,regime,step,certified,cells,possible_pairs,exact_pairs,unknown,orientation_unknown\n";
    pairs<<"size,regime,step,kind,first,second\n";
    constexpr double dt=.001;constexpr int nsteps=120;
    for(unsigned size:{8u,20u,32u})for(int regime=0;regime<2;++regime){
        s::Material mat;mat.gravity=regime?9.81:0.;mat.damping=0.;mat.restitution=.25;mat.friction=.2;
        auto b=s::toroidal_solid(size,2,2,mat);
        const auto label=regime?"impact":"free";
        if(!regime){for(std::size_t i=0;i<b.tets.size();++i){const auto t=b.tets[i].n;topo<<size<<",tet,"<<i<<','<<t[0]<<','<<t[1]<<','<<t[2]<<','<<t[3]<<'\n';}
            for(std::size_t i=0;i<b.surface_faces.size();++i){const auto t=b.surface_faces[i];topo<<size<<",face,"<<i<<','<<t[0]<<','<<t[1]<<','<<t[2]<<",-1\n";}}
        for(auto& n:b.nodes){if(regime){n.x.z-=.30;n.v={.12,0.,-1.};}
            else{n.v={.25*n.x.x,-.15*n.x.y,.1*(n.x.z-.52)};}}
        auto emit=[&](int step){const auto q=b.evaluate();for(std::size_t i=0;i<b.nodes.size();++i){const auto& n=b.nodes[i];const auto z=q.elastic_force[i];
            nodes<<size<<','<<label<<','<<step<<','<<b.time_seconds<<','<<i<<','<<n.rest.x<<','<<n.rest.y<<','<<n.rest.z<<','<<n.x.x<<','<<n.x.y<<','<<n.x.z<<','<<n.v.x<<','<<n.v.y<<','<<n.v.z<<','<<n.mass<<','<<z.x<<','<<z.y<<','<<z.z<<','<<q.nodal_von_mises_pa[i]<<','<<n.last_normal_impulse<<'\n';}};
        std::size_t hits=0,unknown=0;emit(0);
        for(int step=1;step<=nsteps;++step){
            const auto a=b;const auto p0=pz(a);const auto begin=Clock::now();b.step(dt);const auto end=Clock::now();
            const auto d=b.diagnostics();const auto residual=pz(b)-p0+d.total_mass_kg*mat.gravity*dt-b.normal_impulse_step;
            need(std::abs(residual)<1e-8&&d.min_jacobian>1e-6&&d.min_height>=0.,"native floor momentum/orientation/height failure");
            hits+=b.contact_count_step;
            steps<<size<<','<<label<<','<<step<<','<<b.time_seconds<<','<<dt<<','<<std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()<<','<<d.kinetic_joule<<','<<d.elastic_joule<<','<<d.gravity_joule<<','<<d.min_jacobian<<','<<d.min_height<<','<<b.contact_count_step<<','<<b.normal_impulse_step<<','<<residual<<'\n';
            emit(step);
            if(step%10==0){const auto m=snapshot(a,b);const auto truth=f::exhaustive(m).pairs;
                for(const auto& p:truth)pairs<<size<<','<<label<<','<<step<<','<<int(p.kind)<<','<<p.first<<','<<p.second<<'\n';
                for(int block=-1;block<5;++block)for(int slot=0;slot<4;++slot){const bool sap=(slot==0||slot==3)==(block%2==0);
                    const auto t0=Clock::now();const auto r=sap?f::sweep_and_prune(m):f::exhaustive(m);const auto t1=Clock::now();
                    need(r.pairs==truth,"surface candidate parity failed");
                    if(block>=0)timings<<size<<','<<label<<','<<step<<','<<block<<','<<slot<<','<<(sap?"sap":"exhaustive")<<','<<std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count()<<','<<r.pairs.size()<<','<<r.candidate_aabb_tests<<','<<r.incident_rejections<<'\n';}
            }
            if(size==8){std::vector<g::Point> x,y;std::vector<g::ExactTetIds> cells;
                for(std::size_t i=0;i<a.nodes.size();++i){const auto p=a.nodes[i].x,q=b.nodes[i].x;x.push_back({p.x,p.y,p.z});y.push_back({q.x,q.y,q.z});}
                for(const auto& t:a.tets)cells.push_back(t.n);
                const auto r=g::audit_exact_swept_volume(x,y,cells,3,false,true,true);
                const auto unresolved=std::accumulate(r.unknown.begin(),r.unknown.end(),std::size_t{});unknown+=!r.certified;
                audits<<size<<','<<label<<','<<step<<','<<r.certified<<','<<r.cells<<','<<r.pair_tests<<','<<std::accumulate(r.tested.begin(),r.tested.end(),std::size_t{})<<','<<unresolved<<','<<r.orientation_unknown<<'\n';
            }
        }
        need((regime&&hits>0)||(!regime&&hits==0),"floor-contact regime was vacuous");
        std::cerr<<size<<' '<<label<<" impacts="<<hits<<" unresolved chords="<<unknown<<'\n';
    }
    need(topo.good()&&nodes.good()&&steps.good()&&timings.good()&&audits.good()&&pairs.good(),"receipt write failure");
}

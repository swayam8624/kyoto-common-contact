// Pass 28: compare the actual native solver with only its face broad
// phase changed. No synthetic trajectories or assumed speedups.
#include "kyoto/rigid_mesh_fem_contact.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void same(V3 a,V3 b){need(a.x==b.x&&a.y==b.y&&a.z==b.z,"physical vector diverged");}
static void same_body(const Body& a,const Body& b){
    need(a.time_seconds==b.time_seconds&&a.nodes.size()==b.nodes.size()&&
         a.contact_count_step==b.contact_count_step&&a.normal_impulse_step==b.normal_impulse_step,
         "physical state or impulse summary diverged");
    for(std::size_t i=0;i<a.nodes.size();++i){
        same(a.nodes[i].x,b.nodes[i].x);same(a.nodes[i].v,b.nodes[i].v);
        need(a.nodes[i].last_normal_impulse==b.nodes[i].last_normal_impulse,
             "nodal impulse diverged");
    }
}
static void candidate_oracle(){
    std::mt19937_64 rng(28012026);
    std::uniform_real_distribution<double> u(-2.,2.),w(0.,.5);
    auto box=[&](){V3 a{u(rng),u(rng),u(rng)};return TriangleBox{a,a+V3{w(rng),w(rng),w(rng)}};};
    std::vector<TriangleBox> boxes;
    for(int i=0;i<257;++i)boxes.push_back(box());
    StaticFaceHierarchy tree(boxes);
    for(int i=0;i<1000;++i){
        const auto q=box();const double gap=i%3==0?0.:.03;
        std::vector<std::size_t> truth,got;
        for(std::size_t j=0;j<boxes.size();++j)if(maybe_near(q,boxes[j],gap))truth.push_back(j);
        std::size_t tests=0;tree.query(q,gap,got,tests);
        need(truth==got,"ordered face oracle mismatch");
    }
    // Inclusive boundary (touching must remain a candidate).
    std::vector<TriangleBox> touch={{{0,0,0},{1,1,1}},{{2,0,0},{3,1,1}}};
    StaticFaceHierarchy t(touch);std::vector<std::size_t> ids;
    std::size_t tests=0;t.query({{1,0,0},{1,1,1}},0.,ids,tests);
    need(ids==std::vector<std::size_t>{0},"inclusive boundary lost");
    StaticFaceHierarchy empty(std::vector<TriangleBox>{});empty.query({{0,0,0},{1,1,1}},0.,ids,tests);
    need(ids.empty(),"empty hierarchy emitted candidates");
}
static void physical_case(bool fast){
    Body a=toroidal_solid();
    auto obstacle=corrugated_contact_patch();
    if(fast){
        const double shift=audit_deformable_rigid_mesh(a,obstacle,0.).minimum_distance_m-.012;
        for(auto& v:obstacle.vertices)v.z+=shift;
        for(auto& n:a.nodes)n.v={0,0,-20.};
    }
    Body b=a;
    RigidMeshContactSettings flat,tree;tree.use_static_face_hierarchy=true;
    std::size_t flat_tests=0,tree_tests=0,impulses=0,rejections=0,leaves=0;
    double flat_ms=0,tree_ms=0;
    using Clock=std::chrono::steady_clock;
    auto run=[&](Body& body,const RigidMeshContactSettings& cfg,double& ms){
        auto start=Clock::now();
        auto r=step_time_consistent_rigid_triangle_mesh(body,obstacle,.00075,cfg);
        ms+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        return r;
    };
    const int steps=fast?1:450;
    for(int i=0;i<steps;++i){
        MeshAdaptiveStats x,y;
        if(i%2==0){x=run(a,flat,flat_ms);y=run(b,tree,tree_ms);}
        else{y=run(b,tree,tree_ms);x=run(a,flat,flat_ms);}
        same_body(a,b);
        need(x.accepted_leaves==y.accepted_leaves&&
             x.rejected_geom_trials==y.rejected_geom_trials&&
             x.rejected_tetra_trials==y.rejected_tetra_trials&&
             x.continuous_pair_tests==y.continuous_pair_tests&&x.interval_nodes==y.interval_nodes&&
             x.face_impulse_updates==y.face_impulse_updates&&x.active_face_pairs==y.active_face_pairs&&
             x.normal_impulse_ns==y.normal_impulse_ns&&x.tangential_impulse_ns==y.tangential_impulse_ns&&
             x.min_clearance_m==y.min_clearance_m&&x.min_tetra_j==y.min_tetra_j,
             "physical audit or narrow-phase work diverged");
        same(x.external_contact_impulse,y.external_contact_impulse);
        flat_tests+=x.broadphase_box_tests;tree_tests+=y.broadphase_box_tests;
        impulses+=x.face_impulse_updates;rejections+=x.rejected_geom_trials;leaves+=x.accepted_leaves;
    }
    need(impulses>0,"fixture never contacted obstacle");
    if(fast)need(rejections>0&&leaves>1,"fast fixture never adaptively reintegrated");
    need(flat_tests>0&&tree_tests>0,"missing broad-phase counters");
    std::cout<<std::setprecision(17)<<"PHYSICAL_PARITY fast="<<fast<<" steps="<<steps
             <<" impulses="<<impulses<<" rejected="<<rejections<<" leaves="<<leaves
             <<" flat_box_tests="<<flat_tests<<" tree_box_tests="<<tree_tests
             <<" flat_wall_ms="<<flat_ms<<" tree_wall_ms="<<tree_ms
             <<" (exploratory alternating-order timing only)\n";
}
int main(){
    try{candidate_oracle();physical_case(false);physical_case(true);
        std::cout<<"PASS native ordered face hierarchy and actual FEM state parity\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

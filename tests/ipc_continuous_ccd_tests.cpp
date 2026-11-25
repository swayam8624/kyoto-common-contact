// Stage 4: ACTUAL upstream IPC Toolkit v1.6.1 continuous CCD gate.
// Distinguishes "a broad-phase candidate exists" from a true continuous event.
// Public upstream LBVH generates candidate VF/EE pairs. The actual official
// FaceVertexCandidate::ccd and EdgeEdgeCandidate::ccd narrow phase tests them.
//
// For every scene, ALL topologically admissible VF/EE stencils are independently
// enumerated and tested by the same upstream CCD to catch broad-phase misses.
// That is independent ENUMERATION, not independent CCD mathematics.
// Analytic mid-step crossings supply independent event-time ground truth.
//
// This test supports *linear* feature trajectories ONLY. A cubic Bezier curve
// cannot be given to IPC's linear two-endpoint CCD without refinement.
// No solver/contact response or performance improvement is asserted.

#include "kyoto/linear_candidate_adapter.hpp"
#include <ipc/broad_phase/lbvh.hpp>
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>

#include <Eigen/Core>

#include <algorithm>
#include <array>
#include <chrono>
#include <compare>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Pair = std::pair<int,int>;
struct TaggedPair {
    int kind{},a{},b{}; // 0 VF: vertex,face; 1 EE: canonical edge,edge
    auto operator<=>(const TaggedPair&) const = default;
};
struct Scene {
    std::string name;
    Eigen::MatrixXd t0,t1;
    Eigen::MatrixXi edges,faces;
    bool expected_impact{};
    int expected_kind{-1};
    double expected_toi{-1}; // -1 = no independent analytic time
};
struct Audit {
    std::vector<TaggedPair> candidates;
    std::vector<TaggedPair> all_events;
    std::vector<TaggedPair> filtered_events;
    std::size_t vf_candidates{},ee_candidates{};
    std::size_t vf_hits{},ee_hits{};
    double earliest_toi{1};
    double broadphase_ms{},candidate_ccd_ms{},exhaustive_ccd_ms{};
};
void ensure(bool ok,const std::string& explanation) {
    if(!ok)throw std::runtime_error(explanation);
}
Scene vf_cross(double lateral_x=.2,double lateral_y=.2,
               double begin_z=1,double end_z=-1,
               const std::string& name="VF_TRUE_MIDSTEP") {
    Scene s;
    s.name=name;
    s.t0.resize(4,3);s.t1.resize(4,3);
    s.t0<<0,0,0, 1,0,0, 0,1,0, lateral_x,lateral_y,begin_z;
    s.t1=s.t0;
    s.t1.row(3)<<lateral_x,lateral_y,end_z;
    s.edges.resize(0,2);s.faces.resize(1,3);
    s.faces<<0,1,2;
    s.expected_impact=(lateral_x>=0&&lateral_y>=0&&
                      lateral_x+lateral_y<=1&&
                      begin_z>0&&end_z<0);
    s.expected_kind=s.expected_impact?0:-1;
    s.expected_toi=s.expected_impact?begin_z/(begin_z-end_z):-1;
    return s;
}
Scene ee_cross(double lateral_y=0,
               double begin_z=1,double end_z=-1,
               const std::string& name="EE_TRUE_MIDSTEP") {
    Scene s;
    s.name=name;
    s.t0.resize(4,3);s.t1.resize(4,3);
    s.t0<<-1,0,0, 1,0,0,
           0,-1+lateral_y,begin_z,0,1+lateral_y,begin_z;
    s.t1=s.t0;
    s.t1(2,2)=end_z;s.t1(3,2)=end_z;
    s.edges.resize(2,2);s.faces.resize(0,3);
    s.edges<<0,1,2,3;
    s.expected_impact=(std::abs(lateral_y)<1&&begin_z>0&&end_z<0);
    s.expected_kind=s.expected_impact?1:-1;
    s.expected_toi=s.expected_impact?begin_z/(begin_z-end_z):-1;
    return s;
}
Scene crossing_at_different_times() {
    // Two disjoint segments have overlapping whole-step swept spatial
    // boxes, but they retain a positive y separation at every SAME time.
    Scene s; s.name="EE_TIME_ALIAS_NO_COLLISION";
    s.t0.resize(4,3);s.t1.resize(4,3);
    s.t0<<-1,0,0,1,0,0,
           -1,.8,0,1,.8,0;
    s.t1=s.t0;
    for(int i=0;i<4;i++)s.t1(i,1)+=2;
    s.edges.resize(2,2);s.faces.resize(0,3);
    s.edges<<0,1,2,3;
    s.expected_impact=false;s.expected_kind=-1;
    return s;
}
Scene random_mixed(int seed) {
    // Independently randomized linear vertex motions. Topology is built
    // from VALID unique vertex indices; some pairs may be near each other.
    Scene s;s.name="SEEDED_MIXED_"+std::to_string(seed);
    std::mt19937_64 gen(934139+std::uint64_t(seed));
    std::uniform_real_distribution<double> u(-1.6,1.6);
    std::uniform_real_distribution<double> d(-.8,.8);
    constexpr int N=10;
    s.t0.resize(N,3);s.t1.resize(N,3);
    for(int i=0;i<N;i++)for(int j=0;j<3;j++){
        s.t0(i,j)=u(gen);
        s.t1(i,j)=s.t0(i,j)+d(gen);
    }
    s.edges.resize(4,2);
    s.edges<<0,1,2,3,4,5,6,7;
    s.faces.resize(2,3);
    s.faces<<0,2,4, 5,7,9;
    return s;
}
void unique(std::vector<TaggedPair>& p) {
    std::sort(p.begin(),p.end());
    p.erase(std::unique(p.begin(),p.end()),p.end());
}
bool incident_vf(const Scene& s,int vertex,int face) {
    for(int k=0;k<3;k++)
        if(s.faces(face,k)==vertex)return true;
    return false;
}
bool incident_ee(const Scene& s,int a,int b) {
    for(int i=0;i<2;i++)for(int j=0;j<2;j++)
        if(s.edges(a,i)==s.edges(b,j))return true;
    return false;
}
bool actual_ccd(const Scene& s,TaggedPair p,double& toi) {
    if(p.kind==0) {
        ipc::FaceVertexCandidate candidate(p.b,p.a);
        return candidate.ccd(
            candidate.dof(s.t0,s.edges,s.faces),
            candidate.dof(s.t1,s.edges,s.faces),toi);
    }
    ipc::EdgeEdgeCandidate candidate(p.a,p.b);
    return candidate.ccd(
        candidate.dof(s.t0,s.edges,s.faces),
        candidate.dof(s.t1,s.edges,s.faces),toi);
}
// Pass 27: replace the broad-phase producer, while retaining exactly
// the existing upstream CCD implementation and original matrix inputs.
// Check the entire event set and earliest conservative safe prefix.
// The persistent adapter survives topology changes between scenes.
void audit_sca(const Scene& s,const Audit& a,
               kyoto::contact::atlas::LinearCandidateAdapter& adapter) {
    using namespace kyoto::contact;
    std::vector<Vec3> start,end;
    std::vector<atlas::IndexedEdge> edges;
    std::vector<atlas::IndexedFace> faces;
    for(int i=0;i<s.t0.rows();++i){
        start.push_back({s.t0(i,0),s.t0(i,1),s.t0(i,2)});
        end.push_back({s.t1(i,0),s.t1(i,1),s.t1(i,2)});
    }
    for(int i=0;i<s.edges.rows();++i)
        edges.push_back({std::size_t(s.edges(i,0)),std::size_t(s.edges(i,1))});
    for(int i=0;i<s.faces.rows();++i)
        faces.push_back({std::size_t(s.faces(i,0)),std::size_t(s.faces(i,1)),
                         std::size_t(s.faces(i,2))});
    const auto pairs=adapter.query(start,end,edges,faces).pairs;
    const auto prior_refits=adapter.refits();
    const auto repeated=adapter.query(start,end,edges,faces).pairs;
    ensure(pairs==repeated && adapter.refits()==prior_refits+1,
           s.name+": SCA repeated query lost parity or warm state");
    std::vector<TaggedPair> events;
    double earliest=1.;
    for(const auto& p:pairs){
        TaggedPair tag;
        if(p.kind==features::Kind::VertexFace){
            ensure(p.first<start.size()&&p.second<faces.size(),
                   s.name+": SCA invalid VF row index");
            tag={0,int(p.first),int(p.second)};
            ensure(!incident_vf(s,tag.a,tag.b),
                   s.name+": SCA emitted incident VF");
        }else{
            ensure(p.first<edges.size()&&p.second<edges.size()&&p.first<p.second,
                   s.name+": SCA invalid EE row index");
            tag={1,int(p.first),int(p.second)};
            ensure(!incident_ee(s,tag.a,tag.b),
                   s.name+": SCA emitted incident EE");
        }
        double toi=1.;
        if(actual_ccd(s,tag,toi)){
            ensure(std::isfinite(toi)&&toi>=0&&toi<=1,
                   s.name+": SCA/IPC invalid safe prefix");
            events.push_back(tag);
            earliest=std::min(earliest,toi);
        }
    }
    unique(events);
    ensure(events==a.all_events,
           s.name+": SCA supplied a different upstream CCD event set");
    ensure(std::abs(earliest-a.earliest_toi)<=1e-12,
           s.name+": SCA supplied a different earliest CCD safe prefix");
    std::cout<<"SCA case="<<s.name<<" candidates="<<pairs.size()
             <<" upstream_events="<<events.size()
             <<" earliest_safe_prefix="<<earliest<<'\n';
}
Audit validate(const Scene& s) {
    Audit a;
    ipc::LBVH official;
    official.can_vertices_collide=[](auto,auto){return true;};
    const auto t0=std::chrono::steady_clock::now();
    official.build(s.t0,s.t1,s.edges,s.faces,0.);
    std::vector<ipc::EdgeEdgeCandidate> ee;
    std::vector<ipc::FaceVertexCandidate> fv;
    official.detect_edge_edge_candidates(ee);
    official.detect_face_vertex_candidates(fv);
    const auto t1=std::chrono::steady_clock::now();
    a.broadphase_ms=std::chrono::duration<double,std::milli>(t1-t0).count();
    a.vf_candidates=fv.size();a.ee_candidates=ee.size();
    for(const auto& c:fv)
        a.candidates.push_back({0,int(c.vertex_id),int(c.face_id)});
    for(const auto& c:ee)
        a.candidates.push_back({1,std::min(int(c.edge0_id),int(c.edge1_id)),
                                 std::max(int(c.edge0_id),int(c.edge1_id))});
    unique(a.candidates);
    const auto t2=std::chrono::steady_clock::now();
    for(const auto& candidate:a.candidates) {
        double toi=1.0;
        if(actual_ccd(s,candidate,toi)) {
            ensure(std::isfinite(toi)&&toi>=0&&toi<=1,
                   s.name+": upstream CCD returned invalid TOI");
            a.filtered_events.push_back(candidate);
            a.earliest_toi=std::min(a.earliest_toi,toi);
            if(candidate.kind==0)a.vf_hits++;else a.ee_hits++;
        }
    }
    const auto t3=std::chrono::steady_clock::now();
    a.candidate_ccd_ms=std::chrono::duration<double,std::milli>(t3-t2).count();
    // EXHAUSTIVE independent enumeration: check every admissible
    // VF and EE pair, regardless of whether broad phase reported it.
    for(int v=0;v<s.t0.rows();v++)
        for(int f=0;f<s.faces.rows();f++){
            if(incident_vf(s,v,f))continue;
            double toi=1.0;
            if(actual_ccd(s,{0,v,f},toi))a.all_events.push_back({0,v,f});
        }
    for(int i=0;i<s.edges.rows();i++)
        for(int j=i+1;j<s.edges.rows();j++) {
            if(incident_ee(s,i,j))continue;
            double toi=1.0;
            if(actual_ccd(s,{1,i,j},toi))a.all_events.push_back({1,i,j});
        }
    const auto t4=std::chrono::steady_clock::now();
    a.exhaustive_ccd_ms=std::chrono::duration<double,std::milli>(t4-t3).count();
    unique(a.all_events);unique(a.filtered_events);
    ensure(a.all_events==a.filtered_events,
           s.name+": missing real official CCD event in LBVH");
    if(s.expected_kind>=0) {
        ensure(!a.all_events.empty(),s.name+": missing analytic collision");
        ensure(std::any_of(a.all_events.begin(),a.all_events.end(),
                [&](const TaggedPair& p){return p.kind==s.expected_kind;}),
               s.name+": wrong event feature family");
        // The official CCD may return a conservative pre-impact fraction.
        // Require a meaningful safe prefix, not falsely claim exact TOI.
        ensure(a.earliest_toi<=s.expected_toi+1e-5 &&
               a.earliest_toi>=s.expected_toi-.12,
               s.name+": CCD TOI inconsistent with analytic crossing");
    }else if(s.name.find("SEEDED_")!=0) {
        ensure(a.all_events.empty(),s.name+": unexpected CCD event");
    }
    return a;
}
void sample_export(const Scene& s,std::ostream& dst) {
    // All coordinates originate in the native C++ linear scene. Vertices
    // of the two primitive kinds are sampled for visualization; not an
    // independent numerical CCD implementation.
    for(int frame=0;frame<=120;frame++){
        const double t=double(frame)/120.;
        for(int id=0;id<s.t0.rows();id++) {
            const Eigen::Vector3d p=
                (1-t)*s.t0.row(id).transpose()+t*s.t1.row(id).transpose();
            dst<<s.name<<','<<frame<<','<<t<<','<<id<<','
               <<p.x()<<','<<p.y()<<','<<p.z()<<'\n';
        }
    }
}
} // namespace
int main(int argc,char**argv) {
    try {
        if(argc>2)throw std::invalid_argument("usage: kyoto_official_ipc_ccd_tests [output_dir]");
        std::vector<Scene> cases={
            vf_cross(),vf_cross(.8,.8,1,-1,"VF_BOX_FALSE_POSITIVE"),
            vf_cross(.2,.2,1,2,"VF_DISJOINT_Z"),
            vf_cross(.2,.2,1,-1.5,"VF_TRUE_ASYMMETRIC"),
            ee_cross(),ee_cross(2.2,1,-1,"EE_SEPARATE_Y"),
            crossing_at_different_times()
        };
        for(int i=0;i<28;i++)cases.push_back(random_mixed(i));
        std::filesystem::path output;
        std::ofstream audit,samples;
        const bool export_data=argc==2;
        if(export_data) {
            output=argv[1];
            std::filesystem::create_directories(output);
            audit.open(output/"ccd_cases.csv");
            audit<<std::setprecision(17);
            audit<<"case_name,source,vf_candidates,ee_candidates,total_candidates,"
                    "vf_events,ee_events,total_events,exhaustive_events,"
                    "analytic_impact,analytic_toi,ccd_earliest_toi,"
                    "official_lbvh_paid_ms,official_candidate_ccd_ms,"
                    "official_exhaustive_ccd_ms\n";
            samples.open(output/"linear_vertex_paths.csv");
            samples<<std::setprecision(17);
            samples<<"case_name,frame,t,vertex_id,x,y,z\n";
        }
        std::size_t count=0,events=0,missing=0,ambiguous=0;
        kyoto::contact::atlas::LinearCandidateAdapter sca;
        for(const auto& scene:cases) {
            const Audit a=validate(scene);
            audit_sca(scene,a,sca);
            count++;
            events+=a.all_events.size();
            if(export_data) {
                audit<<scene.name<<",IPC_v1.6.1_real_LBVH_and_real_CCD,"
                     <<a.vf_candidates<<','<<a.ee_candidates<<','
                     <<a.candidates.size()<<','<<a.vf_hits<<','
                     <<a.ee_hits<<','<<a.filtered_events.size()<<','
                     <<a.all_events.size()<<','<<int(scene.expected_impact)
                     <<','<<scene.expected_toi<<','<<a.earliest_toi<<','
                     <<a.broadphase_ms<<','<<a.candidate_ccd_ms<<','
                     <<a.exhaustive_ccd_ms<<'\n';
                if(scene.name.find("SEEDED_")==0 ||
                   scene.name=="VF_TRUE_MIDSTEP" ||
                   scene.name=="VF_BOX_FALSE_POSITIVE" ||
                   scene.name=="EE_TRUE_MIDSTEP" ||
                   scene.name=="EE_TIME_ALIAS_NO_COLLISION")
                    sample_export(scene,samples);
            }
            if(scene.expected_kind<0 &&
               scene.name.find("SEEDED_")==0)
                ++ambiguous; // no independent event-time labels on random scenes
            std::cout<<"case="<<scene.name
                     <<" candidates="<<a.candidates.size()
                     <<" VF_events="<<a.vf_hits<<" EE_events="<<a.ee_hits
                     <<" earliest_TOI="<<a.earliest_toi<<'\n';
        }
        ensure(sca.rebuilds()>1 && sca.refits()>=count,
               "SCA topology change and warm reuse were not exercised");
        std::cout<<"SCA upstream CCD parity gate: "<<count
                 <<" scenes; rebuilds="<<sca.rebuilds()
                 <<" refits="<<sca.refits()<<'\n';
        if(export_data) {
            ensure(audit.good()&&samples.good(),"cannot write CCD evidence");
            audit.close();samples.close();
            std::ofstream manifest(output/"manifest.json");
            manifest<<"{\n"
                    " \"stage\": 4,\n"
                    " \"upstream\": \"real IPC Toolkit v1.6.1 pinned release\",\n"
                    " \"trajectories\": \"linear only\",\n"
                    " \"scenes\": "<<count<<",\n"
                    " \"constructive_analytic_scenarios\": 7,\n"
                    " \"randomized_scenarios_without_analytic_toi\": "<<ambiguous<<",\n"
                    " \"independent_feature_enumeration\": true,\n"
                    " \"independent_CCD_implementation\": false,\n"
                    " \"physics_solver\": false,\n"
                    " \"whole_solver_speedup\": false\n"
                    "}\n";
        }
        std::cout<<"PASS real IPC CCD: "<<count
                 <<" scenes; total detected stencil events="<<events
                 <<"; broad-phase misses="<<missing
                 <<"; 28 seeded random topological scenes"
                 <<"; exact analytical crossing at midstep included\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<'\n';
        return 1;
    }
}

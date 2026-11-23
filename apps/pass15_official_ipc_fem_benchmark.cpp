// Pass 15: pinned OFFICIAL IPC Toolkit LBVH versus our maintained
// refittable hierarchy on EXACT same original physical FEM snapshots.
// Candidate coverage is mandatory. Timings are C++ native paid pipeline
// timings only, not full solver or continuous narrow-phase CCD.
#include "kyoto/fem_feature_snapshots.hpp"
#include "kyoto/refit_feature_hierarchy.hpp"
#include <ipc/broad_phase/lbvh.hpp>
#include <ipc/broad_phase/aabb.hpp>
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <Eigen/Core>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace f=kyoto::contact::features;
namespace fem=kyoto::contact::fem;
namespace s=kyoto::solid;
using Clock=std::chrono::steady_clock;

static std::int64_t ns(Clock::time_point a,Clock::time_point b){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count();
}
struct Source {
    int step{};
    double time{};
    std::size_t impulses{},leaves{};
    f::Mesh mesh;
    std::string regime;
    int physical_leaf_id{-1};
};
struct IPCPrepared {
    ipc::AABBs vertex_boxes;
    Eigen::MatrixXi E,F;
};

static IPCPrepared prepare(const f::Mesh& mesh){
    // This full f::build, ID mapping, topology conversion and all AABB
    // allocation are inside official IPC's paid conversion timestamp.
    const auto built=f::build(mesh);
    if(mesh.vertices.size()>std::size_t(std::numeric_limits<int>::max()) ||
       mesh.edges.size()>std::size_t(std::numeric_limits<int>::max()) ||
       mesh.faces.size()>std::size_t(std::numeric_limits<int>::max()))
        throw std::overflow_error("official IPC feature index overflow");
    for(const auto& v:mesh.vertices)
        if(v.category!=1||v.mask!=~std::uint64_t{0})
            throw std::invalid_argument("unsupported IPC vertex mask");
    for(const auto& e:mesh.edges)
        if(e.category!=1||e.mask!=~std::uint64_t{0})
            throw std::invalid_argument("unsupported IPC edge mask");
    for(const auto& face:mesh.faces)
        if(face.category!=1||face.mask!=~std::uint64_t{0})
            throw std::invalid_argument("unsupported IPC face mask");
    IPCPrepared out;
    std::map<std::uint64_t,int> indices;
    out.vertex_boxes.reserve(built.vertices.size());
    for(std::size_t i=0;i<mesh.vertices.size();++i){
        const auto& b=built.vertices[i].bounds;
        indices.emplace(mesh.vertices[i].id,static_cast<int>(i));
        ipc::AABB box(
            Eigen::Array3d(b.min.x,b.min.y,b.min.z),
            Eigen::Array3d(b.max.x,b.max.y,b.max.z));
        box.vertex_ids={static_cast<ipc::index_t>(i),-1,-1};
        out.vertex_boxes.push_back(box);
    }
    out.E.resize(static_cast<Eigen::Index>(mesh.edges.size()),2);
    out.F.resize(static_cast<Eigen::Index>(mesh.faces.size()),3);
    for(std::size_t i=0;i<mesh.edges.size();++i)
        for(int j=0;j<2;++j)
            out.E(static_cast<Eigen::Index>(i),j)=
                indices.at(mesh.edges[i].vertices[j]);
    for(std::size_t i=0;i<mesh.faces.size();++i)
        for(int j=0;j<3;++j)
            out.F(static_cast<Eigen::Index>(i),j)=
                indices.at(mesh.faces[i].vertices[j]);
    return out;
}
static std::vector<f::Candidate> collect(ipc::LBVH& b,
                                         const f::Mesh& mesh){
    std::vector<ipc::EdgeEdgeCandidate> ee;
    std::vector<ipc::FaceVertexCandidate> fv;
    b.detect_edge_edge_candidates(ee);
    b.detect_face_vertex_candidates(fv);
    std::vector<f::Candidate> out;
    out.reserve(ee.size()+fv.size());
    for(const auto& p:ee) {
        const auto a=mesh.edges.at(p.edge0_id).id;
        const auto c=mesh.edges.at(p.edge1_id).id;
        out.push_back({f::Kind::EdgeEdge,std::min(a,c),std::max(a,c)});
    }
    for(const auto& p:fv)
        out.push_back({f::Kind::VertexFace,
            mesh.vertices.at(p.vertex_id).id,mesh.faces.at(p.face_id).id});
    std::sort(out.begin(),out.end());
    out.erase(std::unique(out.begin(),out.end()),out.end());
    return out;
}
struct PaidIPC {
    std::vector<f::Candidate> pairs;
    std::int64_t conversion_ns{},build_ns{},query_ns{};
};
static PaidIPC official(const f::Mesh& mesh){
    const auto a=Clock::now();
    auto input=prepare(mesh);
    const auto b=Clock::now();
    ipc::LBVH lbvh;
    lbvh.can_vertices_collide=[](auto,auto){return true;};
    lbvh.build(input.vertex_boxes,input.E,input.F,
               static_cast<std::uint8_t>(3));
    const auto c=Clock::now();
    auto pairs=collect(lbvh,mesh);
    const auto d=Clock::now();
    return {std::move(pairs),ns(a,b),ns(b,c),ns(c,d)};
}
static std::vector<Source> generate(){
    s::Body body=s::toroidal_solid();
    const auto rigid=s::corrugated_contact_patch();
    std::vector<Source> data;
    data.reserve(36);
    for(int step=1;step<=360;++step){
        const s::Body previous=body;
        const auto result=s::step_time_consistent_rigid_triangle_mesh(
            body,rigid,.00075);
        if(result.accepted_leaves<1)
            throw std::runtime_error("unaccepted physical macro-step");
        if(step%12!=0)continue;
        data.push_back({step,body.time_seconds,
            result.face_impulse_updates,result.accepted_leaves,
            fem::feature_snapshot(previous,body,rigid),
            "routine", -1});
    }
    if(data.size()!=30)
        throw std::runtime_error("native FEM routine snapshot count mismatch");

    // Actual fast finite-triangle impact, recorded as the *accepted*
    // solver-generated physical leaves. This is a separate regime of
    // the same real 3D hyperelastic solver, not an independent solver.
    s::Body fast=s::toroidal_solid();
    auto raised=s::corrugated_contact_patch();
    const auto original_gap=s::audit_deformable_rigid_mesh(fast,raised,0.);
    const double shift=original_gap.minimum_distance_m-.012;
    for(auto& vertex:raised.vertices)vertex.z+=shift;
    for(auto& node:fast.nodes)node.v={0,0,-20.0};
    std::vector<s::MeshAcceptedTransition> physical_leaves;
    const auto impact=s::step_time_consistent_rigid_triangle_mesh(
        fast,raised,.00075,{},nullptr,&physical_leaves);
    if(impact.accepted_leaves<=1 || impact.rejected_geom_trials==0 ||
       physical_leaves.size()!=impact.accepted_leaves)
        throw std::runtime_error("fast FEM source was not adaptively subdivided");
    for(std::size_t j=0;j<physical_leaves.size();++j){
        const auto& leaf=physical_leaves[j];
        if(!(leaf.receipt.dt_s>0 &&
             leaf.after.time_seconds>leaf.before.time_seconds))
            throw std::runtime_error("invalid accepted physical leaf time");
        auto features=fem::feature_snapshot(leaf.before,leaf.after,raised);
        features.topology_epoch=2; // Different scene/obstacle: pay rebuild
        data.push_back({361+static_cast<int>(j),leaf.after.time_seconds,
            leaf.receipt.face_impulse_updates,1,std::move(features),
            "fast_adaptive",static_cast<int>(j)});
    }
    return data;
}
int main(int argc,char**argv){
 try{
    if(argc!=2)throw std::invalid_argument(
        "usage: kyoto_pass15_official_ipc_fem_benchmark OUTPUT_FOLDER");
    const auto directory=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(directory);
    const auto data=generate(); // Excluded from both methods' costs.
    std::ofstream csv(directory/"official_ipc_fem_paired.csv");
    if(!csv)throw std::runtime_error("failed to create original receipt");
    csv<<"repeat,step,regime,physical_leaf_id,physical_time_s,source_face_impulse_updates,"
          "accepted_physical_leaves,reference_pairs,official_ipc_pairs,"
          "maintained_hierarchy_pairs,official_extra,official_missing,"
          "official_conversion_ns,official_build_ns,official_query_ns,"
          "maintained_paid_total_ns,hierarchy_rebuilds,hierarchy_refits,"
          "first_method,uncached_hierarchy_paid_ns,uncached_hierarchy_pairs,"
          "first_internal_hierarchy_method\n";
    csv<<std::setprecision(17);
    constexpr int repeats=12;
    std::size_t rows=0,with_contact=0;
    for(int trial=0;trial<repeats;++trial){
        kyoto::contact::atlas::RefitHierarchy hierarchy;
        kyoto::contact::atlas::RefitHierarchy uncached_hierarchy(false);
        for(std::size_t i=0;i<data.size();++i){
            const auto& source=data[i];
            const auto oracle=f::exhaustive(source.mesh).pairs;
            PaidIPC upstream;
            f::Result ours,uncached;
            std::int64_t ours_ns=0,uncached_ns=0;
            const bool ipc_first=((i+std::size_t(trial))%2==0);
            auto our_run=[&](){
                const auto begin=Clock::now();
                ours=hierarchy.query(source.mesh);
                ours_ns=ns(begin,Clock::now());
            };
            auto uncached_run=[&](){
                const auto begin=Clock::now();
                uncached=uncached_hierarchy.query(source.mesh);
                uncached_ns=ns(begin,Clock::now());
            };
            // Independently counterbalance IPC-vs-local and retained-vs-
            // uncached hierarchy order, so a fixed warm-cache advantage
            // cannot masquerade as a better spatial data structure.
            const bool cached_first=((i+std::size_t(trial/2))%2==0);
            auto run_pair=[&](){
                if(cached_first){our_run();uncached_run();}
                else{uncached_run();our_run();}
            };
            if(ipc_first){upstream=official(source.mesh);run_pair();}
            else{run_pair();upstream=official(source.mesh);}
            if(uncached.pairs!=oracle)
                throw std::runtime_error(
                    "UNCACHED HIERARCHY CANDIDATE PARITY FAILURE at step "+
                    std::to_string(source.step));
            if(ours.pairs!=oracle)
                throw std::runtime_error(
                    "MAINTAINED HIERARCHY MISSED OR EXTRAS at step "+
                    std::to_string(source.step));
            std::vector<f::Candidate> missing;
            std::set_difference(oracle.begin(),oracle.end(),
                upstream.pairs.begin(),upstream.pairs.end(),
                std::back_inserter(missing));
            if(!missing.empty())
                throw std::runtime_error(
                    "OFFICIAL IPC FAILED MATCHED SEMANTICS COVERAGE at step "+
                    std::to_string(source.step)+" missing="+
                    std::to_string(missing.size()));
            const auto extra=upstream.pairs.size()-oracle.size();
            csv<<trial<<','<<source.step<<','<<source.regime<<','
               <<source.physical_leaf_id<<','<<source.time<<','
               <<source.impulses<<','<<source.leaves<<','<<oracle.size()<<','
               <<upstream.pairs.size()<<','<<ours.pairs.size()<<','
               <<extra<<",0,"<<upstream.conversion_ns<<','
               <<upstream.build_ns<<','<<upstream.query_ns<<','
               <<ours_ns<<','<<hierarchy.rebuilds()<<','
               <<hierarchy.refits()<<','
               <<(ipc_first?"ipc":"hierarchy")<<','
               <<uncached_ns<<','<<uncached.pairs.size()<<','
               <<(cached_first?"cached":"uncached")<<'\n';
            ++rows;
            if(source.impulses)++with_contact;
        }
    }
    std::cout<<"PASS15: pinned upstream IPC Toolkit LBVH on routine real FEM "
             <<"plus actual fast impact adaptive physical leaves; "
             <<rows<<" matched repeated solver-FEM snapshot rows; "
             <<with_contact<<" contact-bearing repeated snapshots; "
             <<"independent oracle candidate coverage and "
             <<"12-repeat cached-vs-uncached BVH control. "
             <<"No narrowphase CCD or solver-step speedup claim.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS15 FAILURE: "<<e.what()<<'\n';return 1;
 }
}

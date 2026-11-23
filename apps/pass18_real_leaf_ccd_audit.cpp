// Pass 18: independent exhaustive cross-object upstream VF/EE CCD
// narrowphase audit on ACTUALLY ACCEPTED 20m/s FEM impact leaves.
//
// Unlike candidate-set parity alone, this enumerates EVERY admissible
// cross-object VF/EE stencil (regardless of box overlap), evaluates the
// REAL pinned IPC Toolkit narrowphase, and checks that each detected
// CCD event was retained by both the maintained hierarchy and upstream
// LBVH. This is independent enumeration, NOT a second CCD algorithm.
// It neither asserts self-contact safety nor physical material accuracy.
#include "kyoto/fem_feature_snapshots.hpp"
#include "kyoto/refit_feature_hierarchy.hpp"
#include <ipc/broad_phase/lbvh.hpp>
#include <ipc/broad_phase/aabb.hpp>
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace f=kyoto::contact::features;
namespace s=kyoto::solid;
namespace fem=kyoto::contact::fem;

struct UpstreamInput {
    Eigen::MatrixXd start,end;
    Eigen::MatrixXi edges,faces;
    ipc::AABBs vertex_boxes;
    std::map<std::uint64_t,std::size_t> indices;
};
static UpstreamInput prepare(const f::Mesh& mesh) {
    const auto checked=f::build(mesh);
    UpstreamInput m;
    const auto nv=static_cast<Eigen::Index>(mesh.vertices.size());
    m.start.resize(nv,3);m.end.resize(nv,3);
    m.edges.resize(static_cast<Eigen::Index>(mesh.edges.size()),2);
    m.faces.resize(static_cast<Eigen::Index>(mesh.faces.size()),3);
    m.vertex_boxes.reserve(mesh.vertices.size());
    for(std::size_t i=0;i<mesh.vertices.size();++i) {
        const auto& source=mesh.vertices[i];
        m.indices.emplace(source.id,i);
        const auto a=source.path.control.front();
        const auto b=source.path.control.back();
        m.start.row(static_cast<Eigen::Index>(i))<<a.x,a.y,a.z;
        m.end.row(static_cast<Eigen::Index>(i))<<b.x,b.y,b.z;
        const auto bb=checked.vertices[i].bounds;
        ipc::AABB box(Eigen::Array3d(bb.min.x,bb.min.y,bb.min.z),
                      Eigen::Array3d(bb.max.x,bb.max.y,bb.max.z));
        box.vertex_ids={static_cast<ipc::index_t>(i),-1,-1};
        m.vertex_boxes.push_back(box);
    }
    for(std::size_t i=0;i<mesh.edges.size();++i)
        for(int k=0;k<2;++k)
            m.edges(static_cast<Eigen::Index>(i),k)=
                static_cast<int>(m.indices.at(mesh.edges[i].vertices[k]));
    for(std::size_t i=0;i<mesh.faces.size();++i)
        for(int k=0;k<3;++k)
            m.faces(static_cast<Eigen::Index>(i),k)=
                static_cast<int>(m.indices.at(mesh.faces[i].vertices[k]));
    return m;
}
static std::vector<f::Candidate> upstream_candidates(
    const f::Mesh& mesh,const UpstreamInput& input){
    ipc::LBVH bp;
    bp.can_vertices_collide=[](auto,auto){return true;};
    bp.build(input.vertex_boxes,input.edges,input.faces,
             static_cast<std::uint8_t>(3));
    std::vector<ipc::FaceVertexCandidate> vf;
    std::vector<ipc::EdgeEdgeCandidate> ee;
    bp.detect_face_vertex_candidates(vf);
    bp.detect_edge_edge_candidates(ee);
    std::vector<f::Candidate> out;
    for(const auto& p:vf)
        out.push_back({f::Kind::VertexFace,
            mesh.vertices.at(p.vertex_id).id,mesh.faces.at(p.face_id).id});
    for(const auto& p:ee){
        const auto a=mesh.edges.at(p.edge0_id).id;
        const auto b=mesh.edges.at(p.edge1_id).id;
        out.push_back({f::Kind::EdgeEdge,std::min(a,b),std::max(a,b)});
    }
    std::sort(out.begin(),out.end());
    out.erase(std::unique(out.begin(),out.end()),out.end());
    return out;
}
static void require(bool condition,const char* reason) {
    if(!condition)throw std::runtime_error(reason);
}
int main(int argc,char**argv) {
 try{
    if(argc!=2)throw std::invalid_argument(
        "usage: kyoto_pass18_real_leaf_ccd_audit OUTPUT_DIR");
    const auto dir=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(dir);
    std::ofstream trace(dir/"actual_fast_leaf_exhaustive_ipc_ccd.csv");
    if(!trace)throw std::runtime_error("cannot open native event audit CSV");
    trace<<std::setprecision(17);
    trace<<"leaf_id,start_time_s,end_time_s,accepted_dt_s,full_face_gap_m,"
           "cross_vf_stencils,cross_ee_stencils,official_CCD_vf_events,"
           "official_CCD_ee_events,events_missing_from_independent_AABB_oracle,"
           "events_missing_from_official_LBVH,"
           "events_missing_from_maintained_hierarchy,earliest_upstream_toi\n";

    s::Body body=s::toroidal_solid();
    auto rigid=s::corrugated_contact_patch();
    const auto initial=s::audit_deformable_rigid_mesh(body,rigid,0.);
    const auto shift=initial.minimum_distance_m-.012;
    for(auto& p:rigid.vertices)p.z+=shift;
    for(auto& node:body.nodes)node.v={0,0,-20.};
    std::vector<s::MeshAcceptedTransition> leaves;
    const auto simulated=s::step_time_consistent_rigid_triangle_mesh(
        body,rigid,.00075,{},nullptr,&leaves);
    require(simulated.rejected_geom_trials>0&&leaves.size()>1,
            "source high-speed FEM never entered adaptive CCD path");
    require(leaves.size()==simulated.accepted_leaves,
            "original FEM leaf snapshot count is incomplete");

    kyoto::contact::atlas::RefitHierarchy maintained(true,true);
    std::size_t total_events=0,global_misses=0;
    for(std::size_t leaf_id=0;leaf_id<leaves.size();++leaf_id){
        const auto& leaf=leaves[leaf_id];
        const auto accepted_surface=s::audit_deformable_rigid_mesh(
            leaf.after,rigid,0.);
        require(accepted_surface.overlapping_face_pairs==0 &&
                accepted_surface.minimum_distance_m>0,
                "original accepted FEM leaf intersects rigid triangular mesh");
        auto mesh=fem::feature_snapshot(leaf.before,leaf.after,rigid);
        mesh.topology_epoch=2;
        const auto input=prepare(mesh);
        const auto truth=f::exhaustive(mesh).pairs;
        const auto tree=maintained.query(mesh).pairs;
        require(tree==truth,"physical source tree candidate coverage failure");
        const auto official=upstream_candidates(mesh,input);
        std::size_t nVF=0,nEE=0,vfHit=0,eeHit=0,missAABB=0,missIPC=0,missTree=0;
        double earliest=1.0;

        // Enumerate all cross-object stencils, even those rejected by
        // bounding volumes. This is the independent event COVERAGE oracle.
        for(std::size_t vertex=0;vertex<mesh.vertices.size();++vertex)
            for(std::size_t face=0;face<mesh.faces.size();++face) {
                const auto& v=mesh.vertices[vertex];
                const auto& tri=mesh.faces[face];
                const auto owner=mesh.vertices[input.indices.at(
                    tri.vertices[0])].object;
                if(v.object==owner)continue; // self-contact not solved
                ++nVF;
                ipc::FaceVertexCandidate test(
                    static_cast<ipc::index_t>(face),
                    static_cast<ipc::index_t>(vertex));
                double toi=1.0;
                if(!test.ccd(test.dof(input.start,input.edges,input.faces),
                             test.dof(input.end,input.edges,input.faces),toi))
                    continue;
                require(std::isfinite(toi)&&toi>=0&&toi<=1,
                        "upstream VF CCD produced invalid TOI");
                earliest=std::min(earliest,toi);++vfHit;
                const f::Candidate key{f::Kind::VertexFace,v.id,tri.id};
                missAABB+=!std::binary_search(truth.begin(),truth.end(),key);
                missIPC+=!std::binary_search(official.begin(),official.end(),key);
                missTree+=!std::binary_search(tree.begin(),tree.end(),key);
            }
        for(std::size_t a=0;a<mesh.edges.size();++a)
            for(std::size_t b=a+1;b<mesh.edges.size();++b) {
                const auto& e=mesh.edges[a];
                const auto& g=mesh.edges[b];
                const auto ownerA=mesh.vertices[input.indices.at(e.vertices[0])].object;
                const auto ownerB=mesh.vertices[input.indices.at(g.vertices[0])].object;
                if(ownerA==ownerB)continue;
                ++nEE;
                ipc::EdgeEdgeCandidate test(
                    static_cast<ipc::index_t>(a),
                    static_cast<ipc::index_t>(b));
                double toi=1.0;
                if(!test.ccd(test.dof(input.start,input.edges,input.faces),
                             test.dof(input.end,input.edges,input.faces),toi))
                    continue;
                require(std::isfinite(toi)&&toi>=0&&toi<=1,
                        "upstream EE CCD produced invalid TOI");
                earliest=std::min(earliest,toi);++eeHit;
                const f::Candidate key{f::Kind::EdgeEdge,
                    std::min(e.id,g.id),std::max(e.id,g.id)};
                missAABB+=!std::binary_search(truth.begin(),truth.end(),key);
                missIPC+=!std::binary_search(official.begin(),official.end(),key);
                missTree+=!std::binary_search(tree.begin(),tree.end(),key);
            }
        trace<<leaf_id<<','<<leaf.receipt.start_time_s<<','
             <<leaf.receipt.end_time_s<<','<<leaf.receipt.dt_s<<','
             <<accepted_surface.minimum_distance_m<<','<<nVF<<','
             <<nEE<<','<<vfHit<<','<<eeHit<<','
             <<missAABB<<','<<missIPC<<','<<missTree<<','<<earliest<<'\n';
        total_events+=vfHit+eeHit;
        global_misses+=missAABB+missIPC+missTree;
        std::cout<<"actual_leaf="<<leaf_id<<" cross-VF="<<nVF
                 <<" cross-EE="<<nEE<<" upstream_CCD_hits="<<vfHit+eeHit
                 <<" broadphase_event_misses="<<missAABB+missIPC+missTree<<'\n';
    }
    require(trace.good(),"original CCD event CSV write failed");
    require(global_misses==0,
            "one or more actual cross-object IPC CCD events were culled");
    std::cout<<"PASS18 actual solver accepted fast leaves="<<leaves.size()
             <<" upstream cross-object CCD events="<<total_events
             <<" missed events="<<global_misses
             <<"; independent event enumeration, NOT independent CCD math.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS18 FAILED: "<<e.what()<<'\n';
    return 1;
 }
}

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
    const s::Body original_unaccepted_source=body;
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
    // POSITIVE CCD CONTROL: The solver itself rejected its first
    // 0.75 ms geometric proposal. Capture that exact proposed geometry
    // using the optional source-only inspection parameter; never call it
    // an accepted physical state. Enumerate all *real* official IPC CCD
    // events on this unaccepted motion and require at least one. This
    // prevents "zero missed / zero detected events" from being mistaken
    // for a non-vacuous collision-event coverage experiment.
    std::vector<s::Triangle3> rigid_triangles;
    std::vector<s::TriangleBox> rigid_boxes;
    for(std::size_t i=0;i<rigid.faces.size();++i){
        const auto triangle=s::face_positions(rigid,i);
        rigid_triangles.push_back(triangle);
        rigid_boxes.push_back(s::bounding(triangle));
    }
    s::Body unaccepted_copy=original_unaccepted_source;
    std::vector<s::V3> exact_proposal;
    const auto rejected=s::try_rigid_mesh_fem_full_step(
        unaccepted_copy,rigid,rigid_triangles,rigid_boxes,
        .00075,s::RigidMeshContactSettings{},&exact_proposal);
    require(!rejected.accepted &&
            rejected.was_rejected_by_continuous_geometry,
            "positive CCD control did not use the solver's rejected trial");
    require(unaccepted_copy.time_seconds==original_unaccepted_source.time_seconds &&
            exact_proposal.size()==original_unaccepted_source.nodes.size(),
            "native rejected source trial mutated public clock/geometry");
    s::Body proposed_end=original_unaccepted_source;
    for(std::size_t j=0;j<exact_proposal.size();++j)
        proposed_end.nodes[j].x=exact_proposal[j];
    auto motion=fem::feature_snapshot(
        original_unaccepted_source,proposed_end,rigid);
    motion.topology_epoch=200; // different physical trial/cache epoch
    const auto inp=prepare(motion);
    const auto truth=f::exhaustive(motion).pairs;
    const auto official=upstream_candidates(motion,inp);
    const auto tree=maintained.query(motion).pairs;
    require(truth==tree,"positive rejected-trial tree candidate mismatch");

    std::size_t actual_vf=0,actual_ee=0,missing_truth=0,
                missing_ipc=0,missing_tree=0;
    double first_toi=1.0;
    for(std::size_t vi=0;vi<motion.vertices.size();++vi)
        for(std::size_t fi=0;fi<motion.faces.size();++fi){
            const auto& v=motion.vertices[vi],&face=motion.faces[fi];
            const auto owner=motion.vertices[inp.indices.at(
                face.vertices[0])].object;
            if(v.object==owner)continue;
            ipc::FaceVertexCandidate stencil(
                static_cast<ipc::index_t>(fi),
                static_cast<ipc::index_t>(vi));
            double toi=1.0;
            if(!stencil.ccd(stencil.dof(inp.start,inp.edges,inp.faces),
                            stencil.dof(inp.end,inp.edges,inp.faces),toi))
                continue;
            require(std::isfinite(toi)&&toi>=0&&toi<=1,
                    "invalid rejected-trial upstream VF collision time");
            first_toi=std::min(first_toi,toi);
            ++actual_vf;
            const f::Candidate pair{f::Kind::VertexFace,v.id,face.id};
            missing_truth+=!std::binary_search(truth.begin(),truth.end(),pair);
            missing_ipc+=!std::binary_search(official.begin(),official.end(),pair);
            missing_tree+=!std::binary_search(tree.begin(),tree.end(),pair);
        }
    for(std::size_t ei=0;ei<motion.edges.size();++ei)
        for(std::size_t ej=ei+1;ej<motion.edges.size();++ej){
            const auto& a=motion.edges[ei],&b=motion.edges[ej];
            const auto ownerA=motion.vertices[inp.indices.at(
                a.vertices[0])].object;
            const auto ownerB=motion.vertices[inp.indices.at(
                b.vertices[0])].object;
            if(ownerA==ownerB)continue;
            ipc::EdgeEdgeCandidate stencil(
                static_cast<ipc::index_t>(ei),
                static_cast<ipc::index_t>(ej));
            double toi=1.0;
            if(!stencil.ccd(stencil.dof(inp.start,inp.edges,inp.faces),
                            stencil.dof(inp.end,inp.edges,inp.faces),toi))
                continue;
            require(std::isfinite(toi)&&toi>=0&&toi<=1,
                    "invalid rejected-trial upstream EE collision time");
            first_toi=std::min(first_toi,toi);
            ++actual_ee;
            const f::Candidate pair{f::Kind::EdgeEdge,
                std::min(a.id,b.id),std::max(a.id,b.id)};
            missing_truth+=!std::binary_search(truth.begin(),truth.end(),pair);
            missing_ipc+=!std::binary_search(official.begin(),official.end(),pair);
            missing_tree+=!std::binary_search(tree.begin(),tree.end(),pair);
        }
    require(actual_vf+actual_ee>0,
            "rejected real FEM geometric proposal yielded ZERO upstream CCD events");
    require(missing_truth+missing_ipc+missing_tree==0,
            "positive upstream CCD event missing from one or more candidates");

    std::ofstream positive(dir/"rejected_native_trial_positive_ccd.json");
    positive<<std::setprecision(17)
        <<"{\\n"
        <<" \\"source\\": \\"native_solver_actual_rejected_FEM_proposal\\",\\n"
        <<" \\"is_accepted_physical_motion\\": false,\\n"
        <<" \\"native_full_trial_rejected_by_geometry\\": true,\\n"
        <<" \\"requested_dt_s\\": 0.00075,\\n"
        <<" \\"actual_official_IPC_CCD_vf_events\\": "<<actual_vf<<",\\n"
        <<" \\"actual_official_IPC_CCD_ee_events\\": "<<actual_ee<<",\\n"
        <<" \\"actual_upstream_earliest_toi_fraction\\": "<<first_toi<<",\\n"
        <<" \\"missed_by_exhaustive_AABB\\": "<<missing_truth<<",\\n"
        <<" \\"missed_by_official_LBVH\\": "<<missing_ipc<<",\\n"
        <<" \\"missed_by_maintained_frontier\\": "<<missing_tree<<"\\n"
        <<"}\\n";
    require(positive.good(),"failed to write positive native CCD control");
    std::cout<<"PASS18 POSITIVE rejected native FEM collision proposal: VF="
             <<actual_vf<<" EE="<<actual_ee<<" earliest upstream TOI="
             <<first_toi<<" candidate misses="
             <<missing_truth+missing_ipc+missing_tree<<'\\n';

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

// Kyoto / official IPC Toolkit LBVH, opt-in reference integration.
// MUST be compiled against pinned IPC Toolkit v1.6.1 release; not a shim.
// This validates feature-level *conservative broad-phase* coverage,
// NOT independent continuous collision detection or whole solver physics.
//
// IPC's LBVH internally stores outward-expanded float node bounds.
// Therefore it is valid for IPC to report additional candidates versus
// our double-precision swept-AABB oracle; missing reference pairs fail.
// All masks are deliberately set to universal compatibility, because
// our per-feature masks are not identical to IPC's per-vertex filter.

#include "kyoto/feature_broadphase.hpp"
#include <ipc/broad_phase/lbvh.hpp>
#include <ipc/broad_phase/aabb.hpp>
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <Eigen/Core>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
using kyoto::contact::CubicVertex;

static f::Vertex fixed(std::uint64_t id,Vec3 p) {
    return {id,CubicVertex{{p,p,p,p}}};
}
static f::Vertex moving(std::uint64_t id,std::array<Vec3,4> p) {
    return {id,CubicVertex{p}};
}
static f::Mesh fixture(int seed){
    f::Mesh mesh; mesh.thickness=.03;
    if(seed==0){
        mesh.vertices={
            fixed(11,{0,0,0}),fixed(12,{1,0,0}),
            fixed(13,{0,1,0}),
            moving(14,{{{.25,.25,2},{.25,.25,-2},
                          {.25,.25,-2},{.25,.25,2}}}),
            fixed(15,{0,0,.1}),fixed(16,{1,0,.1}),
            fixed(17,{0,1,.1})
        };
        mesh.faces={{501,{11,12,13}},{502,{15,16,17}}};
        mesh.edges={{701,{11,12}},{702,{12,13}},{703,{13,11}},
                    {704,{15,16}},{705,{16,17}},{706,{17,15}}};
        return mesh;
    }
    std::mt19937_64 rng(21012026+seed);
    std::uniform_real_distribution<double> pos(-2.5,2.5);
    for(std::uint64_t v=0;v<40;v++){
        std::array<Vec3,4> points;
        for(auto& p:points)p={pos(rng),pos(rng),pos(rng)};
        mesh.vertices.push_back(moving(1000+v,points));
    }
    for(std::uint64_t e=0;e<60;e++){
        auto a=std::uint64_t(rng()%40),b=std::uint64_t(rng()%40);
        if(b==a)b=(b+1)%40;
        mesh.edges.push_back({3000+e,{1000+a,1000+b}});
    }
    for(std::uint64_t c=0;c<40;c++){
        auto a=std::uint64_t(rng()%40),b=std::uint64_t(rng()%40),
             d=std::uint64_t(rng()%40);
        if(b==a)b=(b+1)%40;
        if(d==a||d==b)d=(d+1)%40;
        if(d==a||d==b)d=(d+1)%40;
        mesh.faces.push_back({5000+c,{1000+a,1000+b,1000+d}});
    }
    return mesh;
}
static std::vector<f::Candidate> official_ipc_candidates(const f::Mesh& mesh) {
    (void)f::build(mesh); // validate topology, masks and bounds
    for(const auto& v:mesh.vertices)
        if(v.category!=1||v.mask!=~std::uint64_t{0})
            throw std::invalid_argument("IPC adapter requires all-compatible vertex metadata");
    for(const auto& e:mesh.edges)
        if(e.category!=1||e.mask!=~std::uint64_t{0})
            throw std::invalid_argument("IPC adapter requires all-compatible edge metadata");
    for(const auto& fce:mesh.faces)
        if(fce.category!=1||fce.mask!=~std::uint64_t{0})
            throw std::invalid_argument("IPC adapter requires all-compatible face metadata");

    const auto size_limit=std::size_t(std::numeric_limits<int>::max());
    if(mesh.vertices.size()>size_limit||mesh.edges.size()>size_limit||
       mesh.faces.size()>size_limit)
        throw std::overflow_error("IPC mesh index range exceeded");

    std::map<std::uint64_t,int> vertex_to_index;
    ipc::AABBs vertex_boxes;
    vertex_boxes.reserve(mesh.vertices.size());
    for(std::size_t i=0;i<mesh.vertices.size();i++){
        const auto& v=mesh.vertices[i];
        const auto box=f::vertex_box(v.path,mesh.thickness);
        vertex_to_index[v.id]=static_cast<int>(i);
        ipc::AABB b(
            Eigen::Array3d(box.min.x,box.min.y,box.min.z),
            Eigen::Array3d(box.max.x,box.max.y,box.max.z));
        b.vertex_ids={static_cast<ipc::index_t>(i),-1,-1};
        vertex_boxes.push_back(b);
    }
    Eigen::MatrixXi E(static_cast<Eigen::Index>(mesh.edges.size()),2);
    Eigen::MatrixXi F(static_cast<Eigen::Index>(mesh.faces.size()),3);
    for(std::size_t i=0;i<mesh.edges.size();i++)
        for(int j=0;j<2;j++)
            E(static_cast<Eigen::Index>(i),j)=
                vertex_to_index.at(mesh.edges[i].vertices[j]);
    for(std::size_t i=0;i<mesh.faces.size();i++)
        for(int j=0;j<3;j++)
            F(static_cast<Eigen::Index>(i),j)=
                vertex_to_index.at(mesh.faces[i].vertices[j]);

    ipc::LBVH broadphase;
    broadphase.can_vertices_collide=[](auto,auto){return true;};
    broadphase.build(vertex_boxes,E,F,static_cast<std::uint8_t>(3));

    std::vector<ipc::EdgeEdgeCandidate> ee;
    std::vector<ipc::FaceVertexCandidate> fv;
    broadphase.detect_edge_edge_candidates(ee);
    broadphase.detect_face_vertex_candidates(fv);

    std::vector<f::Candidate> output;
    output.reserve(ee.size()+fv.size());
    for(const auto& c:ee){
        const auto a=mesh.edges.at(c.edge0_id).id;
        const auto b=mesh.edges.at(c.edge1_id).id;
        output.push_back({f::Kind::EdgeEdge,std::min(a,b),std::max(a,b)});
    }
    for(const auto& c:fv) {
        output.push_back({f::Kind::VertexFace,
            mesh.vertices.at(c.vertex_id).id,mesh.faces.at(c.face_id).id});
    }
    std::sort(output.begin(),output.end());
    output.erase(std::unique(output.begin(),output.end()),output.end());
    return output;
}
int main(){
    try{
        std::size_t total_reference=0,total_ipc=0,total_extra=0;
        for(int seed=0;seed<13;seed++){
            const auto mesh=fixture(seed);
            const auto truth=f::exhaustive(mesh).pairs;
            const auto sweep=f::sweep_and_prune(mesh).pairs;
            if(truth!=sweep)
                throw std::runtime_error("internal reference parity failed");
            const auto official=official_ipc_candidates(mesh);
            if(!std::includes(official.begin(),official.end(),
                              truth.begin(),truth.end())){
                std::vector<f::Candidate> missing;
                std::set_difference(truth.begin(),truth.end(),
                    official.begin(),official.end(),
                    std::back_inserter(missing));
                std::cerr<<"Official IPC missing "<<missing.size()
                         <<" exact semantic pairs for seed "<<seed<<'\n';
                throw std::runtime_error("IPC LBVH coverage disagreement");
            }
            const auto extra=official.size()-truth.size();
            std::cout<<"seed="<<seed<<" exact_reference="<<truth.size()
                     <<" official_ipc_lbvh="<<official.size()
                     <<" extra_conservative="<<extra<<'\n';
            total_reference+=truth.size();
            total_ipc+=official.size();
            total_extra+=extra;
        }
        std::cout<<"PASS official IPC Toolkit v1.6.1 LBVH: 13 3D scenes, "
                 <<"0 missing reference candidates, reference="
                 <<total_reference<<" IPC="<<total_ipc
                 <<" additional_conservative="<<total_extra<<'\n';
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}

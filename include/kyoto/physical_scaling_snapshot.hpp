#pragma once
// Shared accepted-source geometry for matched CPU and GPU campaigns.
#include "kyoto/connected_self_contact_fixture.hpp"
namespace kyoto::contact::physical_scaling {
namespace s=kyoto::solid;
namespace f=kyoto::contact::features;
inline f::Mesh snapshot(const s::Body& a,const s::Body& b,double padding){
    if(a.surface_faces!=b.surface_faces||a.nodes.size()!=b.nodes.size())throw std::invalid_argument("source changed topology");
    f::Mesh mesh;mesh.thickness=padding;mesh.topology_epoch=1;
    std::set<std::size_t> ids;
    for(const auto& face:a.surface_faces)for(auto v:face)ids.insert(v);
    std::map<std::size_t,std::uint64_t> row;
    for(auto i:ids){
        row.emplace(i,mesh.vertices.size());
        const auto x=a.nodes.at(i).x,y=b.nodes.at(i).x;
        const kyoto::contact::Vec3 p{x.x,x.y,x.z},q{y.x,y.y,y.z};
        mesh.vertices.push_back({row.at(i),kyoto::contact::CubicVertex{{p,p,q,q}}});
    }
    std::set<std::array<std::uint64_t,2>> edges;
    for(const auto& face:a.surface_faces){
        const std::array<std::uint64_t,3> v{row.at(face[0]),row.at(face[1]),row.at(face[2])};
        mesh.faces.push_back({std::uint64_t(mesh.faces.size()),v});
        for(int k=0;k<3;++k)edges.insert({std::min(v[k],v[(k+1)%3]),std::max(v[k],v[(k+1)%3])});
    }
    for(auto e:edges)mesh.edges.push_back({std::uint64_t(mesh.edges.size()),e});
    return mesh;
}
} // namespace kyoto::contact::physical_scaling

#pragma once
// Validate the combinatorial boundary of an oriented tetrahedral complex.
// This is not a geometric embedding or global injectivity certificate.
// The caller supplies positively oriented reference tetrahedron index rows.
#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

namespace kyoto::geometry {
using CellRow=std::array<std::size_t,4>;
using FaceRow=std::array<std::size_t,3>;
using EdgeRow=std::array<std::size_t,2>;
struct BoundaryTopologyReceipt {
    std::size_t cells{},interior_faces{},boundary_faces{},boundary_edges{},boundary_vertices{};
};
namespace topology_detail {
struct Incidence {unsigned count{};int orientation_sum{};};
template<std::size_t N> inline std::array<std::size_t,N> key(std::array<std::size_t,N> row){
    std::sort(row.begin(),row.end());
    if(std::adjacent_find(row.begin(),row.end())!=row.end())
        throw std::invalid_argument("repeated topological node");
    return row;
}
inline int sign(FaceRow row){
    unsigned inversions=0;
    for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)inversions+=row[i]>row[j];
    return inversions%2?-1:1;
}
}
inline BoundaryTopologyReceipt validate_volume_boundary_topology(
    std::size_t node_count,std::span<const CellRow> cells,std::span<const FaceRow> supplied){
    using namespace topology_detail;
    if(node_count==0||cells.empty()||supplied.empty())
        throw std::invalid_argument("empty volume topology");
    std::set<CellRow> unique_cells;
    std::map<FaceRow,Incidence> faces;
    std::vector<bool> used(node_count);
    BoundaryTopologyReceipt r;r.cells=cells.size();
    for(const auto& cell:cells){
        if(!unique_cells.insert(key(cell)).second)throw std::invalid_argument("duplicate tetrahedron");
        for(auto i:cell){if(i>=node_count)throw std::invalid_argument("tetrahedron node out of range");used[i]=true;}
        for(int omit=0;omit<4;++omit){
            FaceRow face{};int k=0;
            for(int i=0;i<4;++i)if(i!=omit)face[k++]=cell[i];
            auto& f=faces[key(face)];
            ++f.count;f.orientation_sum+=(omit%2?-1:1)*sign(face);
            if(f.count>2)throw std::invalid_argument("nonmanifold tetrahedron face");
        }
    }
    if(std::find(used.begin(),used.end(),false)!=used.end())
        throw std::invalid_argument("unused volume node");
    std::set<FaceRow> boundary;
    std::map<EdgeRow,unsigned> boundary_edges;
    // The link of a boundary vertex must be a single cycle. Checking
    // edge degree alone misses two closed shells pinched at a vertex.
    std::map<std::size_t,std::map<std::size_t,std::set<std::size_t>>> links;
    for(const auto& [face,incidence]:faces){
        if(incidence.count==2){
            if(incidence.orientation_sum!=0)throw std::invalid_argument("inconsistent interior face orientation");
            ++r.interior_faces;continue;
        }
        boundary.insert(face);
        for(int i=0;i<3;++i){
            const auto v=face[i],a=face[(i+1)%3],b=face[(i+2)%3];
            ++boundary_edges[key(EdgeRow{v,a})];
            links[v][a].insert(b);links[v][b].insert(a);
        }
    }
    for(const auto& [edge,count]:boundary_edges)
        if(count!=2)throw std::invalid_argument("open or nonmanifold boundary edge");
    for(const auto& [vertex,link]:links){
        for(const auto& [neighbor,adjacent]:link)
            if(adjacent.size()!=2)throw std::invalid_argument("nonmanifold boundary vertex degree");
        std::set<std::size_t> reached;
        std::vector<std::size_t> pending{link.begin()->first};
        while(!pending.empty()){
            auto x=pending.back();pending.pop_back();
            if(!reached.insert(x).second)continue;
            for(auto n:link.at(x))pending.push_back(n);
        }
        if(reached.size()!=link.size())throw std::invalid_argument("disconnected boundary vertex link");
    }
    std::set<FaceRow> provided;
    for(auto face:supplied){
        for(auto i:face)if(i>=node_count)throw std::invalid_argument("boundary node out of range");
        if(!provided.insert(key(face)).second)throw std::invalid_argument("duplicate supplied boundary face");
    }
    if(provided!=boundary)throw std::invalid_argument("supplied boundary differs from tetrahedron incidence");
    r.boundary_faces=boundary.size();r.boundary_edges=boundary_edges.size();r.boundary_vertices=links.size();
    return r;
}
} // namespace kyoto::geometry

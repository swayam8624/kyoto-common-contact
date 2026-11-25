#pragma once
// Filter conservative external candidate coverage back to the common
// binary64 surface contract. Never manufacture a missing candidate.
// The caller must compare the resulting canonical set with exhaustive
// truth before allowing a foreign producer to affect the native solver.
#include "kyoto/feature_broadphase.hpp"
namespace kyoto::contact::features {
inline Result filter_dense_external_candidates(const Built& mesh,const std::vector<Candidate>& candidates){
    auto require_dense=[](const auto& rows){
        for(std::size_t i=0;i<rows.size();++i)
            if(rows[i].id!=i)throw std::invalid_argument("external candidate filter requires dense IDs");
    };
    require_dense(mesh.vertices);require_dense(mesh.edges);require_dense(mesh.faces);
    Result out;
    for(auto p:candidates){
        if(p.kind==Kind::VertexFace){
            if(p.first>=mesh.vertices.size()||p.second>=mesh.faces.size())
                throw std::invalid_argument("external VF candidate index out of range");
            if(valid_pair(mesh.vertices[p.first],mesh.faces[p.second],out))out.pairs.push_back(p);
        }else if(p.kind==Kind::EdgeEdge){
            if(p.first>=mesh.edges.size()||p.second>=mesh.edges.size())
                throw std::invalid_argument("external EE candidate index out of range");
            if(p.first==p.second){++out.incident_rejections;continue;}
            if(p.first>p.second)std::swap(p.first,p.second);
            if(valid_pair(mesh.edges[p.first],mesh.edges[p.second],out))out.pairs.push_back(p);
        }else throw std::invalid_argument("external candidate kind invalid");
    }
    normalize(out);
    return out;
}
} // namespace kyoto::contact::features

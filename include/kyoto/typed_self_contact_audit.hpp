#pragma once
// Pass 32B: typed nonincident VF/EE self-contact audit. Unlike the
// face-pair filter, this retains nonincident stencils from two faces
// sharing vertices. It is an audit, not a feature-level response law.
// Surface features are compacted from connectivity (no interior nodes).
// Pass 27 produces dense row IDs; each admitted linear stencil is
// certified by Pass 31A using degenerate triangles representing the
// exact point or segment convex sets. No positive clearance is imposed
// between a vertex and its incident face or edges sharing a vertex.
#include "kyoto/linear_candidate_adapter.hpp"
#include "kyoto/rounded_relative_triangle_certificate.hpp"
#include <map>
#include <set>
namespace kyoto::solid {
struct TypedSelfAudit {
    bool separated{true};
    std::size_t vertices{},edges{},faces{},candidates{},certificates{},projections{};
    kyoto::contact::features::Candidate unresolved{};
};
class TypedSelfAuditor {
    kyoto::contact::atlas::LinearCandidateAdapter adapter_;
public:
    TypedSelfAudit audit(const Body& a,const Body& b,double skin,unsigned depth=12){
        namespace f=kyoto::contact::features;
        namespace at=kyoto::contact::atlas;
        using kyoto::contact::Vec3;
        if(a.nodes.size()!=b.nodes.size()||a.surface_faces!=b.surface_faces||
           !std::isfinite(skin)||skin<0.||depth>20||!rounded::prerequisites())
            throw std::invalid_argument("invalid typed self audit");
        std::set<std::size_t> ids;
        for(const auto& face:a.surface_faces)for(auto i:face){
            if(i>=a.nodes.size())throw std::invalid_argument("invalid surface ID");
            ids.insert(i);
        }
        std::vector<std::size_t> original(ids.begin(),ids.end());
        std::map<std::size_t,std::size_t> dense;
        std::vector<Vec3> start,end;
        for(auto i:original){
            dense.emplace(i,start.size());
            const auto p=a.nodes[i].x,q=b.nodes[i].x;
            start.push_back({p.x,p.y,p.z});end.push_back({q.x,q.y,q.z});
        }
        std::vector<at::IndexedFace> faces;
        std::set<at::IndexedEdge> unique_edges;
        for(const auto& face:a.surface_faces){
            const at::IndexedFace row{dense.at(face[0]),dense.at(face[1]),dense.at(face[2])};
            faces.push_back(row);
            for(int k=0;k<3;++k){
                const auto x=row[k],y=row[(k+1)%3];
                unique_edges.insert({std::min(x,y),std::max(x,y)});
            }
        }
        const std::vector<at::IndexedEdge> edges(unique_edges.begin(),unique_edges.end());
        const auto pairs=adapter_.query(start,end,edges,faces,1,skin).pairs;
        TypedSelfAudit out;
        out.vertices=start.size();out.edges=edges.size();out.faces=faces.size();
        out.candidates=pairs.size();
        auto path=[&](at::IndexedFace row){
            LinearTriangleMotion m;
            for(int k=0;k<3;++k){
                m.start[k]=a.nodes[original.at(row[k])].x;
                m.end[k]=b.nodes[original.at(row[k])].x;
            }
            return m;
        };
        for(const auto& p:pairs){
            LinearTriangleMotion x,y;
            if(p.kind==f::Kind::VertexFace){
                const auto i=std::size_t(p.first);
                x=path({i,i,i});y=path(faces.at(p.second));
            }else{
                const auto e=edges.at(p.first),g=edges.at(p.second);
                x=path({e[0],e[1],e[1]});y=path({g[0],g[1],g[1]});
            }
            const auto c=certify_rounded_relative_triangle_pair(x,y,skin,depth);
            ++out.certificates;out.projections+=c.interval_projections;
            if(!c.entirely_separated){out.separated=false;out.unresolved=p;return out;}
        }
        return out;
    }
};
} // namespace kyoto::solid

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
#include "kyoto/nonincident_self_contact.hpp"
#include <map>
#include <set>
namespace kyoto::solid {
struct TypedSelfAudit {
    bool separated{true};
    std::size_t vertices{},edges{},faces{},candidates{},certificates{},projections{},
                box_tests{},incident_rejections{},interval_nodes{};
    kyoto::contact::features::Candidate unresolved{};
};
struct TypedSelfStencil {
    std::array<std::size_t,3> left{},right{};
    kyoto::contact::features::Kind kind{};
};
struct TypedSelfCandidates {
    TypedSelfAudit info;
    std::vector<TypedSelfStencil> stencils;
    std::vector<kyoto::contact::features::Candidate> keys;
    std::size_t box_tests{},incident_rejections{};
};
class TypedSelfAuditor {
    kyoto::contact::atlas::LinearCandidateAdapter adapter_;
    kyoto::contact::atlas::ExternalCandidateProducer external_{};
    // Cache only topology-derived rows, never geometry or safety receipts.
    // Full face order and original node count are checked on every query.
    std::vector<SelfFace> source_faces_;
    std::vector<std::size_t> original_;
    std::vector<kyoto::contact::atlas::IndexedFace> faces_;
    std::vector<kyoto::contact::atlas::IndexedEdge> edges_;
    std::size_t source_nodes_{},mapping_rebuilds_{};
    std::uint64_t epoch_{};
    void prepare_topology(const Body& a){
        namespace at=kyoto::contact::atlas;
        if(epoch_&&a.nodes.size()==source_nodes_&&a.surface_faces==source_faces_)return;
        std::set<std::size_t> ids;
        for(const auto& face:a.surface_faces){
            if(face[0]==face[1]||face[1]==face[2]||face[0]==face[2])
                throw std::invalid_argument("degenerate typed face");
            for(auto i:face){
                if(i>=a.nodes.size())throw std::invalid_argument("invalid surface ID");
                ids.insert(i);
            }
        }
        std::vector<std::size_t> original(ids.begin(),ids.end());
        std::map<std::size_t,std::size_t> dense;
        for(std::size_t i=0;i<original.size();++i)dense.emplace(original[i],i);
        std::vector<at::IndexedFace> faces;
        std::set<at::IndexedEdge> unique_edges;
        for(const auto& face:a.surface_faces){
            const at::IndexedFace row{dense.at(face[0]),dense.at(face[1]),dense.at(face[2])};
            faces.push_back(row);
            for(int k=0;k<3;++k){
                auto x=row[k],y=row[(k+1)%3];
                unique_edges.insert({std::min(x,y),std::max(x,y)});
            }
        }
        std::vector<at::IndexedEdge> edges(unique_edges.begin(),unique_edges.end());
        auto source=a.surface_faces;
        if(epoch_==std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("typed topology epoch exhausted");
        source_faces_.swap(source);original_.swap(original);
        faces_.swap(faces);edges_.swap(edges);
        source_nodes_=a.nodes.size();++epoch_;++mapping_rebuilds_;
    }
public:
    explicit TypedSelfAuditor(kyoto::contact::atlas::ExternalCandidateProducer external=nullptr,bool cached_sap=false):adapter_(cached_sap),external_(external){}
    std::size_t mapping_rebuilds()const{return mapping_rebuilds_;}
    std::size_t hierarchy_rebuilds()const{return adapter_.rebuilds();}
    std::size_t hierarchy_refits()const{return adapter_.refits();}
    TypedSelfCandidates candidates(const Body& a,const Body& b,double skin,bool exhaustive=false){
        namespace f=kyoto::contact::features;
        namespace at=kyoto::contact::atlas;
        using kyoto::contact::Vec3;
        if(a.nodes.size()!=b.nodes.size()||a.surface_faces!=b.surface_faces||
           !std::isfinite(skin)||skin<0.||!rounded::prerequisites())
            throw std::invalid_argument("invalid typed self audit");
        prepare_topology(a);
        const auto& original=original_;const auto& faces=faces_;const auto& edges=edges_;
        std::vector<Vec3> start,end;
        start.reserve(original.size());end.reserve(original.size());
        for(auto i:original){
            const auto p=a.nodes[i].x,q=b.nodes[i].x;
            start.push_back({p.x,p.y,p.z});end.push_back({q.x,q.y,q.z});
        }
        const auto result=adapter_.query(start,end,edges,faces,epoch_,skin,exhaustive,external_);
        TypedSelfCandidates out;
        out.info.vertices=start.size();out.info.edges=edges.size();out.info.faces=faces.size();
        out.info.candidates=result.pairs.size();
        out.box_tests=result.candidate_aabb_tests;out.incident_rejections=result.incident_rejections;
        out.keys=result.pairs;
        auto global=[&](at::IndexedFace row){
            for(auto& k:row)k=original.at(k);
            return row;
        };
        for(const auto& p:result.pairs){
            if(p.kind==f::Kind::VertexFace){
                const auto i=std::size_t(p.first);
                out.stencils.push_back({global({i,i,i}),global(faces.at(p.second)),p.kind});
            }else{
                const auto e=edges.at(p.first),g=edges.at(p.second);
                out.stencils.push_back({global({e[0],e[1],e[1]}),
                                        global({g[0],g[1],g[1]}),p.kind});
            }
        }
        return out;
    }
    TypedSelfAudit audit(const Body& a,const Body& b,double skin,unsigned depth=12,
                         bool exhaustive=false,bool fast_projection=false){
        if(depth>20)throw std::invalid_argument("invalid typed self depth");
        const auto set=candidates(a,b,skin,exhaustive);
        auto out=set.info;
        out.box_tests=set.box_tests;out.incident_rejections=set.incident_rejections;
        auto path=[&](const auto& row){
            return LinearTriangleMotion{self_positions(a,row),self_positions(b,row)};
        };
        for(std::size_t i=0;i<set.stencils.size();++i){
            const auto& s=set.stencils[i];
            const auto c=certify_rounded_relative_triangle_pair(
                path(s.left),path(s.right),skin,depth,fast_projection);
            ++out.certificates;out.projections+=c.interval_projections;out.interval_nodes+=c.nodes;
            if(!c.entirely_separated){out.separated=false;out.unresolved=set.keys[i];return out;}
        }
        return out;
    }
};
} // namespace kyoto::solid

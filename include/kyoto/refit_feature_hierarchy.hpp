#pragma once
// Pass 14: maintained, refittable binary AABB hierarchies for typed
// moving 3D mesh features. Vertex-face uses two-tree traversal;
// edge-edge uses one-tree self traversal with an i<j leaf rule.
// All feature boxes are rebuilt each frame; tree node bounding volumes
// refit bottom-up. Topology/signature changes rebuild the hierarchy.
// Broadphase candidates are NOT CCD events. No speedup is claimed.
#include "kyoto/cached_feature_boxes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::contact::atlas {
namespace f=kyoto::contact::features;

class RefitHierarchy {
    struct Node {
        Aabb bounds{};
        int left{-1},right{-1};
        std::size_t record{};
        bool leaf()const{return left<0;}
    };
    struct Tree {
        std::vector<Node> nodes;
        std::vector<std::size_t> indices;
        int root{-1};

        static double centroid(const Aabb& a,int axis){
            if(axis==0)return a.min.x/2+a.max.x/2;
            if(axis==1)return a.min.y/2+a.max.y/2;
            return a.min.z/2+a.max.z/2;
        }
        static int longest(const Aabb& a){
            const double dx=a.max.x-a.min.x,dy=a.max.y-a.min.y,
                         dz=a.max.z-a.min.z;
            return dx>=dy&&dx>=dz?0:(dy>=dz?1:2);
        }
        int construct(const std::vector<f::Record>& records,
                      std::size_t first,std::size_t end){
            if(first>=end)throw std::logic_error("empty BVH node");
            const int here=static_cast<int>(nodes.size());
            nodes.emplace_back();
            if(end-first==1) {
                nodes[here].record=indices[first];
                nodes[here].bounds=records[indices[first]].bounds;
                return here;
            }
            Aabb bound=records[indices[first]].bounds;
            for(std::size_t j=first+1;j<end;++j)
                bound=f::unite(bound,records[indices[j]].bounds);
            const int axis=longest(bound);
            const auto mid=first+(end-first)/2;
            std::nth_element(indices.begin()+first,indices.begin()+mid,
                indices.begin()+end,[&](std::size_t a,std::size_t b){
                    const auto ca=centroid(records[a].bounds,axis);
                    const auto cb=centroid(records[b].bounds,axis);
                    return ca==cb?records[a].id<records[b].id:ca<cb;
                });
            const int l=construct(records,first,mid);
            const int r=construct(records,mid,end);
            nodes[here].left=l;
            nodes[here].right=r;
            nodes[here].bounds=f::unite(nodes[l].bounds,nodes[r].bounds);
            return here;
        }
        void build(const std::vector<f::Record>& records){
            nodes.clear();
            indices.resize(records.size());
            std::iota(indices.begin(),indices.end(),std::size_t{0});
            if(records.empty()){root=-1;return;}
            nodes.reserve(2*records.size()-1);
            root=construct(records,0,records.size());
        }
        Aabb refit_at(int at,const std::vector<f::Record>& records) {
            Node& n=nodes[at];
            if(n.leaf())return n.bounds=records[n.record].bounds;
            const auto a=refit_at(n.left,records);
            const auto b=refit_at(n.right,records);
            return n.bounds=f::unite(a,b);
        }
        void refit(const std::vector<f::Record>& records){
            if(root>=0)refit_at(root,records);
        }
    };

    Tree vertices_,faces_,edges_;
    CachedFeatureBoxes swept_boxes_;
    std::vector<std::uint64_t> key_;
    bool initialized_{};
    std::size_t rebuilds_{},refits_{};

    static std::vector<std::uint64_t> signature(const f::Mesh& m){
        std::vector<std::uint64_t> s;
        s.reserve(4+4*m.vertices.size()+5*m.edges.size()+6*m.faces.size());
        s.insert(s.end(),{m.topology_epoch,
            std::uint64_t(m.vertices.size()),std::uint64_t(m.edges.size()),
            std::uint64_t(m.faces.size())});
        for(const auto& v:m.vertices)
            s.insert(s.end(),{v.id,v.object,v.category,v.mask});
        for(const auto& e:m.edges)
            s.insert(s.end(),{e.id,e.vertices[0],e.vertices[1],e.category,e.mask});
        for(const auto& face:m.faces)
            s.insert(s.end(),{face.id,face.vertices[0],face.vertices[1],
                              face.vertices[2],face.category,face.mask});
        return s;
    }

    static void cross(const Tree& a,int ai,const Tree& b,int bi,
                      const std::vector<f::Record>& va,
                      const std::vector<f::Record>& fb,f::Result& out){
        const auto& x=a.nodes[ai],&y=b.nodes[bi];
        if(!overlaps(x.bounds,y.bounds))return;
        if(x.leaf()&&y.leaf()) {
            const auto& v=va[x.record];
            const auto& face=fb[y.record];
            if(f::valid_pair(v,face,out))
                out.pairs.push_back({f::Kind::VertexFace,v.id,face.id});
            return;
        }
        if(x.leaf()) {
            cross(a,ai,b,y.left,va,fb,out);
            cross(a,ai,b,y.right,va,fb,out);
        } else if(y.leaf()){
            cross(a,x.left,b,bi,va,fb,out);
            cross(a,x.right,b,bi,va,fb,out);
        } else {
            cross(a,x.left,b,y.left,va,fb,out);
            cross(a,x.left,b,y.right,va,fb,out);
            cross(a,x.right,b,y.left,va,fb,out);
            cross(a,x.right,b,y.right,va,fb,out);
        }
    }
    static void self(const Tree& tree,int ai,int bi,
                     const std::vector<f::Record>& records,
                     f::Result& out){
        const auto& x=tree.nodes[ai],&y=tree.nodes[bi];
        if(!overlaps(x.bounds,y.bounds))return;
        if(ai==bi){
            if(x.leaf())return;
            self(tree,x.left,x.left,records,out);
            self(tree,x.left,x.right,records,out);
            self(tree,x.right,x.right,records,out);
            return;
        }
        if(x.leaf()&&y.leaf()){
            const auto& a=records[x.record],&b=records[y.record];
            if(f::valid_pair(a,b,out))
                out.pairs.push_back({f::Kind::EdgeEdge,
                    std::min(a.id,b.id),std::max(a.id,b.id)});
            return;
        }
        if(x.leaf()){
            self(tree,ai,y.left,records,out);
            self(tree,ai,y.right,records,out);
        } else {
            self(tree,x.left,bi,records,out);
            self(tree,x.right,bi,records,out);
        }
    }
public:
    std::size_t rebuilds()const{return rebuilds_;}
    std::size_t refits()const{return refits_;}

    f::Result query(const f::Mesh& mesh) {
        // Include all source mesh validation, box construction and
        // topology signature work in the benchmark timer.
        const auto next=signature(mesh);
        const bool topology_changed=(!initialized_||next!=key_);
        // Both methods pay current signature validation and fresh swept
        // conservative boxes. Warm calls reuse validated vertex indices.
        const auto& built=swept_boxes_.update(mesh,topology_changed);
        if(topology_changed){
            vertices_.build(built.vertices);
            edges_.build(built.edges);
            faces_.build(built.faces);
            key_=next;initialized_=true;++rebuilds_;
        }else{
            vertices_.refit(built.vertices);
            edges_.refit(built.edges);
            faces_.refit(built.faces);
            ++refits_;
        }
        f::Result out;
        if(vertices_.root>=0&&faces_.root>=0)
            cross(vertices_,vertices_.root,faces_,faces_.root,
                  built.vertices,built.faces,out);
        if(edges_.root>=0)
            self(edges_,edges_.root,edges_.root,built.edges,out);
        f::normalize(out);
        return out;
    }
};
} // namespace kyoto::contact::atlas

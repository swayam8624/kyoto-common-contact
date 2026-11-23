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
    // Persistent *pair frontier*: a disjoint cut of the BVH product.
    // Each stored node pair covers its Cartesian leaf product, which is
    // either certified separated at this frame or a single leaf pair.
    // Re-testing and refining the cut on every future frame cannot miss
    // newly overlapping pairs, even under arbitrarily large motion.
    struct NodePair{int first{},second{};};
    std::vector<NodePair> vf_cut_,ee_cut_;
    bool maintain_pair_cut_{false},pair_cut_ready_{};
    std::size_t pair_cut_skipped_frames_{};
    std::size_t last_pair_cut_attempted_size_{};
    std::size_t pair_cut_fallbacks_{},pair_cut_reuses_{};
    bool retain_feature_lookup_{true};
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
    // A bounded cut partitions the entire product of two trees. A
    // coarse retained node-pair is NOT a rejection certificate: on each
    // new frame it must either be shown separated, refined, or traversed
    // completely using the ordinary safe traversal for that region.
    //
    // Crucially the budget limits *construction*, rather than building a
    // massive leaf-pair cut and throwing it away after paying its cost.
    // The stack plus the finalized cut never exceeds the given budget.
    static void bounded_vf(
        const Tree& a,const Tree& b,
        const std::vector<f::Record>& vertices,
        const std::vector<f::Record>& faces,
        const std::vector<NodePair>& previous,
        std::vector<NodePair>& next,
        f::Result& out,std::size_t budget,std::size_t& coarse_regions)
    {
        std::vector<NodePair> stack=previous;
        while(!stack.empty()) {
            const NodePair p=stack.back();stack.pop_back();
            const auto& x=a.nodes[p.first],&y=b.nodes[p.second];
            if(!overlaps(x.bounds,y.bounds)){
                next.push_back(p);
                continue;
            }
            if(x.leaf()&&y.leaf()) {
                next.push_back(p);
                const auto& v=vertices[x.record],&face=faces[y.record];
                if(f::valid_pair(v,face,out))
                    out.pairs.push_back({f::Kind::VertexFace,v.id,face.id});
                continue;
            }
            // The split is an exact partition of the parent's product.
            // Do not split if preserving all remaining work would exceed
            // the memory bound; the parent is then *fully* queried.
            const std::size_t children=x.leaf()||y.leaf()?2:4;
            if(next.size()+stack.size()+children>budget) {
                next.push_back(p);
                cross(a,p.first,b,p.second,vertices,faces,out);
                ++coarse_regions;
                continue;
            }
            if(x.leaf()){
                stack.push_back({p.first,y.left});
                stack.push_back({p.first,y.right});
            }else if(y.leaf()){
                stack.push_back({x.left,p.second});
                stack.push_back({x.right,p.second});
            }else{
                stack.push_back({x.left,y.left});
                stack.push_back({x.left,y.right});
                stack.push_back({x.right,y.left});
                stack.push_back({x.right,y.right});
            }
        }
    }

    // For the unordered edge-edge self-product, the diagonal node
    // partitions into LL, LR, RR. Each off-diagonal pair splits into
    // disjoint child products. No pair is ever skipped on account of
    // cache state, and a diagonal *leaf* has no admissible self-pair.
    static void bounded_ee(
        const Tree& tree,const std::vector<f::Record>& edges,
        const std::vector<NodePair>& previous,
        std::vector<NodePair>& next,
        f::Result& out,std::size_t budget,std::size_t& coarse_regions)
    {
        std::vector<NodePair> stack=previous;
        while(!stack.empty()){
            const NodePair p=stack.back();stack.pop_back();
            const auto& x=tree.nodes[p.first],&y=tree.nodes[p.second];
            if(p.first==p.second){
                if(x.leaf())continue;
                if(next.size()+stack.size()+3>budget){
                    next.push_back(p);
                    self(tree,p.first,p.second,edges,out);
                    ++coarse_regions;
                }else{
                    stack.push_back({x.left,x.left});
                    stack.push_back({x.left,x.right});
                    stack.push_back({x.right,x.right});
                }
                continue;
            }
            if(!overlaps(x.bounds,y.bounds)){
                next.push_back(p);
                continue;
            }
            if(x.leaf()&&y.leaf()){
                next.push_back(p);
                const auto& a=edges[x.record],&b=edges[y.record];
                if(f::valid_pair(a,b,out))
                    out.pairs.push_back({f::Kind::EdgeEdge,
                        std::min(a.id,b.id),std::max(a.id,b.id)});
                continue;
            }
            if(next.size()+stack.size()+2>budget){
                next.push_back(p);
                self(tree,p.first,p.second,edges,out);
                ++coarse_regions;
                continue;
            }
            if(x.leaf()){
                stack.push_back({p.first,y.left});
                stack.push_back({p.first,y.right});
            }else{
                stack.push_back({x.left,p.second});
                stack.push_back({x.right,p.second});
            }
        }
    }
public:
    explicit RefitHierarchy(bool retain_feature_lookup=true,
                            bool maintain_pair_cut=false)
        :retain_feature_lookup_(retain_feature_lookup),
         maintain_pair_cut_(maintain_pair_cut){}
    std::size_t rebuilds()const{return rebuilds_;}
    std::size_t refits()const{return refits_;}
    std::size_t pair_cut_reuses()const{return pair_cut_reuses_;}
    // Count coarse region traversals, not discarded full-size cuts.
    std::size_t pair_cut_fallbacks()const{return pair_cut_fallbacks_;}
    std::size_t pair_cut_size()const{return vf_cut_.size()+ee_cut_.size();}
    // Historical metric name: frames with any coarse region traversed.
    std::size_t pair_cut_skipped_frames()const{return pair_cut_skipped_frames_;}
    std::size_t last_pair_cut_attempted_size()const{
        return last_pair_cut_attempted_size_;
    }

    f::Result query(const f::Mesh& mesh) {
        // Include all source mesh validation, box construction and
        // topology signature work in the benchmark timer.
        const auto next=signature(mesh);
        const bool topology_changed=(!initialized_||next!=key_);
        // Both methods pay current signature validation and fresh swept
        // conservative boxes. Warm calls reuse validated vertex indices.
        f::Built uncached;
        const f::Built* source=nullptr;
        if(retain_feature_lookup_) {
            source=&swept_boxes_.update(mesh,topology_changed);
        }else {
            // A/B control: same spatial tree and same candidate traversal,
            // but fully pay f::build() every frame as in the old method.
            uncached=f::build(mesh);
            source=&uncached;
        }
        const auto& built=*source;
        if(topology_changed){
            vertices_.build(built.vertices);
            edges_.build(built.edges);
            faces_.build(built.faces);
            key_=next;initialized_=true;++rebuilds_;
            pair_cut_ready_=false;vf_cut_.clear();ee_cut_.clear();
        }else{
            vertices_.refit(built.vertices);
            edges_.refit(built.edges);
            faces_.refit(built.faces);
            ++refits_;
        }
        f::Result out;
        if(maintain_pair_cut_){
            std::vector<NodePair> next_vf,next_ee;
            std::size_t coarse_regions=0;
            // Two independent cuts have bounded memory even on dense
            // AABB workloads. Budget is fully charged in query() timings.
            const std::size_t max_cut=std::max(std::size_t{64},
                std::size_t{4}*(built.vertices.size()+
                    built.edges.size()+built.faces.size()));
            const std::size_t vf_budget=max_cut/2;
            const std::size_t ee_budget=max_cut-vf_budget;
            if(pair_cut_ready_){
                pair_cut_reuses_+=vf_cut_.size()+ee_cut_.size();
            }
            if(vertices_.root>=0&&faces_.root>=0){
                const std::vector<NodePair> root{{vertices_.root,faces_.root}};
                bounded_vf(vertices_,faces_,built.vertices,built.faces,
                           pair_cut_ready_?vf_cut_:root,next_vf,out,
                           vf_budget,coarse_regions);
            }
            if(edges_.root>=0){
                const std::vector<NodePair> root{{edges_.root,edges_.root}};
                bounded_ee(edges_,built.edges,
                           pair_cut_ready_?ee_cut_:root,next_ee,out,
                           ee_budget,coarse_regions);
            }
            if(coarse_regions){
                pair_cut_fallbacks_+=coarse_regions;
                ++pair_cut_skipped_frames_; // Frames with coarse traversal
            }
            last_pair_cut_attempted_size_=next_vf.size()+next_ee.size();
            if(next_vf.size()>vf_budget||next_ee.size()>ee_budget)
                throw std::logic_error("bounded frontier violated memory cap");
            vf_cut_.swap(next_vf);
            ee_cut_.swap(next_ee);
            pair_cut_ready_=true;
        }else{
            if(vertices_.root>=0&&faces_.root>=0)
                cross(vertices_,vertices_.root,faces_,faces_.root,
                      built.vertices,built.faces,out);
            if(edges_.root>=0)
                self(edges_,edges_.root,edges_.root,built.edges,out);
        }
        f::normalize(out);
        return out;
    }
};
} // namespace kyoto::contact::atlas

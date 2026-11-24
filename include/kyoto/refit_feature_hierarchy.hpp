#pragma once
// Pass 14: maintained, refittable binary AABB hierarchies for typed
// moving 3D mesh features. Vertex-face uses two-tree traversal;
// edge-edge uses one-tree self traversal with an i<j leaf rule.
// All feature boxes are rebuilt each frame; tree node bounding volumes
// refit bottom-up. Topology/signature changes rebuild the hierarchy.
// Broadphase candidates are NOT CCD events. No speedup is claimed.
#include "kyoto/cached_feature_boxes.hpp"
#include "kyoto/selective_feature_boxes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::contact::atlas {
namespace f=kyoto::contact::features;

class RefitHierarchy {
public:
    // Optional paid-stage measurement. Supplying a non-null pointer
    // instruments only this query; normal consumers remain unprofiled.
    // The five consecutive stage intervals SUM exactly to total_ns.
    // These are native C++ wall-clock durations, not solver-step costs.
    struct QueryStages {
        std::int64_t topology_signature_ns{};
        std::int64_t swept_feature_boxes_ns{};
        std::int64_t cold_build_or_warm_refit_ns{};
        std::int64_t vf_ee_traversal_ns{};
        std::int64_t candidate_normalize_ns{};
        std::int64_t total_ns{};
        bool topology_cold{};
        std::size_t dirty_vertices{},candidate_count{};
    };
private:
    struct Node {
        Aabb bounds{};
        int left{-1},right{-1},parent{-1};
        std::size_t record{};
        bool leaf()const{return left<0;}
    };
    struct Tree {
        std::vector<Node> nodes;
        std::vector<std::size_t> indices;
        std::vector<int> leaf_of_record;
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
                leaf_of_record[indices[first]]=here;
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
            nodes[l].parent=here;
            nodes[r].parent=here;
            nodes[here].bounds=f::unite(nodes[l].bounds,nodes[r].bounds);
            return here;
        }
        void build(const std::vector<f::Record>& records){
            nodes.clear();
            leaf_of_record.assign(records.size(),-1);
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
        // Nodes are stored in depth-first preorder: a parent index is
        // smaller than both children. Updating affected ancestors in
        // DESCENDING index order therefore always sees fully updated
        // descendants. Never reuse a topology-incompatible leaf index.
        void refit_changed(const std::vector<f::Record>& records,
                           const std::vector<std::size_t>& dirty) {
            if(root<0||dirty.empty())return;
            if(dirty.size()*4>=records.size()){
                refit(records);
                return;
            }
            std::vector<int> ancestors;
            for(const auto index:dirty) {
                if(index>=leaf_of_record.size())
                    throw std::out_of_range("selective tree record index");
                int at=leaf_of_record[index];
                if(at<0)throw std::logic_error("missing selective BVH leaf");
                nodes[at].bounds=records[index].bounds;
                while(nodes[at].parent>=0){
                    at=nodes[at].parent;
                    ancestors.push_back(at);
                }
            }
            std::sort(ancestors.begin(),ancestors.end(),
                [](int a,int b){return a>b;});
            ancestors.erase(std::unique(ancestors.begin(),ancestors.end()),
                            ancestors.end());
            for(const auto at:ancestors){
                const auto& n=nodes[at];
                if(n.leaf())throw std::logic_error(
                    "selective ancestor unexpectedly leaf");
                nodes[at].bounds=f::unite(
                    nodes[n.left].bounds,nodes[n.right].bounds);
            }
        }
    };

    Tree vertices_,faces_,edges_;
    CachedFeatureBoxes swept_boxes_;
    SelectiveFeatureBoxes selective_boxes_;
    bool selective_motion_{false};
    std::size_t partial_refits_{},unchanged_refits_{};
    // Persistent *pair frontier*: a disjoint cut of the BVH product.
    // Each stored node pair covers its Cartesian leaf product, which is
    // either certified separated at this frame or a single leaf pair.
    // Re-testing and refining the cut on every future frame cannot miss
    // newly overlapping pairs, even under arbitrarily large motion.
    struct NodePair{int first{},second{};};
    std::vector<NodePair> vf_cut_,ee_cut_;
    bool maintain_pair_cut_{false},pair_cut_ready_{};
    bool compact_separated_vf_{false};
    std::size_t frontier_budget_factor_{4};
    std::size_t compacted_vf_merges_{};
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
    // Proof sketch: disjoint cuts partition V x F products. Two items
    // (leftChild,b) and (rightChild,b) are a complete partition of
    // (parent,b). If the current parent/b AABBs are disjoint, the union
    // contains no candidate. Replacing BOTH by (parent,b) is exact.
    // The symmetric argument holds on the face tree side. A future
    // frame always rechecks and subdivides the retained parent pair.
    static void compact_vf_cut(const Tree& vertices,const Tree& faces,
                               std::vector<NodePair>& cut,
                               std::size_t& merge_counter){
        std::set<std::pair<int,int>> items;
        for(const auto& p:cut)items.emplace(p.first,p.second);
        bool changed=true;
        while(changed){
            changed=false;
            for(const auto& item:items){
                const int a=item.first,b=item.second;
                const int pa=vertices.nodes[a].parent;
                if(pa>=0&&!overlaps(vertices.nodes[pa].bounds,
                                    faces.nodes[b].bounds)){
                    const auto& p=vertices.nodes[pa];
                    const int sibling=(p.left==a)?p.right:p.left;
                    if(items.contains({sibling,b})){
                        items.erase({a,b});
                        items.erase({sibling,b});
                        items.emplace(pa,b);
                        ++merge_counter;changed=true;break;
                    }
                }
                const int pb=faces.nodes[b].parent;
                if(pb>=0&&!overlaps(vertices.nodes[a].bounds,
                                    faces.nodes[pb].bounds)){
                    const auto& p=faces.nodes[pb];
                    const int sibling=(p.left==b)?p.right:p.left;
                    if(items.contains({a,sibling})){
                        items.erase({a,b});
                        items.erase({a,sibling});
                        items.emplace(a,pb);
                        ++merge_counter;changed=true;break;
                    }
                }
            }
        }
        cut.clear();cut.reserve(items.size());
        for(auto [a,b]:items)cut.push_back({a,b});
    }
public:
    explicit RefitHierarchy(bool retain_feature_lookup=true,
                            bool maintain_pair_cut=false,
                            bool compact_separated_vf=false,
                            std::size_t frontier_budget_factor=4,
                            bool selective_motion=false,
                            bool dense_motion_fast_path=false)
        :selective_boxes_(dense_motion_fast_path),
         retain_feature_lookup_(retain_feature_lookup),
         maintain_pair_cut_(maintain_pair_cut),
         compact_separated_vf_(compact_separated_vf),
         frontier_budget_factor_(frontier_budget_factor),
         selective_motion_(selective_motion){
        if(frontier_budget_factor_<1||frontier_budget_factor_>8)
            throw std::invalid_argument("frontier budget factor must be 1..8");
        if(selective_motion_&&!retain_feature_lookup_)
            throw std::invalid_argument(
                "selective motion tracking requires feature topology cache");
        if(dense_motion_fast_path&&!selective_motion_)
            throw std::invalid_argument(
                "dense refit optimization requires selective motion tracking");
    }
    std::size_t rebuilds()const{return rebuilds_;}
    std::size_t refits()const{return refits_;}
    std::size_t partial_refits()const{return partial_refits_;}
    std::size_t unchanged_refits()const{return unchanged_refits_;}
    std::size_t dirty_vertex_count()const{
        return selective_motion_?selective_boxes_.dirty_vertices().size():0;
    }
    std::size_t dense_motion_warm_frames()const{
        return selective_motion_?
            selective_boxes_.dense_motion_warm_frames():0;
    }
    std::size_t pair_cut_reuses()const{return pair_cut_reuses_;}
    std::size_t compacted_vf_merges()const{return compacted_vf_merges_;}
    // Count coarse region traversals, not discarded full-size cuts.
    std::size_t pair_cut_fallbacks()const{return pair_cut_fallbacks_;}
    std::size_t pair_cut_size()const{return vf_cut_.size()+ee_cut_.size();}
    // Historical metric name: frames with any coarse region traversed.
    std::size_t pair_cut_skipped_frames()const{return pair_cut_skipped_frames_;}
    std::size_t last_pair_cut_attempted_size()const{
        return last_pair_cut_attempted_size_;
    }

    f::Result query(const f::Mesh& mesh,QueryStages* stage=nullptr) {
        // This is a separate INSTRUMENTED path; never mistake its
        // extra clock reads for a fair head-to-head performance duel.
        using Clock=std::chrono::steady_clock;
        const auto t0=stage?Clock::now():Clock::time_point{};
        // Include all source mesh validation, box construction and
        // topology signature work in the benchmark timer.
        const auto next=signature(mesh);
        const bool topology_changed=(!initialized_||next!=key_);
        const auto t1=stage?Clock::now():Clock::time_point{};
        // Both methods pay current signature validation and fresh swept
        // conservative boxes. Warm calls reuse validated vertex indices.
        f::Built uncached;
        const f::Built* source=nullptr;
        if(selective_motion_) {
            source=&selective_boxes_.update(mesh,topology_changed);
        }else if(retain_feature_lookup_) {
            source=&swept_boxes_.update(mesh,topology_changed);
        }else {
            // A/B control: same spatial tree and same candidate traversal,
            // but fully pay f::build() every frame as in the old method.
            uncached=f::build(mesh);
            source=&uncached;
        }
        const auto& built=*source;
        const auto t2=stage?Clock::now():Clock::time_point{};
        if(topology_changed){
            vertices_.build(built.vertices);
            edges_.build(built.edges);
            faces_.build(built.faces);
            key_=next;initialized_=true;++rebuilds_;
            pair_cut_ready_=false;vf_cut_.clear();ee_cut_.clear();
        }else{
            if(selective_motion_){
                const auto& v=selective_boxes_.dirty_vertices();
                const auto& e=selective_boxes_.dirty_edges();
                const auto& f=selective_boxes_.dirty_faces();
                vertices_.refit_changed(built.vertices,v);
                edges_.refit_changed(built.edges,e);
                faces_.refit_changed(built.faces,f);
                if(v.empty()&&e.empty()&&f.empty())++unchanged_refits_;
                else ++partial_refits_;
            }else{
                vertices_.refit(built.vertices);
                edges_.refit(built.edges);
                faces_.refit(built.faces);
            }
            ++refits_;
        }
        const auto t3=stage?Clock::now():Clock::time_point{};
        f::Result out;
        if(maintain_pair_cut_){
            std::vector<NodePair> next_vf,next_ee;
            std::size_t coarse_regions=0;
            // Two independent cuts have bounded memory even on dense
            // AABB workloads. Budget is fully charged in query() timings.
            const std::size_t max_cut=std::max(std::size_t{64},
                frontier_budget_factor_*(built.vertices.size()+
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
            // Experimentally controlled, fully timed coarsening pass.
            // Compact only a small sparse cut; avoid quadratic scanning
            // overhead on large dense fronts. A merge never loses a leaf
            // product because it requires its complete sibling to exist.
            if(compact_separated_vf_ && next_vf.size()<=512)
                compact_vf_cut(vertices_,faces_,next_vf,compacted_vf_merges_);
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
        const auto t4=stage?Clock::now():Clock::time_point{};
        f::normalize(out);
        if(stage){
            const auto t5=Clock::now();
            const auto ns=[](Clock::time_point a,Clock::time_point b) {
                return std::chrono::duration_cast<
                    std::chrono::nanoseconds>(b-a).count();
            };
            // Only commit a complete stage receipt after the entire
            // candidate set has been generated and normalized.
            *stage={ns(t0,t1),ns(t1,t2),ns(t2,t3),
                    ns(t3,t4),ns(t4,t5),ns(t0,t5),
                    topology_changed,dirty_vertex_count(),out.pairs.size()};
        }
        return out;
    }
};
} // namespace kyoto::contact::atlas

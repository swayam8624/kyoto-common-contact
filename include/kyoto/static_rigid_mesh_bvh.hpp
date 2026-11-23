#pragma once
// Pass 20: conservative static rigid-triangle BVH for REAL FEM solver CCD.
// This is a source acceleration component, not a new narrowphase and not
// proof of speedup. Query results are sorted by the ORIGINAL rigid face
// index: impulse iteration order must not change with hierarchy topology.
#include "kyoto/triangle_mesh_contact.hpp"
#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace kyoto::solid {
class StaticRigidMeshBVH {
    struct Node {
        TriangleBox bounds{};
        int left{-1},right{-1};
        std::size_t triangle{};
        bool leaf()const{return left<0;}
    };
    std::vector<Node> nodes_;
    std::vector<TriangleBox> source_;
    std::vector<std::size_t> indices_;
    int root_{-1};
    static TriangleBox join(const TriangleBox& a,const TriangleBox& b){
        return {{std::min(a.lo.x,b.lo.x),std::min(a.lo.y,b.lo.y),
                 std::min(a.lo.z,b.lo.z)},
                {std::max(a.hi.x,b.hi.x),std::max(a.hi.y,b.hi.y),
                 std::max(a.hi.z,b.hi.z)}};
    }
    static double midpoint(const TriangleBox& b,int axis){
        if(axis==0)return b.lo.x/2+b.hi.x/2;
        if(axis==1)return b.lo.y/2+b.hi.y/2;
        return b.lo.z/2+b.hi.z/2;
    }
    static int longest(const TriangleBox& b){
        const auto x=b.hi.x-b.lo.x,y=b.hi.y-b.lo.y,z=b.hi.z-b.lo.z;
        return x>=y&&x>=z?0:(y>=z?1:2);
    }
    int build(std::size_t first,std::size_t end){
        if(first>=end)throw std::logic_error("empty triangle hierarchy");
        const int here=static_cast<int>(nodes_.size());
        nodes_.emplace_back();
        if(end-first==1){
            nodes_[here].triangle=indices_[first];
            nodes_[here].bounds=source_[indices_[first]];
            return here;
        }
        auto bounds=source_[indices_[first]];
        for(std::size_t j=first+1;j<end;++j)
            bounds=join(bounds,source_[indices_[j]]);
        const int axis=longest(bounds);
        const auto mid=first+(end-first)/2;
        std::nth_element(indices_.begin()+first,indices_.begin()+mid,
            indices_.begin()+end,[&](auto a,auto b){
                const auto ca=midpoint(source_[a],axis);
                const auto cb=midpoint(source_[b],axis);
                return ca==cb?a<b:ca<cb;
            });
        const auto left=build(first,mid),right=build(mid,end);
        nodes_[here].left=left;nodes_[here].right=right;
        nodes_[here].bounds=join(nodes_[left].bounds,nodes_[right].bounds);
        return here;
    }
    void collect(int at,TriangleBox query,double skin,
                 std::vector<std::size_t>& hits,
                 std::size_t& node_tests)const {
        const auto& node=nodes_[at];
        ++node_tests;
        if(!maybe_near(query,node.bounds,skin))return;
        if(node.leaf()){
            if(maybe_near(query,source_[node.triangle],skin))
                hits.push_back(node.triangle);
            return;
        }
        collect(node.left,query,skin,hits,node_tests);
        collect(node.right,query,skin,hits,node_tests);
    }
public:
    explicit StaticRigidMeshBVH(const std::vector<TriangleBox>& faces)
        :source_(faces){
        indices_.resize(source_.size());
        std::iota(indices_.begin(),indices_.end(),std::size_t{0});
        if(!source_.empty()){
            nodes_.reserve(2*source_.size()-1);
            root_=build(0,source_.size());
        }
    }
    std::size_t face_count()const{return source_.size();}
    std::vector<std::size_t> query(TriangleBox a,double skin,
                                   std::size_t* nodes=nullptr)const {
        std::vector<std::size_t> matches;
        std::size_t inspected=0;
        if(root_>=0)collect(root_,a,skin,matches,inspected);
        std::sort(matches.begin(),matches.end());
        if(nodes)*nodes=inspected;
        return matches;
    }
};
} // namespace kyoto::solid

#pragma once
// Pass 28: conservative finite-face broad phase for the native FEM solver.
// This is a face/face specialization, NOT the VF/EE adapter of Pass 27.
// Each internal bound contains every descendant static triangle box.
// The legacy maybe_near predicate is monotone under enlargement of the
// second box: if a parent fails, every descendant fails. At leaves the
// original predicate is evaluated unchanged. Sorted row indices preserve
// the order of the legacy sequential contact impulses and CCD early exit.
#include "kyoto/triangle_mesh_contact.hpp"
#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace kyoto::solid {
class StaticFaceHierarchy {
    struct Node {
        TriangleBox box{};
        std::size_t begin{},end{},left{},right{};
        bool leaf()const{return end-begin==1;}
    };
    std::vector<Node> nodes_;
    std::vector<std::size_t> order_;
    static TriangleBox unite(TriangleBox a,TriangleBox b){
        return {{std::min(a.lo.x,b.lo.x),std::min(a.lo.y,b.lo.y),std::min(a.lo.z,b.lo.z)},
                {std::max(a.hi.x,b.hi.x),std::max(a.hi.y,b.hi.y),std::max(a.hi.z,b.hi.z)}};
    }
    static double center(const TriangleBox& b,int axis){
        if(axis==0)return .5*b.lo.x+.5*b.hi.x;
        if(axis==1)return .5*b.lo.y+.5*b.hi.y;
        return .5*b.lo.z+.5*b.hi.z;
    }
    std::size_t build(const std::vector<TriangleBox>& boxes,std::size_t first,std::size_t last){
        const auto id=nodes_.size();
        auto b=boxes[order_[first]];
        for(auto i=first+1;i<last;++i)b=unite(b,boxes[order_[i]]);
        nodes_.push_back({b,first,last});
        if(last-first==1)return id;
        const auto x=b.hi.x-b.lo.x,y=b.hi.y-b.lo.y,z=b.hi.z-b.lo.z;
        const int axis=x>=y&&x>=z?0:(y>=z?1:2);
        const auto mid=first+(last-first)/2;
        std::nth_element(order_.begin()+first,order_.begin()+mid,order_.begin()+last,
            [&](auto a,auto c){
                const auto u=center(boxes[a],axis),v=center(boxes[c],axis);
                return u==v?a<c:u<v;
            });
        const auto l=build(boxes,first,mid),r=build(boxes,mid,last);
        nodes_[id].left=l;nodes_[id].right=r;
        return id;
    }
    void visit(std::size_t id,const TriangleBox& query,double gap,
               std::vector<std::size_t>& rows,std::size_t& tests)const{
        const auto& n=nodes_[id];
        ++tests;
        if(!maybe_near(query,n.box,gap))return;
        if(n.leaf()){rows.push_back(order_[n.begin]);return;}
        visit(n.left,query,gap,rows,tests);
        visit(n.right,query,gap,rows,tests);
    }
public:
    explicit StaticFaceHierarchy(const std::vector<TriangleBox>& boxes){
        order_.resize(boxes.size());
        std::iota(order_.begin(),order_.end(),std::size_t{0});
        for(const auto& b:boxes)
            if(!(std::isfinite(b.lo.x)&&std::isfinite(b.lo.y)&&std::isfinite(b.lo.z)&&
                 std::isfinite(b.hi.x)&&std::isfinite(b.hi.y)&&std::isfinite(b.hi.z)&&
                 b.lo.x<=b.hi.x&&b.lo.y<=b.hi.y&&b.lo.z<=b.hi.z))
                throw std::invalid_argument("invalid static face box");
        if(!boxes.empty())build(boxes,0,boxes.size());
    }
    void query(const TriangleBox& box,double gap,std::vector<std::size_t>& rows,
               std::size_t& tests)const{
        rows.clear();
        if(!nodes_.empty())visit(0,box,gap,rows,tests);
        std::sort(rows.begin(),rows.end());
    }
};
// The legacy flat candidate producer remains the independent control.
inline void collect_static_face_rows(const TriangleBox& box,double gap,
        const std::vector<TriangleBox>& boxes,const StaticFaceHierarchy* tree,
        std::vector<std::size_t>& rows,std::size_t& tests){
    if(tree){tree->query(box,gap,rows,tests);return;}
    rows.clear();
    for(std::size_t j=0;j<boxes.size();++j){
        ++tests;
        if(maybe_near(box,boxes[j],gap))rows.push_back(j);
    }
}
} // namespace kyoto::solid

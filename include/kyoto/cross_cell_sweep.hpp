#pragma once
// Conservative bipartite sweep on closed binary64 AABBs. Equal-coordinate
// starts precede ends, so touching and zero-width boxes are never lost.
#include "kyoto/exact_swept_cell_candidates.hpp"
#include <limits>
#include <set>
namespace kyoto::geometry {
inline SweptCellCandidates cross_cell_candidates(const std::vector<SweptCellBox>& a,
                                                  const std::vector<SweptCellBox>& b){
    for(const auto* rows:{&a,&b})for(const auto& box:*rows)for(int k=0;k<3;++k)
        if(!std::isfinite(box.lo[k])||!std::isfinite(box.hi[k])||box.lo[k]>box.hi[k])
            throw std::invalid_argument("invalid cross cell box");
    SweptCellCandidates out;
    if(a.empty()||b.empty())return out;
    if(a.size()>std::numeric_limits<std::size_t>::max()/b.size())throw std::overflow_error("cross cell pair count");
    int axis=0;std::size_t best=std::numeric_limits<std::size_t>::max();
    for(int k=0;k<3;++k){
        std::vector<double> lo,hi;lo.reserve(b.size());hi.reserve(b.size());
        for(const auto& x:b){lo.push_back(x.lo[k]);hi.push_back(x.hi[k]);}
        std::sort(lo.begin(),lo.end());std::sort(hi.begin(),hi.end());
        std::size_t count=0;
        for(const auto& x:a)count+=std::size_t((std::upper_bound(lo.begin(),lo.end(),x.hi[k])-lo.begin())-(std::lower_bound(hi.begin(),hi.end(),x.lo[k])-hi.begin()));
        if(count<best){best=count;axis=k;}
    }
    struct Event{double x;bool end,side;std::size_t row;};
    std::vector<Event> events;
    for(bool side:{false,true}){const auto& boxes=side?b:a;for(std::size_t i=0;i<boxes.size();++i){events.push_back({boxes[i].lo[axis],false,side,i});events.push_back({boxes[i].hi[axis],true,side,i});}}
    std::sort(events.begin(),events.end(),[](const auto& x,const auto& y){
        if(x.x!=y.x)return x.x<y.x;
        if(x.end!=y.end)return x.end<y.end;
        if(x.side!=y.side)return x.side<y.side;
        return x.row<y.row;
    });
    std::array<std::set<std::size_t>,2> active;
    for(const auto& e:events){
        if(e.end){active[e.side].erase(e.row);continue;}
        for(auto j:active[!e.side]){
            const std::size_t i0=e.side?j:e.row,i1=e.side?e.row:j;
            ++out.overlap_tests;
            if(swept_cell_overlap(a[i0],b[i1]))out.pairs.push_back({i0,i1});
        }
        active[e.side].insert(e.row);
    }
    std::sort(out.pairs.begin(),out.pairs.end());return out;
}
} // namespace kyoto::geometry

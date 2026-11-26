#pragma once
// Candidate-only broad phase for exact continuous volume audit. Bounds
// contain both endpoints of each linear path. Inclusive overlap retains
// all possible touching and incident cells. Stable canonical pairs make
// the optimized audit order identical to the exhaustive reference.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>
namespace kyoto::geometry {
struct SweptCellBox {std::array<double,3> lo{},hi{};};
using SweptCellPair=std::array<std::size_t,2>;
struct SweptCellCandidates {
    std::vector<SweptCellPair> pairs;
    std::size_t overlap_tests{};
};
inline bool swept_cell_overlap(const SweptCellBox& a,const SweptCellBox& b){
    for(int k=0;k<3;++k)if(a.hi[k]<b.lo[k]||b.hi[k]<a.lo[k])return false;
    return true;
}
inline SweptCellCandidates swept_cell_candidates(const std::vector<SweptCellBox>& boxes,bool exhaustive=false){
    for(const auto& b:boxes)for(int k=0;k<3;++k)
        if(!std::isfinite(b.lo[k])||!std::isfinite(b.hi[k])||b.lo[k]>b.hi[k])
            throw std::invalid_argument("invalid swept cell box");
    SweptCellCandidates r;
    auto check=[&](std::size_t i,std::size_t j){
        ++r.overlap_tests;
        if(swept_cell_overlap(boxes[i],boxes[j]))r.pairs.push_back({std::min(i,j),std::max(i,j)});
    };
    if(exhaustive){
        for(std::size_t i=0;i<boxes.size();++i)for(std::size_t j=i+1;j<boxes.size();++j)check(i,j);
    }else{
        std::vector<std::size_t> order(boxes.size()),active;
        std::iota(order.begin(),order.end(),std::size_t{});
        // Choose an axis from the number of projected interval overlaps,
        // computed by sorting starts and ends. Endpoints touching count.
        // No floating subtraction (overflow) or aspect-ratio heuristic.
        int axis=0;std::size_t best=0;
        for(int k=0;k<3;++k){
            std::vector<double> lo,hi;lo.reserve(boxes.size());hi.reserve(boxes.size());
            for(const auto& b:boxes){lo.push_back(b.lo[k]);hi.push_back(b.hi[k]);}
            std::sort(lo.begin(),lo.end());std::sort(hi.begin(),hi.end());
            std::size_t expired=0,count=0;
            for(std::size_t i=0;i<lo.size();++i){
                while(expired<hi.size()&&hi[expired]<lo[i])++expired;
                count+=i-expired;
            }
            if(k==0||count<best){best=count;axis=k;}
        }
        std::sort(order.begin(),order.end(),[&](auto i,auto j){
            if(boxes[i].lo[axis]!=boxes[j].lo[axis])return boxes[i].lo[axis]<boxes[j].lo[axis];
            return i<j;
        });
        for(auto i:order){
            active.erase(std::remove_if(active.begin(),active.end(),[&](auto j){return boxes[j].hi[axis]<boxes[i].lo[axis];}),active.end());
            for(auto j:active)check(i,j);
            active.push_back(i);
        }
        std::sort(r.pairs.begin(),r.pairs.end());
    }
    return r;
}
} // namespace kyoto::geometry

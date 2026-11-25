#pragma once
// Whole-mesh reference audit of linear source trajectories. Strict AABB
// culling uses endpoint extrema. Exact integer Bernstein signs certify
// cell orientation and pairwise intersection confined to shared simplices.
// Unknown is counted, never promoted to valid. This is not a fast solver.
#include "kyoto/exact_swept_tet_certificate.hpp"
#include "kyoto/exact_swept_cell_candidates.hpp"
namespace kyoto::geometry {
struct ExactSweptVolumeReceipt {
    bool certified{};
    std::size_t cells{},orientation_unknown{},pair_tests{},aabb_pruned{},overlap_tests{};
    std::array<std::size_t,4> tested{},unknown{};
    std::size_t pair_nodes{},axes{},polynomials{};
    std::array<std::size_t,2> first_unknown{};
};
inline bool certify_exact_positive_cell(const ExactLinearTetMotion& m,unsigned limit=6){
    using namespace swept_tet_detail;
    if(limit>12)throw std::invalid_argument("exact orientation depth >12");
    unique_ids(m.ids);
    int exp=0;
    for(const auto* t:{&m.start,&m.end})for(auto p:*t)for(double v:p){
        const auto d=decode(v);if(d.mantissa)exp=std::min(exp,d.exponent);
    }
    const auto x=scaled_motion(m,exp);
    auto recurse=[&](auto&& self,const Motion& a,unsigned depth)->bool{
        const auto c=det_coeff(difference(a[1],a[0]),difference(a[2],a[0]),difference(a[3],a[0]));
        if(signed_coeff(c,1,true))return true;
        if(c.front()<=0||c.back()<=0||depth==limit)return false;
        return self(self,half(a,false),depth+1)&&self(self,half(a,true),depth+1);
    };
    return recurse(recurse,x,0);
}
inline ExactSweptVolumeReceipt audit_exact_swept_volume(
    const std::vector<Point>& start,const std::vector<Point>& end,
    const std::vector<ExactTetIds>& cells,unsigned depth=6,bool exhaustive=false){
    if(start.empty()||start.size()!=end.size()||cells.empty()||depth>12)
        throw std::invalid_argument("invalid swept volume inputs");
    for(const auto* points:{&start,&end})for(auto p:*points)for(double v:p)
        if(!std::isfinite(v))throw std::invalid_argument("nonfinite swept volume");
    struct Entry {ExactLinearTetMotion m;Point lo,hi;};
    std::vector<Entry> entries;entries.reserve(cells.size());
    ExactSweptVolumeReceipt r;r.cells=cells.size();
    for(const auto& ids:cells){
        Entry e{};e.m.ids=ids;
        for(int i=0;i<4;++i){e.m.start[i]=start.at(ids[i]);e.m.end[i]=end.at(ids[i]);}
        e.lo=e.hi=e.m.start[0];
        for(const auto* t:{&e.m.start,&e.m.end})for(auto p:*t)for(int k=0;k<3;++k){
            e.lo[k]=std::min(e.lo[k],p[k]);e.hi[k]=std::max(e.hi[k],p[k]);
        }
        r.orientation_unknown+=!certify_exact_positive_cell(e.m,depth);
        entries.push_back(e);
    }
    std::vector<SweptCellBox> boxes;boxes.reserve(entries.size());
    for(const auto& e:entries)boxes.push_back({e.lo,e.hi});
    const auto candidates=swept_cell_candidates(boxes,exhaustive);
    // Count possible pairs without overflow; the actual broad-phase work
    // is overlap_tests, and aabb_pruned includes axis-sweep rejections.
    const auto n=entries.size();
    const auto p=n%2?n:n/2,q=n%2?(n-1)/2:n-1;
    if(q&&p>std::numeric_limits<std::size_t>::max()/q)
        throw std::overflow_error("swept volume pair count overflow");
    r.pair_tests=p*q;r.overlap_tests=candidates.overlap_tests;
    r.aabb_pruned=r.pair_tests-candidates.pairs.size();
    bool recorded=false;
    for(const auto& [i,j]:candidates.pairs){
        const auto& a=entries[i];const auto& b=entries[j];
        std::size_t shared=0;for(auto x:a.m.ids)for(auto y:b.m.ids)shared+=x==y;
        if(shared==4)throw std::invalid_argument("duplicate swept cell");
        ++r.tested[shared];
        const auto p=certify_exact_swept_tet_pair(a.m,b.m,depth);
        r.pair_nodes+=p.nodes;r.axes+=p.axes;r.polynomials+=p.polynomials;
        if(!p.certified){++r.unknown[shared];if(!recorded){r.first_unknown={i,j};recorded=true;}}
    }
    r.certified=r.orientation_unknown==0&&std::all_of(r.unknown.begin(),r.unknown.end(),[](auto x){return x==0;});
    return r;
}
} // namespace kyoto::geometry

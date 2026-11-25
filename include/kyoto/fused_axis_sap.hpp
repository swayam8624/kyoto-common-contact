#pragma once
// Persistent axis-selected SAP with fused active-list compaction and query.
// This is NOT the full proposed dual-tree Spacetime Contact Atlas.
// It reuses sorting permutations and verified topology lookup, never candidate membership;
// every new frame rebuilds all swept AABBs and rechecks every live pair.
// Topology epoch, IDs, connectivity, categories and masks are compared;
// a changed signature forces a cold order initialization. No stale
// geometry or cached candidate can silently survive an invalidation.
#include "kyoto/feature_broadphase.hpp"
#include "kyoto/projected_sap_axis.hpp"
#include "kyoto/cached_feature_boxes.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace kyoto::contact::persistent {

namespace f=kyoto::contact::features;

struct FusedSweepEntry {
    bool vertex{};
    std::size_t index{};
};

class FusedAxisSweep {
    atlas::CachedFeatureBoxes boxes_;
    std::size_t fallback_sorts_{};
    std::vector<std::uint64_t> topology_;
    std::vector<std::size_t> ee_order_;
    std::vector<FusedSweepEntry> vf_order_;
    bool initialized_{};
    
    // Spatial-first deliberately counts overlap predicates before mask/incident
    // rejection. The key set is the same; work receipts differ by order.
    bool valid_pair(const f::Record& a,const f::Record& b,f::Result& r)const {
        ++r.candidate_aabb_tests;
        if(!overlaps(a.bounds,b.bounds))return false;
        if(!f::allows(a,b)){++r.mask_rejections;return false;}
        if(f::incident(a,b)){++r.incident_rejections;return false;}
        return true;
    }
    int axis_{};
    std::size_t invalidations_{};
    std::size_t warm_frames_{};

    static std::vector<std::uint64_t> signature(const f::Mesh& m) {
        std::vector<std::uint64_t> s;
        s.reserve(8 + m.vertices.size()*4 + m.edges.size()*6 +
                  m.faces.size()*7);
        s.insert(s.end(),{m.topology_epoch,
            std::uint64_t(m.vertices.size()),
            std::uint64_t(m.edges.size()),std::uint64_t(m.faces.size())});
        for(const auto& v:m.vertices)
            s.insert(s.end(),{v.id,v.object,v.category,v.mask});
        for(const auto& e:m.edges)
            s.insert(s.end(),{e.id,e.vertices[0],e.vertices[1],
                              e.category,e.mask});
        for(const auto& face:m.faces)
            s.insert(s.end(),{face.id,face.vertices[0],face.vertices[1],
                              face.vertices[2],face.category,face.mask});
        return s;
    }

    // Bound repair work; sorting a partially permuted array retains every ID.
    template<class T,class Less>
    void repair_order(std::vector<T>& order,Less less) {
        std::size_t moves=0;
        for(std::size_t i=1;i<order.size();++i){
            std::size_t j=i;
            while(j>0&&less(order[j],order[j-1])){
                std::swap(order[j],order[j-1]);--j;
                if(++moves/8>=order.size()){
                    std::sort(order.begin(),order.end(),less);++fallback_sorts_;return;
                }
            }
        }
    }
public:
    FusedAxisSweep()=default;
    std::size_t rebuilds()const{return initialized_?invalidations_+1:0;}
    std::size_t fallback_sorts()const{return fallback_sorts_;}
    std::size_t invalidations()const{return invalidations_;}
    std::size_t warm_frames()const{return warm_frames_;}

    // A full physical frame must be measured INCLUDING f::build() and
    // topology/signature validation. Run this method inside the timer.
    f::Result query(const f::Mesh& mesh) {
        if(!std::isfinite(mesh.thickness)||mesh.thickness<0)throw std::invalid_argument("invalid thickness");
        const auto key=signature(mesh);
        const bool cold=!initialized_ || key!=topology_;
        const auto& built=boxes_.update(mesh,cold);
        if(cold) {
            axis_=f::projected_sap_axis(built);
            if(initialized_)++invalidations_;
            initialized_=true;
            topology_=key;
            ee_order_.resize(built.edges.size());
            std::iota(ee_order_.begin(),ee_order_.end(),std::size_t{0});
            vf_order_.clear();
            vf_order_.reserve(built.vertices.size()+built.faces.size());
            for(std::size_t i=0;i<built.vertices.size();++i)
                vf_order_.push_back({true,i});
            for(std::size_t i=0;i<built.faces.size();++i)
                vf_order_.push_back({false,i});
        } else ++warm_frames_;
        auto lo=[&](const f::Record& r){return f::component(r.bounds.min,axis_);};
        auto hi=[&](const f::Record& r){return f::component(r.bounds.max,axis_);};

        // Warm path uses bounded order repair; cold path sorts.
        auto ee_less=[&](std::size_t i,std::size_t j){
            const auto& a=built.edges[i],&b=built.edges[j];
            if(lo(a)!=lo(b))
                return lo(a)<lo(b);
            return a.id<b.id;
        };
        auto vf_less=[&](FusedSweepEntry a,FusedSweepEntry b){
            const auto& x=a.vertex?built.vertices[a.index]:built.faces[a.index];
            const auto& y=b.vertex?built.vertices[b.index]:built.faces[b.index];
            if(lo(x)!=lo(y))
                return lo(x)<lo(y);
            if(a.vertex!=b.vertex)return a.vertex<b.vertex;
            return a.index<b.index;
        };
        // Cold topology uses O(n log n) sorting; warm frames retain ordering.
        if(cold){std::sort(ee_order_.begin(),ee_order_.end(),ee_less);std::sort(vf_order_.begin(),vf_order_.end(),vf_less);}
        else{repair_order(ee_order_,ee_less);repair_order(vf_order_,vf_less);}

        f::Result result;
        // Expiration and candidate enumeration share one pass over the
        // active set. Inclusive equality survives: hi == lo is a candidate.
        // Only the opposing type must be compacted on a VF event. Stale
        // same-type records cannot produce a pair until they are inspected.
        std::vector<std::size_t> active;
        active.reserve(built.edges.size());
        for(auto i:ee_order_) {
            const auto& a=built.edges[i];
            const double lower=lo(a);std::size_t kept=0;
            for(auto j:active) {
                const auto& b=built.edges[j];
                if(hi(b)<lower)continue;
                active[kept++]=j;
                if(valid_pair(a,b,result))
                    result.pairs.push_back({f::Kind::EdgeEdge,
                        std::min(a.id,b.id),std::max(a.id,b.id)});
            }
            active.resize(kept);active.push_back(i);
        }
        std::vector<std::size_t> av,af;
        av.reserve(built.vertices.size());af.reserve(built.faces.size());
        for(const auto e:vf_order_) {
            auto& own=e.vertex?av:af;
            auto& other=e.vertex?af:av;
            const auto& records=e.vertex?built.vertices:built.faces;
            const auto& opposing=e.vertex?built.faces:built.vertices;
            const auto& a=records[e.index];const double lower=lo(a);
            std::size_t kept=0;
            for(auto j:other){
                const auto& b=opposing[j];
                if(hi(b)<lower)continue;
                other[kept++]=j;
                const auto& v=e.vertex?a:b;
                const auto& face=e.vertex?b:a;
                if(valid_pair(v,face,result))
                    result.pairs.push_back({f::Kind::VertexFace,v.id,face.id});
            }
            other.resize(kept);own.push_back(e.index);
        }
        f::normalize(result);
        return result;
    }
};
} // namespace kyoto::contact::persistent

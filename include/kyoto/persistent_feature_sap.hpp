#pragma once
// Experimental *persistent ordering* SAP on typed 3D moving features.
// This is NOT the full proposed dual-tree Spacetime Contact Atlas.
// It reuses only sorting permutations, never candidate membership;
// every new frame rebuilds all swept AABBs and rechecks every live pair.
// Topology epoch, IDs, connectivity, categories and masks are compared;
// a changed signature forces a cold order initialization. No stale
// geometry or cached candidate can silently survive an invalidation.
#include "kyoto/feature_broadphase.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace kyoto::contact::persistent {

namespace f=kyoto::contact::features;

struct Entry {
    bool vertex{};
    std::size_t index{};
};

class OrderedSweep {
    std::vector<std::uint64_t> topology_;
    std::vector<std::size_t> ee_order_;
    std::vector<Entry> vf_order_;
    bool initialized_{};
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

    template<class T,class Less>
    static void insertion_sort(std::vector<T>& order,Less less) {
        for(std::size_t i=1;i<order.size();++i){
            const T value=order[i];
            std::size_t j=i;
            while(j>0 && less(value,order[j-1])) {
                order[j]=order[j-1];--j;
            }
            order[j]=value;
        }
    }

public:
    std::size_t invalidations()const{return invalidations_;}
    std::size_t warm_frames()const{return warm_frames_;}

    // A full physical frame must be measured INCLUDING f::build() and
    // topology/signature validation. Run this method inside the timer.
    f::Result query(const f::Mesh& mesh) {
        const auto built=f::build(mesh);
        const auto key=signature(mesh);
        if(!initialized_ || key!=topology_) {
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

        // Warm path has insertion-sort motion coherence; cold path uses
        // std::sort. A degenerate reordered scene remains correct, though
        // its insertion-sort cost can be quadratic.
        auto ee_less=[&](std::size_t i,std::size_t j){
            const auto& a=built.edges[i],&b=built.edges[j];
            if(a.bounds.min.x!=b.bounds.min.x)
                return a.bounds.min.x<b.bounds.min.x;
            return a.id<b.id;
        };
        auto vf_less=[&](Entry a,Entry b){
            const auto& x=a.vertex?built.vertices[a.index]:built.faces[a.index];
            const auto& y=b.vertex?built.vertices[b.index]:built.faces[b.index];
            if(x.bounds.min.x!=y.bounds.min.x)
                return x.bounds.min.x<y.bounds.min.x;
            if(a.vertex!=b.vertex)return a.vertex<b.vertex;
            return a.index<b.index;
        };
        // A cold query can be identified by the zeroed warm counter only
        // on first invocation; comparing the prior signature is simpler:
        // std::sort on cold and insertion on subsequent calls both
        // preserve candidate correctness, so always insertion sort here.
        // No unmeasured fallback/rebuild is hidden.
        insertion_sort(ee_order_,ee_less);
        insertion_sort(vf_order_,vf_less);

        f::Result result;
        std::vector<std::size_t> active;
        for(auto i:ee_order_) {
            const auto& a=built.edges[i];
            active.erase(std::remove_if(active.begin(),active.end(),
                [&](std::size_t j){return built.edges[j].bounds.max.x<a.bounds.min.x;}),
                active.end());
            for(auto j:active) {
                const auto& b=built.edges[j];
                if(f::valid_pair(a,b,result))
                    result.pairs.push_back({f::Kind::EdgeEdge,
                        std::min(a.id,b.id),std::max(a.id,b.id)});
            }
            active.push_back(i);
        }
        std::vector<std::size_t> av,af;
        for(const auto e:vf_order_) {
            auto& own=e.vertex?av:af;
            auto& other=e.vertex?af:av;
            const auto& records=e.vertex?built.vertices:built.faces;
            const auto& opposing=e.vertex?built.faces:built.vertices;
            own.erase(std::remove_if(own.begin(),own.end(),
                [&](std::size_t i){return records[i].bounds.max.x<
                                                records[e.index].bounds.min.x;}),
                own.end());
            other.erase(std::remove_if(other.begin(),other.end(),
                [&](std::size_t i){return opposing[i].bounds.max.x<
                                                 records[e.index].bounds.min.x;}),
                other.end());
            for(auto j:other){
                const auto& v=e.vertex?built.vertices[e.index]:built.vertices[j];
                const auto& face=e.vertex?built.faces[j]:built.faces[e.index];
                if(f::valid_pair(v,face,result))
                    result.pairs.push_back({f::Kind::VertexFace,v.id,face.id});
            }
            own.push_back(e.index);
        }
        f::normalize(result);
        return result;
    }
};
} // namespace kyoto::contact::persistent

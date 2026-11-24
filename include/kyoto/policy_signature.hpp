#pragma once
// Experimental Pass 27: collision-policy summaries over BVH leaf sets.
// Policy is solver supplied; no extra geometric or incident exclusions.
// Every exact rejection is a statement about ALL cross-product leaves.
#include <cstdint>
namespace kyoto::contact::atlas {
struct PolicySignature {
    std::uint64_t categories{}, masks{}, relation{}, transposed{};
    static PolicySignature leaf(std::uint64_t category, std::uint64_t mask) {
        PolicySignature s{category,mask,0,0};
        // Low eight category bits: relation bit (i,j) means a leaf
        // simultaneously belongs to i and accepts j. Mask bits above
        // 7 are harmless when BOTH nodes use low category bits only.
        for(unsigned i=0;i<8;++i){
            if(category & (std::uint64_t{1}<<i))
                s.relation |= (mask & 255u) << (8*i);
            if(mask & (std::uint64_t{1}<<i))
                s.transposed |= (category & 255u) << (8*i);
        }
        return s;
    }
    static PolicySignature unite(const PolicySignature& a,
                                 const PolicySignature& b) {
        return {a.categories|b.categories,a.masks|b.masks,
                a.relation|b.relation,a.transposed|b.transposed};
    }
    bool may_collide(const PolicySignature& b) const {
        if(!(categories & b.masks) || !(b.categories & masks))
            return false;
        // High-bit categories: the cheap union condition is only
        // necessary. Fail open rather than truncate a valid pair.
        if((categories | b.categories) & ~std::uint64_t{255})
            return true;
        return (relation & b.transposed)!=0;
    }
};
} // namespace kyoto::contact::atlas

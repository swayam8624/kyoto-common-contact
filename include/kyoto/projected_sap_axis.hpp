#pragma once
// Choose the sweep axis by the exact number of inclusive projected
// interval overlaps (VF and EE). Sorting/selection are paid by callers.
#include "kyoto/feature_broadphase.hpp"
namespace kyoto::contact::features {
inline double component(Vec3 p,int axis){return axis==0?p.x:axis==1?p.y:p.z;}
inline int projected_sap_axis(const Built& mesh){
    int best_axis=0;std::uint64_t best=std::numeric_limits<std::uint64_t>::max();
    for(int axis=0;axis<3;++axis){
        std::vector<double> starts,ends;starts.reserve(mesh.edges.size());ends.reserve(mesh.edges.size());
        for(const auto& e:mesh.edges){starts.push_back(component(e.bounds.min,axis));ends.push_back(component(e.bounds.max,axis));}
        std::sort(starts.begin(),starts.end());std::sort(ends.begin(),ends.end());
        std::uint64_t count=0;std::size_t expired=0;
        for(std::size_t i=0;i<starts.size();++i){while(expired<ends.size()&&ends[expired]<starts[i])++expired;count+=i-expired;}
        starts.clear();ends.clear();
        for(const auto& f:mesh.faces){starts.push_back(component(f.bounds.min,axis));ends.push_back(component(f.bounds.max,axis));}
        std::sort(starts.begin(),starts.end());std::sort(ends.begin(),ends.end());
        for(const auto& v:mesh.vertices){const auto a=std::upper_bound(starts.begin(),starts.end(),component(v.bounds.max,axis))-starts.begin();const auto b=std::lower_bound(ends.begin(),ends.end(),component(v.bounds.min,axis))-ends.begin();count+=std::uint64_t(a-b);}
        if(count<best){best=count;best_axis=axis;}
    }
    return best_axis;
}
inline void permute_sap_axis(Built& mesh,int axis){
    if(axis<0||axis>2)throw std::invalid_argument("invalid SAP axis");
    for(auto* rows:{&mesh.vertices,&mesh.edges,&mesh.faces})for(auto& r:*rows){
        if(axis==1){std::swap(r.bounds.min.x,r.bounds.min.y);std::swap(r.bounds.max.x,r.bounds.max.y);}
        if(axis==2){std::swap(r.bounds.min.x,r.bounds.min.z);std::swap(r.bounds.max.x,r.bounds.max.z);}
    }
}
inline Result adaptive_sweep_and_prune(const Mesh& m){auto b=build(m);permute_sap_axis(b,projected_sap_axis(b));return sweep_and_prune(b);}
} // namespace kyoto::contact::features

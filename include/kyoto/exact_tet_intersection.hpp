#pragma once
// Exact reference for STATIC binary64 tetrahedra. This is a research
// oracle, not a fast broad phase or continuous collision detector.
// Convert every finite binary64 coordinate to a common integer grid;
// intersect the eight exact halfspaces using homogeneous Cramer's rule.
// A bounded nonempty polytope has a vertex at three independent active
// planes, even if the intersection is lower-dimensional. It equals the
// shared topological simplex iff all such vertices belong to that simplex.
#include "kyoto/initial_tet_separation.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <bit>
#include <cstdint>
#include <vector>

namespace kyoto::geometry {
using ExactTetIds=std::array<std::size_t,4>;
struct ExactTetIntersection {
    bool matches_shared_simplex{};
    std::size_t shared_vertices{}, independent_plane_triples{}, feasible_plane_triples{};
};
namespace exact_tet_detail {
using Z=boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>,boost::multiprecision::et_off>;
using Q=std::array<Z,3>;
using QT=std::array<Q,4>;
struct Dyadic {std::uint64_t mantissa{};int exponent{};bool negative{};};
inline Dyadic decode(double x){
    static_assert(sizeof(double)==8&&std::numeric_limits<double>::is_iec559&&
                  std::numeric_limits<double>::digits==53);
    if(!std::isfinite(x))throw std::invalid_argument("nonfinite exact tet coordinate");
    const auto bits=std::bit_cast<std::uint64_t>(x);
    const auto exp=int((bits>>52)&0x7ff);
    auto mantissa=bits&((std::uint64_t{1}<<52)-1);
    if(exp)mantissa|=std::uint64_t{1}<<52;
    return {mantissa,exp?exp-1023-52:-1074,bool(bits>>63)};
}
inline Q sub(const Q& a,const Q& b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline Q cross(const Q& a,const Q& b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline Z dot(const Q& a,const Q& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
struct Plane{Q n;Z d;}; // n dot x <= d, opposite vertex strictly inside
inline std::array<Plane,4> planes(const QT& t){
    std::array<Plane,4> out;
    for(int i=0;i<4;++i){
        std::array<int,3> j{};int k=0;
        for(int a=0;a<4;++a)if(a!=i)j[k++]=a;
        Q n=cross(sub(t[j[1]],t[j[0]]),sub(t[j[2]],t[j[0]]));
        Z d=dot(n,t[j[0]]), side=dot(n,t[i])-d;
        if(side==0)throw std::invalid_argument("degenerate exact tetrahedron");
        if(side>0){for(auto& x:n)x=-x;d=-d;}
        out[i]={std::move(n),std::move(d)};
    }
    return out;
}
inline void unique_ids(ExactTetIds ids){
    std::sort(ids.begin(),ids.end());
    if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())
        throw std::invalid_argument("repeated exact tet vertex ID");
}
} // namespace exact_tet_detail

inline ExactTetIntersection exact_static_tet_intersection(
    const Tetrahedron& a,const ExactTetIds& ai,const Tetrahedron& b,const ExactTetIds& bi){
    using namespace exact_tet_detail;
    unique_ids(ai);unique_ids(bi);
    std::array<bool,4> common{};
    ExactTetIntersection r;
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)if(ai[i]==bi[j]){
        if(a[i]!=b[j])throw std::invalid_argument("shared node has inconsistent coordinates");
        common[i]=true;++r.shared_vertices;
    }
    if(r.shared_vertices==4)throw std::invalid_argument("duplicate exact tetrahedron");
    int min_exponent=0;
    for(const auto* t:{&a,&b})for(const auto& p:*t)for(double x:p){
        const auto d=decode(x);if(d.mantissa)min_exponent=std::min(min_exponent,d.exponent);
    }
    auto convert=[&](const Tetrahedron& t){
        QT result;
        for(int i=0;i<4;++i)for(int j=0;j<3;++j){
            const auto d=decode(t[i][j]);
            Z v=0;
            if(d.mantissa){
                const unsigned shift=unsigned(d.exponent-min_exponent);
                for(unsigned bit=0;bit<53;++bit)
                    if((d.mantissa>>bit)&1u)boost::multiprecision::bit_set(v,shift+bit);
            }
            result[i][j]=d.negative?-v:v;
        }
        return result;
    };
    const auto ap=planes(convert(a)),bp=planes(convert(b));
    std::array<Plane,8> p;
    for(int i=0;i<4;++i){p[i]=ap[i];p[i+4]=bp[i];}
    bool extra=false;
    for(int i=0;i<8;++i)for(int j=i+1;j<8;++j)for(int k=j+1;k<8;++k){
        const Q u=cross(p[j].n,p[k].n),v=cross(p[k].n,p[i].n),w=cross(p[i].n,p[j].n);
        Z den=dot(p[i].n,u);
        if(den==0)continue;
        ++r.independent_plane_triples;
        Q num;
        for(int q=0;q<3;++q)num[q]=p[i].d*u[q]+p[j].d*v[q]+p[k].d*w[q];
        if(den<0){den=-den;for(auto& q:num)q=-q;}
        bool feasible=true;
        for(const auto& h:p)if(dot(h.n,num)>h.d*den){feasible=false;break;}
        if(!feasible)continue;
        ++r.feasible_plane_triples; // not unique vertices
        if(!r.shared_vertices){extra=true;continue;}
        // A point inside tetrahedron A is in the shared simplex iff
        // every barycentric coordinate of a nonshared vertex is zero.
        // Those coordinates vanish on their opposite planes exactly.
        for(int q=0;q<4;++q)if(!common[q]&&dot(p[q].n,num)!=p[q].d*den){extra=true;break;}
    }
    if(r.shared_vertices&&!r.feasible_plane_triples)
        throw std::logic_error("exact intersection lost a shared vertex");
    r.matches_shared_simplex=!extra;
    return r;
}

struct ExactSelfVolumeReceipt {
    bool valid{true};
    std::size_t pair_tests{},aabb_pruned{},exact_pairs{},incident_pairs{},plane_triples{};
    std::array<std::size_t,2> invalid_pair{};
};
// Exhaustive static audit. Strict AABB separation is exact for stored
// finite endpoints. Every remaining pair is checked against the common
// topological simplex, including pairs that share a vertex/edge/face.
inline ExactSelfVolumeReceipt audit_exact_self_volume(
    const std::vector<Point>& vertices,const std::vector<ExactTetIds>& cells){
    if(vertices.empty()||cells.empty())throw std::invalid_argument("empty exact self volume");
    struct Entry{Tetrahedron tet;Point lo,hi;};
    std::vector<Entry> entries;entries.reserve(cells.size());
    for(const auto& ids:cells){
        exact_tet_detail::unique_ids(ids);
        Entry e{};
        for(int i=0;i<4;++i){
            e.tet[i]=vertices.at(ids[i]);
            for(double x:e.tet[i])if(!std::isfinite(x))throw std::invalid_argument("nonfinite exact self volume");
            if(!i)e.lo=e.hi=e.tet[i];
            else for(int j=0;j<3;++j){e.lo[j]=std::min(e.lo[j],e.tet[i][j]);e.hi[j]=std::max(e.hi[j],e.tet[i][j]);}
        }
        // Validate each tetrahedron, including isolated cells and cells
        // whose AABBs never overlap another tetrahedron.
        // Use an exact per-cell common-grid conversion via a self pair
        // with distinct IDs; identical geometry must have an intersection.
        const auto valid=exact_static_tet_intersection(e.tet,{0,1,2,3},e.tet,{4,5,6,7});
        if(valid.matches_shared_simplex)throw std::logic_error("nonempty exact tet lost");
        entries.push_back(e);
    }
    ExactSelfVolumeReceipt out;
    for(std::size_t i=0;i<cells.size();++i)for(std::size_t j=i+1;j<cells.size();++j){
        ++out.pair_tests;
        bool separated=false;
        for(int k=0;k<3;++k)separated|=entries[i].hi[k]<entries[j].lo[k]||entries[j].hi[k]<entries[i].lo[k];
        if(separated){++out.aabb_pruned;continue;}
        ++out.exact_pairs;
        const auto r=exact_static_tet_intersection(entries[i].tet,cells[i],entries[j].tet,cells[j]);
        out.incident_pairs+=r.shared_vertices!=0;out.plane_triples+=r.independent_plane_triples;
        if(!r.matches_shared_simplex){out.valid=false;out.invalid_pair={i,j};return out;}
    }
    return out;
}
} // namespace kyoto::geometry

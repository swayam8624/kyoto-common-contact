#include "kyoto/exact_tet_intersection.hpp"
#include <iostream>
#include <random>
using namespace kyoto::geometry;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Tetrahedron translated(Tetrahedron a,Point p){for(auto& v:a)for(int k=0;k<3;++k)v[k]+=p[k];return a;}
int main(){
    const Tetrahedron a{{{0,0,0},{1,0,0},{0,1,0},{0,0,1}}};
    const ExactTetIds ids{0,1,2,3},different{4,5,6,7};
    const auto check=[&](Tetrahedron b,ExactTetIds bi,bool expected,std::size_t shared){
        const auto r=exact_static_tet_intersection(a,ids,b,bi),s=exact_static_tet_intersection(b,bi,a,ids);
        need(r.matches_shared_simplex==expected&&s.matches_shared_simplex==expected&&r.shared_vertices==shared&&s.shared_vertices==shared,"incident or disjoint control failed");
    };
    check(translated(a,{2,0,0}),different,true,0);
    check(translated(a,{1,0,0}),different,false,0);
    check(translated(a,{.25,.25,.25}),different,false,0);
    check({{{0,0,0},{1,0,0},{0,1,0},{0,0,-1}}},{0,1,2,4},true,3);
    check({{{0,0,0},{1,0,0},{0,1,0},{.1,.1,.1}}},{0,1,2,4},false,3);
    check({{{0,0,0},{1,0,0},{0,-1,0},{0,0,-1}}},{0,1,4,5},true,2);
    check({{{0,0,0},{1,0,0},{.1,.2,.1},{0,.1,.3}}},{0,1,4,5},false,2);
    check({{{0,0,0},{-1,0,0},{0,-1,0},{0,0,-1}}},{0,4,5,6},true,1);
    check({{{0,0,0},{.2,0,0},{0,.2,0},{0,0,.2}}},{0,4,5,6},false,1);
    // Shared vertex alone does not authorize containment of one tetrahedron
    // in another. This exercises plane-triple enumeration.
    check({{{0,0,0},{2,0,0},{0,2,0},{0,0,2}}},{0,4,5,6},false,1);
    bool threw=false;try{(void)exact_static_tet_intersection(a,ids,a,ids);}catch(const std::invalid_argument&){threw=true;}need(threw,"duplicate tetrahedron accepted");
    threw=false;try{(void)exact_static_tet_intersection(a,ids,translated(a,{1,0,0}),{0,4,5,6});}catch(const std::invalid_argument&){threw=true;}need(threw,"inconsistent shared coordinate accepted");
    // Uniform exact powers of two exercise the full normal/subnormal
    // exponent range without changing the mathematical answer.
    for(int exp:{-1070,-500,-20,0,20,500,1000}){
        auto x=a,y=translated(a,{.5,.5,.5});
        for(auto* t:{&x,&y})for(auto& p:*t)for(auto& q:p)q=std::ldexp(q,exp);
        need(exact_static_tet_intersection(x,ids,y,different).matches_shared_simplex,"scaled oblique separation failed");
    }
    std::mt19937_64 rng(3902027);std::uniform_int_distribution<int> offset(-16,16);
    std::size_t clear=0,contact=0;
    // Axis-aligned simplex translated on an exact dyadic grid. For
    // T={x>=0,sum x<=1}, T intersects T+d iff sum(max(d,0))<=1
    // and sum(max(-d,0))<=1. This is independent of plane enumeration.
    for(int n=0;n<2000;++n){
        Point d{};int pos=0,neg=0;
        for(auto& q:d){const auto k=offset(rng);pos+=std::max(k,0);neg+=std::max(-k,0);q=double(k)/16.;}
        const bool separated=pos>16||neg>16;
        const auto r=exact_static_tet_intersection(a,ids,translated(a,d),different);
        need(r.matches_shared_simplex==separated,"translated simplex analytic oracle mismatch");
        clear+=separated;contact+=!separated;
    }
    const std::vector<Point> vertices{{0,0,0},{1,0,0},{0,1,0},{0,0,1},{0,0,-1},{.1,.1,.1}};
    const auto good=audit_exact_self_volume(vertices,{{0,1,2,3},{0,1,2,4}});
    need(good.valid&&good.incident_pairs==1&&good.exact_pairs==1,"valid shared-face complex rejected");
    const auto bad=audit_exact_self_volume(vertices,{{0,1,2,3},{0,1,2,5}});
    need(!bad.valid&&bad.invalid_pair==std::array<std::size_t,2>{0,1},"invalid same-body overlap missed");
    // Two conforming unit cubes sharing a triangulated face. Audit all
    // tetrahedron pairs, including edge/vertex adjacency, not only faces.
    std::vector<Point> grid;
    const auto idx=[](int i,int j,int k){return std::size_t((i*2+j)*2+k);};
    for(int i=0;i<3;++i)for(int j=0;j<2;++j)for(int k=0;k<2;++k)
        grid.push_back({double(i),double(j),double(k)});
    std::vector<ExactTetIds> cells;
    for(int i=0;i<2;++i){
        const auto a0=idx(i,0,0),b0=idx(i+1,0,0),c0=idx(i,1,0),d0=idx(i+1,1,0),
                   e0=idx(i,0,1),f0=idx(i+1,0,1),g0=idx(i,1,1),h0=idx(i+1,1,1);
        cells.insert(cells.end(),{{a0,b0,d0,h0},{a0,d0,c0,h0},{a0,c0,g0,h0},
                                  {a0,g0,e0,h0},{a0,e0,f0,h0},{a0,f0,b0,h0}});
    }
    const auto mesh=audit_exact_self_volume(grid,cells);
    need(mesh.valid&&mesh.pair_tests==66&&mesh.incident_pairs>0,"conforming two-cube mesh rejected");
    grid[idx(2,0,0)]={.5,.25,.25};
    need(!audit_exact_self_volume(grid,cells).valid,"folded cell overlap missed");
    std::cout<<"PASS exact static intersection analytic translated cases=2000 separated="<<clear<<" intersecting="<<contact<<"; incident simplex, extremes, mesh controls passed\n";
}

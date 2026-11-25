// Held-out whole-mesh integration: same physical source positions and
// candidate producer for all three arithmetic modes. Prescribed, not FEM.
#include "kyoto/exact_swept_volume_audit.hpp"
#include <chrono>
#include <iostream>
using namespace kyoto::geometry;
struct Mesh{std::vector<Point> p;std::vector<ExactTetIds> cells;};
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static bool same(const ExactSweptVolumeReceipt& a,const ExactSweptVolumeReceipt& b){
    return a.certified==b.certified&&a.cells==b.cells&&a.orientation_unknown==b.orientation_unknown&&
           a.pair_tests==b.pair_tests&&a.aabb_pruned==b.aabb_pruned&&a.overlap_tests==b.overlap_tests&&
           a.tested==b.tested&&a.unknown==b.unknown&&a.pair_nodes==b.pair_nodes&&a.axes==b.axes&&
           a.polynomials==b.polynomials&&a.first_unknown==b.first_unknown;
}
static Mesh box(unsigned n,unsigned m,unsigned l){
    Mesh out;auto idx=[&](unsigned i,unsigned j,unsigned k){return std::size_t((i*(m+1)+j)*(l+1)+k);};
    for(unsigned i=0;i<=n;++i)for(unsigned j=0;j<=m;++j)for(unsigned k=0;k<=l;++k)
        out.p.push_back({double(i),double(j),double(k)});
    for(unsigned i=0;i<n;++i)for(unsigned j=0;j<m;++j)for(unsigned k=0;k<l;++k){
        const auto a=idx(i,j,k),b=idx(i+1,j,k),c=idx(i,j+1,k),d=idx(i+1,j+1,k),
                   e=idx(i,j,k+1),f=idx(i+1,j,k+1),g=idx(i,j+1,k+1),h=idx(i+1,j+1,k+1);
        out.cells.insert(out.cells.end(),{{a,b,d,h},{a,d,c,h},{a,c,g,h},{a,g,e,h},{a,e,f,h},{a,f,b,h}});
    }
    return out;
}
int main(){
    using Clock=std::chrono::steady_clock;
    std::cout<<"width,regime,block,slot,arm,cells,pairs,exact_pairs,unknown,orientation_unknown,certified,axes,polynomials,elapsed_ns\n";
    for(unsigned width:{1u,3u})for(int regime=0;regime<4;++regime){
        const auto m=box(width,2,2);auto q=m.p;
        for(auto& p:q){const auto x=p[0],y=p[1],z=p[2];
            if(regime==1){p[0]=x+y/8.;p[1]=y+z/16.;}
            if(regime==2){p[0]=x+y*z/256.;p[1]=y+x*z/256.;p[2]=z+x*y/256.;}
        }
        if(regime==3)std::swap(q[m.cells.front()[0]],q[m.cells.front()[1]]);
        const auto r=audit_exact_swept_volume(m.p,q,m.cells,2,false,false,false);
        need(regime==3?!r.certified:r.certified,"whole-volume control classification failed");
        std::cerr<<"width="<<width<<" regime="<<regime<<" cells="<<r.cells<<" exact_pairs="
                 <<std::accumulate(r.tested.begin(),r.tested.end(),std::size_t{})<<" certified="<<r.certified<<'\n';
        for(int block=-1;block<3;++block)for(int slot=0;slot<6;++slot){
            const int arm=((slot<3?slot:5-slot)+block+3)%3;
            const auto begin=Clock::now();
            const auto s=audit_exact_swept_volume(m.p,q,m.cells,2,false,arm!=0,arm==2);
            const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();
            need(same(r,s)&&ns>0,"whole volume receipt mismatch");
            if(block>=0)std::cout<<width<<','<<regime<<','<<block<<','<<slot<<','<<(arm==0?"reference":arm==1?"factored":"constant_fast")<<','<<s.cells<<','<<s.pair_tests<<','
                <<std::accumulate(s.tested.begin(),s.tested.end(),std::size_t{})<<','<<std::accumulate(s.unknown.begin(),s.unknown.end(),std::size_t{})<<','<<s.orientation_unknown<<','<<s.certified<<','<<s.axes<<','<<s.polynomials<<','<<ns<<'\n';
        }
    }
}

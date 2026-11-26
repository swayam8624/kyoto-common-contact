// Paid whole-audit comparison: original determinant expansion versus factored normal.
#define KYOTO_FIXTURE_PATTERNS_ONLY
#include "../tests/volume_boundary_fixture_tests.cpp"
#undef KYOTO_FIXTURE_PATTERNS_ONLY
#include "kyoto/exact_swept_volume_audit.hpp"
#include <chrono>
using Clock=std::chrono::steady_clock;
static bool same(const ExactSweptVolumeReceipt& a,const ExactSweptVolumeReceipt& b){
    return a.certified==b.certified&&a.cells==b.cells&&a.orientation_unknown==b.orientation_unknown&&
           a.pair_tests==b.pair_tests&&a.aabb_pruned==b.aabb_pruned&&a.tested==b.tested&&a.unknown==b.unknown&&
           a.pair_nodes==b.pair_nodes&&a.axes==b.axes&&a.polynomials==b.polynomials&&a.first_unknown==b.first_unknown;
}
static void run(const Mesh& m,const char* label){
    for(int regime=0;regime<3;++regime){
        auto q=m.p;
        for(auto& p:q){const auto x=p[0],y=p[1],z=p[2];
            if(regime==1){p[0]=x+.125*y;p[1]=y+.0625*z;}
            if(regime==2){p[0]=x+.001*y*z;p[1]=y+.001*x*z;p[2]=z+.001*x*y;}
        }
        const auto truth=audit_exact_swept_volume(m.p,q,m.cells,3,true);
        need(truth.certified,"invalid source fixture");
        for(int block=-1;block<3;++block){
            const bool reverse=block%2==1;
            const std::array<bool,4> arms{reverse,!reverse,!reverse,reverse};
            for(int slot=0;slot<4;++slot){
                const bool factored=arms[slot];
                const auto start=Clock::now();
                const auto r=audit_exact_swept_volume(m.p,q,m.cells,3,false,factored);
                const auto end=Clock::now();
                const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
                need(ns>0&&same(r,truth),"paid comparison changed exact decisions");
                if(block<0)continue;
                std::cout<<label<<','<<regime<<','<<block<<','<<slot<<','<<(factored?"factored":"reference")<<','
                         <<r.cells<<','<<r.pair_tests<<','<<r.overlap_tests<<','<<r.polynomials<<','<<ns<<'\n';
            }
        }
    }
}
int main(){
    std::cout<<"fixture,regime,block,slot,arm,cells,pairs,overlap_tests,polynomials,elapsed_ns\n";
    run(torus(8,2,2),"torus8");run(u_solid(1),"U1");run(u_solid(4),"U4");
}

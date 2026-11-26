// Reuse the standalone source fixture patterns; these are prescribed
// geometric deformations, not native FEM trajectories.
#define KYOTO_FIXTURE_PATTERNS_ONLY
#include "volume_boundary_fixture_tests.cpp"
#undef KYOTO_FIXTURE_PATTERNS_ONLY
#include "kyoto/exact_swept_volume_audit.hpp"
static void run(const Mesh& m,const char* label){
    for(int regime=0;regime<3;++regime){
        auto q=m.p;
        for(auto& p:q){const auto x=p[0],y=p[1],z=p[2];
            if(regime==1){p[0]=x+.125*y;p[1]=y+.0625*z;}
            if(regime==2){p[0]=x+.001*y*z;p[1]=y+.001*x*z;p[2]=z+.001*x*y;}
        }
        const auto r=audit_exact_swept_volume(m.p,q,m.cells,3);
        const auto e=audit_exact_swept_volume(m.p,q,m.cells,3,true);
        need(r.certified==e.certified&&r.cells==e.cells&&r.orientation_unknown==e.orientation_unknown&&
             r.pair_tests==e.pair_tests&&r.aabb_pruned==e.aabb_pruned&&r.tested==e.tested&&r.unknown==e.unknown&&
             r.pair_nodes==e.pair_nodes&&r.axes==e.axes&&r.polynomials==e.polynomials&&
             r.first_unknown==e.first_unknown&&r.overlap_tests<=e.overlap_tests,
             "optimized whole-volume audit diverged from exhaustive");
        std::cout<<label<<','<<regime<<','<<r.cells<<','<<r.pair_tests<<','<<r.aabb_pruned<<','<<r.orientation_unknown;
        for(auto n:r.tested)std::cout<<','<<n;
        for(auto n:r.unknown)std::cout<<','<<n;
        std::cout<<','<<r.certified<<','<<r.overlap_tests<<','<<e.overlap_tests<<'\n';
        need(r.certified,"valid whole-mesh path unresolved");
    }
}
int main(){
    std::cout<<"fixture,regime,cells,pairs,pruned,orientation_unknown,test0,test1,test2,test3,unknown0,unknown1,unknown2,unknown3,certified,sweep_overlap_tests,exhaustive_overlap_tests\n";
    run(torus(8,2,2),"torus8");run(u_solid(1),"U1");run(u_solid(4),"U4");
    auto m=u_solid(1);auto q=m.p;
    const auto& cell=m.cells.front();std::swap(q[cell[0]],q[cell[1]]);
    need(!audit_exact_swept_volume(m.p,q,m.cells,2).certified,"inversion certified");
}

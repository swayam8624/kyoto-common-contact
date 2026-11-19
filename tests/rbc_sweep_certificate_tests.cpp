// Stage-2 indexed RBC versus INDEPENDENT exhaustive reach oracle and
// Stage-0 naive certificate. This is correctness and actual indexing work
// accounting, never a claim of benchmark superiority.
#include "ccp/rbc/sweep_certificate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::rbc::Reach;
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
Bounds box(int id,double x,double y,double z,double half=.12) {
    Bounds b{};
    b.id=id;
    b.min={x-half,y-half,z-half};
    b.max={x+half,y+half,z+half};
    b.category=1;
    b.mask=~std::uint32_t(0);
    return b;
}
void move(Bounds& b,double x,double y,double z) {
    b.min.x+=x;b.max.x+=x;
    b.min.y+=y;b.max.y+=y;
    b.min.z+=z;b.max.z+=z;
}
void check(const std::vector<Bounds>& b,
           const std::vector<Reach>& r,
           ccp::rbc::CertifiedSweepIndex& sweep,
           ccp::rbc::CertifiedReferenceList& naive,
           ccp::live::Vec3 translation={}) {
    auto observed=sweep.query_translated(b,translation);
    auto reference=naive.query_translated(b,translation);
    auto truth=ccp::rbc::exhaustive(b,r);
    require(observed.pairs==truth,"indexed RBC missed current reach pairs");
    require(reference.pairs==truth,"naive RBC reference lost a pair");
    require(sweep.potential_pairs()==naive.potential_pairs(),
            "indexed and naive reference potential pair counts differ");
}
void random_cohorts() {
    std::mt19937_64 rng(1849293);
    std::uniform_real_distribution<double> coord(-10.,10.);
    std::uniform_real_distribution<double> drift(-.18,.18);
    std::uniform_real_distribution<double> radius(0,.22);
    for(int scene=0;scene<40;++scene) {
        int n=32+(scene%4)*12;
        std::vector<Bounds> boxes;boxes.reserve(std::size_t(n));
        std::vector<Reach> reaches;reaches.reserve(std::size_t(n));
        for(int i=0;i<n;++i) {
            boxes.push_back(box(10000+i,coord(rng),coord(rng),coord(rng),
                                i%17==0?0.:.08+.02*(i%8)));
            reaches.push_back({radius(rng),.08+.05*(i%5)});
        }
        ccp::rbc::CertifiedSweepIndex sweep;
        ccp::rbc::CertifiedReferenceList oracle;
        sweep.initialize(boxes,reaches);
        oracle.initialize(boxes,reaches);
        check(boxes,reaches,sweep,oracle);
        for(int frame=0;frame<56;++frame) {
            for(std::size_t i=0;i<boxes.size();++i) {
                move(boxes[i],drift(rng),drift(rng),drift(rng));
                if((frame+int(i))%39==0) {
                    boxes[i].mask=boxes[i].mask?0:~std::uint32_t(0);
                }
                if((frame+int(i))%47==0) {
                    boxes[i].category=boxes[i].category?0:1;
                }
            }
            check(boxes,reaches,sweep,oracle);
        }
    }
}

void coherent_motion() {
    std::vector<Bounds> boxes={box(12,0,0,0),box(7,1,0,0),
                               box(45,0,.1,0),box(25,4,0,0)};
    std::vector<Reach> reach(4,{.01,.02});
    ccp::rbc::CertifiedSweepIndex sweep;
    ccp::rbc::CertifiedReferenceList naive;
    sweep.initialize(boxes,reach);
    naive.initialize(boxes,reach);
    const std::size_t original=sweep.metrics().reference_builds;
    for(int i=1;i<500;++i) {
        for(auto& b:boxes)move(b,3.25,-1.5,2.75);
        const ccp::live::Vec3 shift{3.25*i,-1.5*i,2.75*i};
        check(boxes,reach,sweep,naive,shift);
    }
    require(sweep.metrics().reference_builds==original,
            "shared translation unexpectedly rebuilt reference index");
    // Lying hint: move one object into contact without updating translation.
    move(boxes[3],-4.0,-.1,0);
    check(boxes,reach,sweep,naive,{3.25*499,-1.5*499,2.75*499});
    require(sweep.metrics().reference_breaks==1,
            "untrustworthy shift missed mandatory rebuild");
}
void dense_and_sparse_complexity_controls() {
    std::vector<Bounds> sparse;
    std::vector<Reach> r;
    for(int i=0;i<2000;++i){
        sparse.push_back(box(i,i*10.,0,0,.1));
        r.push_back({0,.02});
    }
    ccp::rbc::CertifiedSweepIndex index;
    index.initialize(sparse,r);
    require(index.potential_pairs()==0,"sparse grid should have no potentials");
    require(index.metrics().x_sweep_tests==0,
            "sparse scene unnecessarily emitted active x-overlap tests");
    require(index.query(sparse).pairs.empty(),
            "sparse scene produced spurious pairs");
    std::vector<Bounds> dense;
    for(int i=0;i<64;++i)dense.push_back(box(i,0,0,0,.1));
    ccp::rbc::CertifiedSweepIndex all;
    all.initialize(dense,std::vector<Reach>(64,{0,.01}));
    require(all.potential_pairs()==64*63/2,
            "fully overlapping output lower bound violated");
    require(all.query(dense).pairs.size()==64*63/2,
            "dense output omitted required pairs");
}
void tangency_and_invalid_order() {
    std::vector<Bounds> b={box(20,0,0,0,.5),box(1,2,0,0,.5)};
    std::vector<Reach> r={{0,.5},{0,.5}};
    ccp::rbc::CertifiedSweepIndex indexed;
    ccp::rbc::CertifiedReferenceList baseline;
    indexed.initialize(b,r);
    baseline.initialize(b,r);
    require(indexed.potential_pairs()==1,"inclusive potential boundary missed");
    move(b[0],.5,0,0);move(b[1],-.5,0,0);
    check(b,r,indexed,baseline);
    const auto value=indexed.query(b);
    require(value.pairs.size()==1 && value.pairs.front().a==1,
            "indexed touching ID canonicalization incorrect");
    std::swap(b[0],b[1]);
    bool rejected=false;
    try{(void)indexed.query(b);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"indexed certificate did not reject input ID reorder");
}
}
int main() {
    try {
        random_cohorts();
        coherent_motion();
        dense_and_sparse_complexity_controls();
        tangency_and_invalid_order();
        std::cout<<"PASS RBC indexed sweep: 40 independently randomized cohorts,"
                 <<" 2240 evolving frames, same pair IDs as reference and oracle;"
                 <<" 499 coherent translations, 2000 sparse and 64 dense primitives\n";
    } catch (const std::exception& e) {
        std::cerr<<"FAIL RBC indexed sweep: "<<e.what()<<"\n";
        return 1;
    }
}

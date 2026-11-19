// RBC independent Stage-2B validation, keeping Stage-2A node-set
// implementation as an explicit historical NEGATIVE control.
// Fails unless both produce the exact Stage-0 brute-force pair IDs.
#include "ccp/rbc/certified_flat_frontier.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::rbc::Reach;
void require(bool ok,const char* why){
    if(!ok)throw std::runtime_error(why);
}
Bounds box(int id,double x,double y,double z,double size){
    Bounds b{};
    b.id=id;
    b.min={x-size,y-size,z-size};
    b.max={x+size,y+size,z+size};
    b.category=1;b.mask=1;b.movable=true;
    return b;
}
void move(Bounds& b,double x,double y,double z){
    b.min.x+=x;b.max.x+=x;
    b.min.y+=y;b.max.y+=y;
    b.min.z+=z;b.max.z+=z;
}
void test_independent_references() {
    std::mt19937_64 rng(0xabcdead55);
    std::uniform_real_distribution<double> pos(-5,5),delta(-.12,.12);
    std::uniform_real_distribution<double> radius(0.,.11);
    std::size_t local=0,bulk=0,candidates=0;
    for(int scene=0;scene<36;++scene){
        const int n=(scene%6==0?128:64);
        std::vector<Bounds> boxes;
        std::vector<Reach> reach;
        for(int i=0;i<n;++i){
            const double density=scene%2==0?.3:2.0;
            boxes.push_back(box(1000+i*3,pos(rng)*density,
                        pos(rng)*density,pos(rng)*density,.04+(i%5)*.03));
            reach.push_back({radius(rng),.065});
        }
        ccp::rbc::CertifiedSpatialFrontier original;
        ccp::rbc::CertifiedFlatFrontier flat;
        original.initialize(boxes,reach);
        flat.initialize(boxes,reach);
        for(int f=0;f<105;++f){
            for(int i=0;i<n;++i){
                move(boxes[i],delta(rng),delta(rng),delta(rng));
                if((i+f)%13==0)
                    boxes[i].mask=boxes[i].mask?0u:1u;
            }
            auto a=original.query(boxes);
            auto b=flat.query(boxes);
            const auto truth=ccp::rbc::exhaustive(boxes,reach);
            require(a.pairs==truth,
                    "historical Stage-2A indexed reference pair mismatch");
            require(b.pairs==truth,
                    "NEW flat Stage-2B failed independent reference oracle");
            require(b.pairs==a.pairs,
                    "new flat and original node-set reference differ");
            local+=b.local_repairs;
            bulk+=int(b.global_rebuilt);
            candidates+=b.pairs.size();
        }
    }
    require(local>0&&bulk>0&&candidates>0,
            "flat property test did not exercise all code branches");
    std::cout<<"PASS RBC 36x105 all-frame oracle crosscheck, "
             <<"repairs="<<local<<" bulk="<<bulk
             <<" candidates="<<candidates<<"\n";
}
void test_motion_and_id_fail_closed(){
    std::vector<Bounds> boxes={box(3,-1,0,0,.1),box(8,1,0,0,.1)};
    std::vector<Reach> r{{0.,.01},{0.,.01}};
    ccp::rbc::CertifiedFlatFrontier flat;
    flat.initialize(boxes,r);
    require(flat.potential_pairs()==0,"separate ref initial pairing wrong");
    for(int f=1;f<=30;++f){
        for(auto& b:boxes)move(b,33.,-2.,5.);
        auto result=flat.query_translated(boxes,{f*33.,f*-2.,f*5.});
        require(!result.global_rebuilt&&result.local_repairs==0,
                "flat did not reuse certified shared translation");
    }
    require(flat.global_builds()==1,
            "co-translation incorrectly expired certificate");
    move(boxes[1],-2.,0.,0.);
    auto result=flat.query_translated(boxes,{990.,-60.,150.});
    require(result.pairs==ccp::rbc::exhaustive(boxes,r),
            "false group-motion hint caused flat candidate omission");
    require(result.local_repairs==1,
            "flat did not repair only the relative moved primitive");
    std::swap(boxes[0],boxes[1]);
    bool rejected=false;
    try{(void)flat.query(boxes);}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"flat accepted changed primitive ordering");
    boxes[0].min.x=std::numeric_limits<double>::quiet_NaN();
    rejected=false;
    try{flat.initialize(boxes,r);}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"flat accepted NaN identity geometry");
}
void test_all_overlap_and_zero_skin(){
    std::vector<Bounds> boxes;
    std::vector<Reach> reach;
    for(int i=0;i<120;++i){
        boxes.push_back(box(i,0,0,0,.5));
        reach.push_back({0.,0.});
    }
    ccp::rbc::CertifiedFlatFrontier flat;
    flat.initialize(boxes,reach);
    require(flat.potential_pairs()==120*119/2,
            "all-overlap algorithm falsely claims sub-quadratic potential pairs");
    auto q=flat.query(boxes);
    require(q.pairs.size()==120*119/2,
            "all-overlap exact pair output missing");
    for(int i=0;i<120;++i)move(boxes[i],i*.005,0,0);
    q=flat.query(boxes);
    require(q.pairs==ccp::rbc::exhaustive(boxes,reach),
            "all-overlap incremental bulk update pair mismatch");
}
} // namespace

int main(){
    try {
        test_independent_references();
        test_motion_and_id_fail_closed();
        test_all_overlap_and_zero_skin();
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL RBC flat indexed frontier: "<<e.what()<<"\n";
        return 1;
    }
}

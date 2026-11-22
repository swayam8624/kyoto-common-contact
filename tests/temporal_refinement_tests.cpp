#include "kyoto/temporal_refinement.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace kyoto::contact;
using namespace kyoto::contact::temporal;
static void must(bool ok,const std::string& msg) {
    if(!ok)throw std::runtime_error(msg);
}
static MovingTriangle hump(std::uint64_t id,double a,double x=0) {
    MovingTriangle t;t.id=id;
    for(int k=0;k<3;k++){
        const double y=k==1?.5:0,z=k==2?.5:0;
        t.vertices[k].control={{{x,y,z},{x+a,y,z},
                                {x+a,y,z},{x,y,z}}};
    }
    return t;
}
static MovingTriangle translate(std::uint64_t id,double begin,double end) {
    MovingTriangle t;t.id=id;
    for(int v=0;v<3;v++){
        const double y=v==1?.5:0,z=v==2?.5:0;
        for(int k=0;k<4;k++)
            t.vertices[v].control[k]={begin+(end-begin)*k/3.,y,z};
    }
    return t;
}
static void check_containment(const MovingTriangle& tri,unsigned depth) {
    std::vector<IntervalTriangle> slabs={initial(tri)};
    for(unsigned level=0;level<depth;level++){
        std::vector<IntervalTriangle> next;
        for(const auto& p:slabs){
            auto [l,r]=bisect(p);
            next.push_back(l);next.push_back(r);
        }
        slabs=std::move(next);
    }
    for(std::size_t i=0;i<slabs.size();i++){
        const auto bound=conservative_box(slabs[i]);
        for(int step=0;step<=40;step++) {
            const double t=(double(i)+double(step)/40.)/double(slabs.size());
            for(const auto& v:tri.vertices){
                const auto x=evaluate(v,t);
                must(bound.min.x<=x.x&&x.x<=bound.max.x&&
                     bound.min.y<=x.y&&x.y<=bound.max.y&&
                     bound.min.z<=x.z&&x.z<=bound.max.z,
                     "sampled real path outside interval enclosure");
            }
        }
    }
}
int main(){
    try{
        const auto moving=hump(11,4),target=hump(19,0,3);
        for(unsigned depth=0;depth<=6;depth++){
            const auto by_sap=query({moving,target},depth);
            const auto by_exhaustive=query({moving,target},depth,true);
            const auto selected=selective_query({moving,target},depth);
            must(by_sap.unique_pairs==std::vector<Pair>{{11,19}},
                 "true mid-step triangle overlap missing");
            must(by_sap.unique_pairs==by_exhaustive.unique_pairs,
                 "SAP/exhaustive mismatch");
            must(by_sap.unique_pairs==selected.unique_pairs,
                 "global/selective mismatch");
        }
        const auto distant=hump(20,0,3.5);
        must(query({moving,distant},0).unique_pairs.size()==1,
             "coarse false positive fixture broken");
        for(unsigned depth=1;depth<=6;depth++){
            must(query({moving,distant},depth).unique_pairs.empty(),
                 "global refinement failed false positive");
            must(selective_query({moving,distant},depth).unique_pairs.empty(),
                 "selective refinement failed false positive");
        }
        const auto a=translate(40,0,4),b=translate(41,.8,4.8);
        must(query({a,b},0).unique_pairs.size()==1,
             "temporal alias fixture broken");
        for(unsigned depth=3;depth<=6;depth++){
            must(query({a,b},depth).unique_pairs.empty(),
                 "same-time global culling failed");
            must(selective_query({a,b},depth).unique_pairs.empty(),
                 "same-time selective culling failed");
        }
        std::mt19937_64 rng(20261123);
        std::uniform_real_distribution<double> pos(-4,4);
        std::uniform_real_distribution<double> time(.01,.99);
        std::uniform_real_distribution<double> thick(0,.2);
        for(int scene=0;scene<96;scene++){
            MovingTriangle moving_random;
            moving_random.id=100;
            moving_random.thickness=thick(rng);
            for(auto& v:moving_random.vertices)
                for(auto& control:v.control)
                    control={pos(rng),pos(rng),pos(rng)};
            for(unsigned d=0;d<=3;d++)
                check_containment(moving_random,d);
            MovingTriangle stationary;
            stationary.id=101;
            const double t=time(rng);
            for(int v=0;v<3;v++){
                const auto point=evaluate(moving_random.vertices[v],t);
                for(auto& control:stationary.vertices[v].control)
                    control=point;
            }
            for(unsigned depth=0;depth<=4;depth++){
                const std::vector<MovingTriangle> objects={moving_random,stationary};
                const auto r=query(objects,depth);
                must(r.unique_pairs==query(objects,depth,true).unique_pairs,
                     "independent exhaustive mismatch");
                must(r.unique_pairs==selective_query(objects,depth).unique_pairs,
                     "selective disagreement");
                must(r.unique_pairs==std::vector<Pair>{{100,101}},
                     "constructed true contact missing");
            }
        }
        {
            auto blocked=moving;blocked.mask=0;
            must(selective_query({blocked,target},3).unique_pairs.empty(),
                 "bidirectional mask ignored");
        }
        {
            bool thrown=false;
            try{(void)selective_query({moving,moving},3);}
            catch(const std::invalid_argument&){thrown=true;}
            must(thrown,"duplicate stable ID allowed");
        }
        {
            bool thrown=false;
            auto invalid=moving;
            invalid.vertices[0].control[2].x=
                std::numeric_limits<double>::quiet_NaN();
            try{(void)query({invalid},2);}
            catch(const std::invalid_argument&){thrown=true;}
            must(thrown,"NaN accepted");
        }
        {
            bool thrown=false;
            try{(void)query({moving},7);}
            catch(const std::invalid_argument&){thrown=true;}
            must(thrown,"unsupported depth accepted");
        }
        std::cout<<"PASS: 96 randomized same-time triangle contact scenes; "
                    "0 lost analytic contacts; exact per-slab SAP/exhaustive parity; "
                    "selective/global parity; two false-positive families.\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}

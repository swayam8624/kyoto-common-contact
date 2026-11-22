#include "kyoto/contact_candidates.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace kyoto::contact;
static void check(bool ok,const std::string& msg) {
    if(!ok) throw std::runtime_error(msg);
}
static Aabb box(double x,double y,double z,double r=0.2) {
    return {{x-r,y-r,z-r},{x+r,y+r,z+r}};
}
static void parity(const std::vector<Primitive>& p,const std::string& name) {
    const auto truth=exhaustive(p), actual=sweep_and_prune(p);
    check(truth==actual,name+": candidate parity failure");
    check(std::adjacent_find(actual.begin(),actual.end())==actual.end(),
          name+": duplicate pair");
}
int main() {
    try {
        parity({},"empty");
        parity({{1,box(-4,0,0),box(4,0,0)},
                {2,box(4,0,0),box(-4,0,0)}},"fast crossings");
        parity({{8,box(0,0,0,0),box(0,0,0,0)},
                {3,box(0,0,0,0),box(0,0,0,0)}},"degenerate touch");
        parity({{1,box(0,0,0),box(0,0,0)},
                {2,box(0,0,6),box(0,0,6)}},"z rejection");
        parity({{1,box(0,0,0),box(0,0,0),1,0},
                {2,box(0,0,0),box(0,0,0)}},"mask rejection");
        check(exhaustive({{9,box(0,0,0),box(0,0,0)},
                         {3,box(0.4,0,0),box(0.4,0,0)}})
              ==std::vector<Pair>{{3,9}},"inclusive face touching");
        bool caught=false;
        try {(void)exhaustive({{1,box(0,0,0),box(0,0,0)},
                               {1,box(4,0,0),box(4,0,0)}});}
        catch(const std::invalid_argument&){caught=true;}
        check(caught,"duplicate ids must fail");
        caught=false;
        try {(void)sweep_and_prune({{1,{{3,0,0},{2,1,1}},box(0,0,0)}});}
        catch(const std::invalid_argument&){caught=true;}
        check(caught,"invalid AABB must fail");
        std::mt19937_64 rng(20261123);
        std::uniform_real_distribution<double> coord(-10,10),delta(-6,6),
                                               radius(0,2);
        for(int s=0;s<240;s++) {
            std::vector<Primitive> p;
            for(int i=0;i<s%65;i++) {
                const double x=coord(rng),y=coord(rng),z=coord(rng);
                const double r=radius(rng);
                const std::uint64_t c=i%3==0?2:1;
                const std::uint64_t m=i%11==0?1:~std::uint64_t{0};
                p.push_back({static_cast<std::uint64_t>(i+1000),
                    box(x,y,z,r),box(x+delta(rng),y+delta(rng),
                                    z+delta(rng),r),c,m});
            }
            if(s%3==0) std::reverse(p.begin(),p.end());
            parity(p,"seeded scene "+std::to_string(s));
        }
        std::cout<<"PASS 240 deterministic scenarios + adversarial cases; "
                    "only swept-AABB candidate parity, not CCD\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}

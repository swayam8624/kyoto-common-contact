#include "kyoto/exact_swept_cell_candidates.hpp"
#include <iostream>
#include <random>
using namespace kyoto::geometry;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(){
    const std::vector<SweptCellBox> touching{{{0,0,0},{1,1,1}},{{1,1,1},{2,2,2}},{{2,0,0},{3,1,1}}};
    need(swept_cell_candidates(touching).pairs==std::vector<SweptCellPair>{{0,1},{1,2}},"inclusive touching failed");
    std::mt19937_64 rng(3802029);std::uniform_int_distribution<int> p(-20,20),w(0,8);
    std::size_t exhaustive_tests=0,sweep_tests=0,matched=0;
    for(int n=0;n<1000;++n){
        std::vector<SweptCellBox> boxes;
        for(int i=0;i<n%97;++i){
            SweptCellBox b;
            for(int j=0;j<3;++j){b.lo[j]=p(rng);b.hi[j]=b.lo[j]+w(rng);}
            boxes.push_back(b);
        }
        const auto a=swept_cell_candidates(boxes,true),b=swept_cell_candidates(boxes);
        need(a.pairs==b.pairs,"candidate mismatch");
        need(b.overlap_tests<=a.overlap_tests,"sweep made extra pair tests");
        exhaustive_tests+=a.overlap_tests;sweep_tests+=b.overlap_tests;matched+=a.pairs.size();
    }
    std::vector<SweptCellBox> sparse;
    for(int i=0;i<4096;++i)sparse.push_back({{0.,double(2*i),0.},{1.,double(2*i+1),1.}});
    const auto s=swept_cell_candidates(sparse);
    need(s.pairs.empty()&&s.overlap_tests==0,"axis selection failed sparse y fixture");
    bool rejected=false;try{(void)swept_cell_candidates({{{1,0,0},{0,1,1}}});}catch(const std::invalid_argument&){rejected=true;}
    need(rejected,"invalid bounds accepted");
    std::cout<<"PASS swept cell candidates: scenes=1000 matched_pairs="<<matched<<" exhaustive_tests="<<exhaustive_tests<<" sweep_tests="<<sweep_tests<<"; sparse4096 tested=0\n";
}

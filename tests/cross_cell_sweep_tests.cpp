#include "kyoto/cross_cell_sweep.hpp"
#include <iostream>
#include <random>
using namespace kyoto::geometry;
static void need(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
int main(){
    std::mt19937_64 rng(3924091);std::uniform_int_distribution<int> d(-32,32),w(0,12);
    std::size_t pairs=0,tests=0,exhaustive=0;
    for(int k=0;k<2000;++k){std::vector<SweptCellBox> a,b;
        for(auto* v:{&a,&b})for(unsigned i=0,n=unsigned(rng()%97);i<n;++i){SweptCellBox q;for(int j=0;j<3;++j){q.lo[j]=d(rng);q.hi[j]=q.lo[j]+w(rng);}v->push_back(q);}
        std::vector<SweptCellPair> truth;
        for(std::size_t i=0;i<a.size();++i)for(std::size_t j=0;j<b.size();++j)if(swept_cell_overlap(a[i],b[j]))truth.push_back({i,j});
        const auto r=cross_cell_candidates(a,b);need(r.pairs==truth,"cross sweep mismatch");pairs+=r.pairs.size();tests+=r.overlap_tests;exhaustive+=a.size()*b.size();
    }
    const auto touching=cross_cell_candidates({{{0,0,0},{1,1,1}},{{1,1,1},{1,1,1}}},{{{1,1,1},{2,2,2}}});
    need(touching.pairs==std::vector<SweptCellPair>{{0,0},{1,0}},"touching boxes lost");
    bool invalid=false;try{(void)cross_cell_candidates({{{0,0,1},{1,1,0}}},{});}catch(const std::invalid_argument&){invalid=true;}need(invalid,"invalid box accepted");
    std::cout<<"PASS 2000 random bipartite scenes pairs="<<pairs<<" overlap_tests="<<tests<<" exhaustive="<<exhaustive<<'\n';
}

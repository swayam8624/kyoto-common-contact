// Deterministic policy-only experiment. No geometry or physical timings.
#include "kyoto/policy_signature.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>
using kyoto::contact::atlas::PolicySignature;
using U=std::uint64_t;
using Leaf=std::pair<U,U>;
template<class A> PolicySignature fold(const A& a){
    PolicySignature s;
    for(auto [c,m]:a)s=PolicySignature::unite(s,PolicySignature::leaf(c,m));
    return s;
}
int main(){
    std::mt19937_64 rng(812029);
    const std::array<Leaf,2> a{{{1,2},{4,8}}},b{{{2,4},{8,1}}};
    const auto x=fold(a),y=fold(b);
    if(!(x.categories&y.masks)||!(y.categories&x.masks)||x.may_collide(y))
        throw std::runtime_error("correlation witness failed");
    std::cout<<"regime,products,oracle_compatible,union_reject,relation_reject,unsafe_reject\n";
    for(int regime=0;regime<5;++regime){
        std::size_t truth_n=0,union_n=0,relation_n=0,unsafe=0;
        for(int t=0;t<50000;++t){
            std::array<Leaf,4> left{},right{};
            const bool low=regime!=4;
            for(auto* side:{&left,&right}) for(auto& [c,m]:*side){
                c=regime==3?rng()&255u:((rng()%3==0)?0:U{1}<<(rng()%(low?8:64)));
                if(regime==0)m=U{1}<<(rng()%8);
                else if(regime==1)m=(U{1}<<(rng()%8))|(U{1}<<(rng()%8));
                else if(regime==2)m=rng()&255u;
                else m=rng();
            }
            bool truth=false;
            for(auto [c,m]:left)for(auto [d,n]:right)
                truth|=(c&n)!=0&&(d&m)!=0;
            const auto p=fold(left),q=fold(right);
            const bool union_allows=(p.categories&q.masks)&&(q.categories&p.masks);
            const bool got=p.may_collide(q);
            truth_n+=truth;union_n+=!union_allows;relation_n+=!got;unsafe+=truth&&!got;
            if(truth&&!got)throw std::runtime_error("unsafe certificate");
            if(low&&truth!=got)throw std::runtime_error("inexact low-bit relation");
        }
        std::cout<<regime<<','<<50000<<','<<truth_n<<','<<union_n<<','<<relation_n<<','<<unsafe<<'\n';
    }
}

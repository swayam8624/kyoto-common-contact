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
    std::size_t exact=0,high=0,extra=0;
    // Union-only false positive: A has i->j and k->l, B has
    // j->k and l->i; no mutually accepting individual pair.
    const std::array<Leaf,2> a{{{1,2},{4,8}}},b{{{2,4},{8,1}}};
    const auto x=fold(a),y=fold(b);
    if(!(x.categories&y.masks)||!(y.categories&x.masks)||x.may_collide(y))
        throw std::runtime_error("correlation witness failed");
    for(int t=0;t<100000;++t){
        std::array<Leaf,4> left{},right{};
        const bool low=t%2==0;
        for(auto* side:{&left,&right}){
            for(auto& [c,m]:*side){
                c=(rng()%3==0)?0:U{1}<<(rng()%(low?8:64));
                m=rng()%2?U{1}<<(rng()%(low?8:64)):rng()&(low?255:~U{0});
            }
        }
        bool truth=false;
        for(auto [c,m]:left)for(auto [d,n]:right)
            truth|=(c&n)!=0&&(d&m)!=0;
        const auto p=fold(left),q=fold(right);
        const bool got=p.may_collide(q);
        if(truth&&!got)throw std::runtime_error("unsafe certificate");
        if(low&&truth!=got)throw std::runtime_error("inexact low-bit relation");
        if(low)++exact;else ++high;
        if(!truth&&!got&&(p.categories&q.masks)&&(q.categories&p.masks))++extra;
    }
    std::cout<<"policy relation exact low-bit products="<<exact
             <<" safe general 64-bit products="<<high
             <<" extra rejections beyond OR="<<extra<<"\n";
}

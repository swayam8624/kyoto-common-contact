#include "kyoto/external_candidate_filter.hpp"
#include <iostream>
#include <random>
namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void add(f::Mesh& m,Vec3 p,Vec3 q){m.vertices.push_back({m.vertices.size(),{{p,p,q,q}}});}
static void check(const f::Mesh& m){
    const auto built=f::build(m);const auto truth=f::exhaustive(built);
    std::vector<f::Candidate> foreign;
    for(std::size_t i=0;i<m.vertices.size();++i)for(std::size_t j=0;j<m.faces.size();++j)
        foreign.push_back({f::Kind::VertexFace,i,j});
    for(std::size_t i=0;i<m.edges.size();++i)for(std::size_t j=0;j<m.edges.size();++j)
        foreign.push_back({f::Kind::EdgeEdge,i,j});
    const auto r=f::filter_dense_external_candidates(built,foreign);
    need(r.pairs==truth.pairs,"external overcoverage did not normalize to exhaustive truth");
    if(!truth.pairs.empty()){
        auto missing=truth.pairs;missing.pop_back();
        need(f::filter_dense_external_candidates(built,missing).pairs!=truth.pairs,"filter fabricated missing candidate");
    }
    bool rejected=false;
    try{(void)f::filter_dense_external_candidates(built,{{f::Kind::VertexFace,m.vertices.size(),0}});}
    catch(const std::invalid_argument&){rejected=true;}need(rejected,"bad external index accepted");
    rejected=false;
    try{(void)f::filter_dense_external_candidates(built,{{static_cast<f::Kind>(99),0,0}});}
    catch(const std::invalid_argument&){rejected=true;}need(rejected,"bad external kind accepted");
}
int main(){
    // At 2^24, gaps below a float ULP can be conservative LBVH hits.
    // The original binary64 predicate must still reject disjoint boxes.
    f::Mesh m;const double x=16777216.;
    add(m,{x,0,0},{x,0,0});add(m,{x,.25,0},{x,.25,0});add(m,{x,0,.25},{x,0,.25});
    add(m,{x+.125,.125,.125},{x+.125,.125,.125});add(m,{x,.125,.125},{x,.125,.125});
    m.faces={{0,{0,1,2}}};m.edges={{0,{0,1}},{1,{3,4}},{2,{1,2}}};
    const auto built=f::build(m);const auto truth=f::exhaustive(built).pairs;
    need(std::find(truth.begin(),truth.end(),f::Candidate{f::Kind::VertexFace,3,0})==truth.end(),"disjoint precision control not disjoint");
    need(std::find(truth.begin(),truth.end(),f::Candidate{f::Kind::VertexFace,4,0})!=truth.end(),"touching precision control absent");
    check(m);
    std::mt19937_64 rng(3919126);std::uniform_int_distribution<int> d(-8,8);
    for(int trial=0;trial<1000;++trial){
        f::Mesh q;q.thickness=(trial%2)*.125;
        auto point=[&](){return Vec3{d(rng)/8.,d(rng)/8.,d(rng)/8.};};
        for(int i=0;i<12;++i)add(q,point(),point());
        for(std::size_t i=0;i<12;i+=3)q.faces.push_back({q.faces.size(),{i,i+1,i+2}});
        for(std::size_t i=0;i<12;i+=2)q.edges.push_back({q.edges.size(),{i,i+1}});
        if(trial%3==0){q.vertices[0].mask=0;q.edges[1].mask=0;}
        check(q);
    }
    std::cout<<"PASS 1000 external overcoverage scenes, float precision, malformed input and missing-candidate controls\n";
}

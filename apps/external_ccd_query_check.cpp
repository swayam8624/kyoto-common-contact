#include "kyoto/fused_axis_sap.hpp"
#include <fstream>
#include <iostream>
#include <string>
namespace f=kyoto::contact::features;
using Vec=kyoto::contact::Vec3;
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("usage: query_check data");
    std::ifstream in(argv[1]);if(!in)throw std::runtime_error("input");
    std::uint64_t id;std::string kind;int yes,overlap;
    std::cout<<"query_id,kind,published_positive,rational_aabb,fused\n";
    while(in>>id>>kind>>yes>>overlap){
        std::array<Vec,8> x;for(auto& v:x)if(!(in>>v.x>>v.y>>v.z)||!kyoto::contact::finite(v))throw std::runtime_error("bad point");
        f::Mesh m;for(unsigned i=0;i<4;++i)m.vertices.push_back({i,{{x[i],x[i],x[i+4],x[i+4]}}});
        f::Candidate expected;
        if(kind=="ee"){m.edges={{0,{0,1}},{1,{2,3}}};expected={f::Kind::EdgeEdge,0,1};}
        else if(kind=="vf"){m.faces={{0,{1,2,3}}};expected={f::Kind::VertexFace,0,0};}
        else throw std::runtime_error("kind");
        kyoto::contact::persistent::FusedAxisSweep sweep;auto z=sweep.query(m);
        if(z.pairs!=f::exhaustive(m).pairs)throw std::runtime_error("candidate parity");
        bool found=std::binary_search(z.pairs.begin(),z.pairs.end(),expected);
        std::cout<<id<<','<<kind<<','<<yes<<','<<overlap<<','<<found<<'\n';
        if((yes||overlap)&&!found)throw std::runtime_error("external positive or rational AABB missed");
    }
    if(!in.eof())throw std::runtime_error("malformed input");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

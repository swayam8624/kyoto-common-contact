// Pass 27: dependency-free solver boundary regression.
// Check dense row IDs against a direct endpoint-box oracle rather than
// another call to the maintained hierarchy. All fixtures are linear.
#include "kyoto/linear_candidate_adapter.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
using namespace kyoto::contact;
using namespace kyoto::contact::atlas;
namespace f=kyoto::contact::features;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static std::vector<f::Candidate> oracle(
    const std::vector<Vec3>& a,const std::vector<Vec3>& b,
    const std::vector<IndexedEdge>& e,const std::vector<IndexedFace>& g,
    double pad){
    std::vector<Aabb> v;
    const auto inf=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<a.size();++i){
        auto lo=[&](double x,double y){return std::nextafter(std::min(x,y)-pad,-inf);};
        auto hi=[&](double x,double y){return std::nextafter(std::max(x,y)+pad,inf);};
        v.push_back({{lo(a[i].x,b[i].x),lo(a[i].y,b[i].y),lo(a[i].z,b[i].z)},
                     {hi(a[i].x,b[i].x),hi(a[i].y,b[i].y),hi(a[i].z,b[i].z)}});
    }
    auto merge=[](Aabb x,Aabb y){
        return Aabb{{std::min(x.min.x,y.min.x),std::min(x.min.y,y.min.y),std::min(x.min.z,y.min.z)},
                    {std::max(x.max.x,y.max.x),std::max(x.max.y,y.max.y),std::max(x.max.z,y.max.z)}};
    };
    auto intersect=[](Aabb x,Aabb y){
        return x.min.x<=y.max.x&&y.min.x<=x.max.x&&
               x.min.y<=y.max.y&&y.min.y<=x.max.y&&
               x.min.z<=y.max.z&&y.min.z<=x.max.z;
    };
    std::vector<f::Candidate> out;
    for(std::size_t i=0;i<v.size();++i)for(std::size_t j=0;j<g.size();++j){
        const auto [x,y,z]=g[j];
        if(i!=x&&i!=y&&i!=z&&intersect(v[i],merge(merge(v[x],v[y]),v[z])))
            out.push_back({f::Kind::VertexFace,i,j});
    }
    for(std::size_t i=0;i<e.size();++i)for(std::size_t j=i+1;j<e.size();++j){
        const auto [x,y]=e[i];const auto [z,w]=e[j];
        if(x!=z&&x!=w&&y!=z&&y!=w&&intersect(merge(v[x],v[y]),merge(v[z],v[w])))
            out.push_back({f::Kind::EdgeEdge,i,j});
    }
    std::sort(out.begin(),out.end());
    return out;
}
int main(){
    try{
        LinearCandidateAdapter adapter;
        std::vector<Vec3> a(12),b(12);
        std::vector<IndexedEdge> edges={{0,1},{2,3},{4,5},{6,7},{8,9}};
        std::vector<IndexedFace> faces={{0,2,4},{1,3,5},{6,8,10},{7,9,11}};
        std::mt19937_64 rng(27012026);
        std::uniform_real_distribution<double> u(-1.,1.);
        std::uint64_t epoch=1;
        for(int i=0;i<240;++i){
            for(std::size_t j=0;j<a.size();++j){
                a[j]={u(rng),u(rng),u(rng)};
                b[j]={a[j].x+.25*u(rng),a[j].y+.25*u(rng),a[j].z+.25*u(rng)};
            }
            const bool changed=i==0||i%30==0||i%47==0;
            if(i!=0&&i%30==0)std::rotate(edges.begin(),edges.begin()+1,edges.end());
            if(i!=0&&i%47==0)++epoch;
            const auto before=adapter.rebuilds();
            const double pad=i%2?.02:0.;
            const auto expected=oracle(a,b,edges,faces,pad);
            require(adapter.query(a,b,edges,faces,epoch,pad).pairs==expected,"endpoint oracle mismatch");
            require(adapter.rebuilds()==before+(changed?1:0),"wrong topology invalidation");
        }
        auto bad=edges;bad[0][0]=a.size();
        bool rejected=false;
        try{(void)adapter.query(a,b,bad,faces,epoch);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"invalid index accepted");
        auto bad_a=a;bad_a[0].x=std::numeric_limits<double>::quiet_NaN();
        rejected=false;
        try{(void)adapter.query(bad_a,b,edges,faces,epoch);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"nonfinite coordinate accepted");
        require(adapter.query(a,b,edges,faces,epoch).pairs==oracle(a,b,edges,faces,0.),
                "valid query after invalid input lost parity");
        const std::vector<Vec3> empty;
        const std::vector<IndexedEdge> no_edges;
        const std::vector<IndexedFace> no_faces;
        require(adapter.query(empty,empty,no_edges,no_faces,epoch+1).pairs.empty(),
                "empty topology emitted pairs");
        std::cout<<"PASS: 240 indexed linear frames, topology changes, padding, rejected inputs, empty topology\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

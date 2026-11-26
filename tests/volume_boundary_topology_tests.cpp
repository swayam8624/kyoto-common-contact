#include "kyoto/volume_boundary_topology.hpp"
#include <iostream>
#include <random>
using namespace kyoto::geometry;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static std::vector<FaceRow> surface(const std::vector<CellRow>& cells){
    std::map<FaceRow,unsigned> counts;
    for(auto c:cells)for(int omit=0;omit<4;++omit){
        FaceRow f{};int k=0;for(int i=0;i<4;++i)if(i!=omit)f[k++]=c[i];
        std::sort(f.begin(),f.end());++counts[f];
    }
    std::vector<FaceRow> out;for(auto [f,n]:counts)if(n==1)out.push_back(f);return out;
}
static void reject(std::size_t n,const std::vector<CellRow>& cells,const std::vector<FaceRow>& faces){
    bool failed=false;try{(void)validate_volume_boundary_topology(n,cells,faces);}
    catch(const std::invalid_argument&){failed=true;}need(failed,"invalid topology accepted");
}
int main(){
    const std::vector<CellRow> single{{0,1,2,3}};
    const auto f=surface(single);const auto r=validate_volume_boundary_topology(4,single,f);
    need(r.boundary_faces==4&&r.boundary_edges==6&&r.boundary_vertices==4&&r.interior_faces==0,"single tetra receipt");
    const std::vector<CellRow> pair{{0,1,2,3},{0,2,1,4}};
    auto p=surface(pair);const auto rr=validate_volume_boundary_topology(5,pair,p);
    need(rr.boundary_faces==6&&rr.interior_faces==1&&rr.boundary_edges==9,"two tetra receipt");
    auto missing=p;missing.pop_back();reject(5,pair,missing);
    auto duplicate=p;duplicate.push_back(p[0]);reject(5,pair,duplicate);
    auto interior=p;interior.push_back({0,1,2});reject(5,pair,interior);
    reject(5,{{0,1,2,3},{0,1,2,4}},p);
    reject(4,{{0,1,2,3},{0,2,1,3}},f);
    reject(4,{{0,1,2,2}},f);
    reject(5,single,f);reject(3,single,f);
    const std::vector<CellRow> pinched{{0,1,2,3},{0,4,5,6}};
    reject(7,pinched,surface(pinched));
    const std::vector<CellRow> edge_pinched{{0,1,2,3},{0,1,4,5}};
    reject(6,edge_pinched,surface(edge_pinched));
    const std::vector<CellRow> triple{{0,1,2,3},{0,2,1,4},{0,1,2,5}};
    reject(6,triple,surface(triple));
    const std::vector<CellRow> disconnected{{0,1,2,3},{4,5,6,7}};
    need(validate_volume_boundary_topology(8,disconnected,surface(disconnected)).boundary_faces==8,"disconnected valid components");
    std::mt19937 rng(3902026);
    for(int i=0;i<1000;++i){
        std::array<std::size_t,5> ids{0,1,2,3,4};std::shuffle(ids.begin(),ids.end(),rng);
        auto cells=pair;for(auto& c:cells)for(auto& v:c)v=ids[v];
        auto faces=surface(cells);std::shuffle(faces.begin(),faces.end(),rng);
        for(auto& face:faces)std::shuffle(face.begin(),face.end(),rng);
        need(validate_volume_boundary_topology(5,cells,faces).boundary_faces==6,"renumbered oriented complex");
        faces.pop_back();reject(5,cells,faces);
    }
    std::cout<<"PASS boundary topology: 1000 renumbered valid complexes, 1000 missing-face rejections, analytic manifold/duplicate controls\n";
}

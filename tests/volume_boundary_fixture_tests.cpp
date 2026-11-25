// Standalone combinatorial compatibility test for the native mesh
// construction patterns. No FEM forces or collision dynamics are run.
#include "kyoto/volume_boundary_topology.hpp"
#include <cmath>
#include <iostream>
#include <numbers>
using namespace kyoto::geometry;
using Point=std::array<double,3>;
struct Mesh{std::vector<Point> p;std::vector<CellRow> cells;std::vector<FaceRow> boundary;};
static void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static Point sub(Point a,Point b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
static double determinant(Point a,Point b,Point c){return a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0]);}
static void tet(Mesh& m,CellRow t){
    const auto a=m.p.at(t[0]);const double j=determinant(sub(m.p.at(t[1]),a),sub(m.p.at(t[2]),a),sub(m.p.at(t[3]),a));
    need(std::isfinite(j)&&std::abs(j)>1e-12,"degenerate source fixture tetrahedron");
    if(j<0)std::swap(t[2],t[3]);
    m.cells.push_back(t);
}
static void cell(Mesh& m,std::size_t a,std::size_t b,std::size_t c,std::size_t d,
                 std::size_t e,std::size_t f,std::size_t g,std::size_t h){
    tet(m,{a,b,d,h});tet(m,{a,d,c,h});tet(m,{a,c,g,h});
    tet(m,{a,g,e,h});tet(m,{a,e,f,h});tet(m,{a,f,b,h});
}
static Mesh torus(unsigned major,unsigned radial,unsigned vertical){
    Mesh m;const auto index=[&](unsigned i,unsigned j,unsigned k){return (std::size_t(i%major)*radial+j)*vertical+k;};
    for(unsigned i=0;i<major;++i){
        const auto theta=2*std::numbers::pi*double(i)/major;
        for(unsigned j=0;j<radial;++j)for(unsigned k=0;k<vertical;++k){
            const double u=-.18+.36*j/(radial-1),v=-.18+.36*k/(vertical-1);
            m.p.push_back({(.70+u)*std::cos(theta),(.70+u)*std::sin(theta),.52+v});
        }
    }
    for(unsigned i=0;i<major;++i)for(unsigned j=0;j+1<radial;++j)for(unsigned k=0;k+1<vertical;++k)
        cell(m,index(i,j,k),index(i+1,j,k),index(i,j+1,k),index(i+1,j+1,k),
               index(i,j,k+1),index(i+1,j,k+1),index(i,j+1,k+1),index(i+1,j+1,k+1));
    const auto quad=[&](std::size_t a,std::size_t b,std::size_t c,std::size_t d){m.boundary.push_back({a,b,c});m.boundary.push_back({a,c,d});};
    for(unsigned i=0;i<major;++i){
        for(unsigned k=0;k+1<vertical;++k)for(unsigned j:{0u,radial-1})
            quad(index(i,j,k),index(i+1,j,k),index(i+1,j,k+1),index(i,j,k+1));
        for(unsigned j=0;j+1<radial;++j)for(unsigned k:{0u,vertical-1})
            quad(index(i,j,k),index(i+1,j,k),index(i+1,j+1,k),index(i,j+1,k));
    }
    return m;
}
static Mesh u_solid(unsigned width){
    Mesh m;std::map<std::array<int,3>,std::size_t> ids;
    const double xs[]={0.,.1,.1025,.2025},zs[]={0.,.1,.3};
    const auto node=[&](int i,int j,int k){
        const std::array<int,3> key{i,j,k};const auto it=ids.find(key);
        if(it!=ids.end())return it->second;
        const auto id=m.p.size();ids.emplace(key,id);m.p.push_back({xs[i],.1*double(j),zs[k]});return id;
    };
    for(int k=0;k<2;++k)for(int j=0;j<int(width);++j)for(int i=0;i<3;++i){
        if(k==1&&i==1)continue;
        const auto a=node(i,j,k),b=node(i+1,j,k),c=node(i,j+1,k),d=node(i+1,j+1,k),
                   e=node(i,j,k+1),f=node(i+1,j,k+1),g=node(i,j+1,k+1),h=node(i+1,j+1,k+1);
        cell(m,a,b,c,d,e,f,g,h);
    }
    std::map<FaceRow,unsigned> counts;
    for(const auto& t:m.cells)for(int omit=0;omit<4;++omit){
        FaceRow f{};int at=0;for(int i=0;i<4;++i)if(i!=omit)f[at++]=t[i];
        std::sort(f.begin(),f.end());++counts[f];
    }
    for(const auto& [f,n]:counts)if(n==1)m.boundary.push_back(f);
    return m;
}
int main(){
    std::size_t total_tets=0,total_meshes=0;
    for(unsigned n:{8u,20u,64u})for(unsigned r:{2u,3u,8u})for(unsigned z:{2u,3u,8u}){
        auto m=torus(n,r,z);const auto out=validate_volume_boundary_topology(m.p.size(),m.cells,m.boundary);
        need(out.boundary_faces==4*n*(r+z-2),"toroidal boundary size mismatch");
        total_tets+=out.cells;++total_meshes;
    }
    for(unsigned w:{1u,4u,16u,32u,64u}){
        auto m=u_solid(w);const auto out=validate_volume_boundary_topology(m.p.size(),m.cells,m.boundary);
        need(out.cells==30*w&&out.boundary_faces==24*w+20,"U-solid receipt mismatch");
        total_tets+=out.cells;++total_meshes;
    }
    std::cout<<"PASS boundary fixture patterns meshes="<<total_meshes<<" tetrahedra="<<total_tets<<'\n';
}

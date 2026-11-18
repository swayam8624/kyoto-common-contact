#pragma once
// Kyoto material-scene generator. Lightweight discrete mass-spring reference
// fixtures with differentiated deformation/damping, NOT calibrated FEM/MPM or
// validated continuum constitutive models. Broad-phase inputs are triangles,
// volume-cell AABBs or spring segments with stable primitive identities.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ccp::materials {
using Vec3=ccp::research::Vec3;
using Primitive=ccp::research::Primitive;
inline Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 mul(Vec3 a,double f){return {a.x*f,a.y*f,a.z*f};}
inline double length(Vec3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
struct Node {Vec3 x{},previous{};bool pin{};double damping{.985};};
struct Bond {int a{},b{};double rest{},strength{};};
struct Element {std::array<int,8> vertices{};int count{},material{};};
struct Spec {double damping{},strength{},weight{},impact{};};
inline Spec specification(int material){
 switch(material){
 case 0:return {.997,.82,.000045,.22}; // cloth / membrane
 case 1:return {.84,.34,.00003,.30};    // sponge-like soft volumetric lattice
 case 2:return {.70,.24,.00004,.45};    // cushion-like dissipative slab
 case 3:return {.990,.91,.000035,.30};  // pressurized-looking elastic surface (no pressure solver)
 case 4:return {.995,.67,.00002,.18};  // helical spring; axial/coil motion
 default:throw std::invalid_argument("unknown material");
 }
}
inline std::string materialName(int material){
 return std::array<std::string,5>{"cloth","sponge","cushion","soft_ball","spring"}.at(material);
}
inline int materialCode(const std::string& s){
 for(int i=0;i<5;++i)if(s==materialName(i))return i;
 throw std::invalid_argument("unknown material: "+s);
}
class World {
 std::vector<Node> nodes_;
 std::vector<Bond> bonds_;
 std::vector<Element> elements_;
 std::uint32_t seed_{};
 std::string regime_{"baseline"};
 int step_{};
 static constexpr double pi=3.14159265358979323846264338;
 int node(Vec3 p,bool pin, double damping){
  const int id=static_cast<int>(nodes_.size());
  nodes_.push_back({p,p,pin,damping});return id;
 }
 void bond(int a,int b,double strength){
  if(a==b)throw std::logic_error("degenerate bond");
  bonds_.push_back({a,b,length(sub(nodes_[a].x,nodes_[b].x)),strength});
 }
 void element(std::initializer_list<int> indices,int mat){
  Element e;e.count=int(indices.size());e.material=mat;
  if(e.count>8)throw std::logic_error("too many cell vertices");
  int j=0;for(int i:indices)e.vertices[j++]=i;
  elements_.push_back(e);
 }
 void cloth(int target,Vec3 center){
  const int cells=std::max(3,int(std::ceil(std::sqrt(double(target)/2.))));
  const int side=cells+1;
  std::vector<int> ids(size_t(side)*side);
  const auto s=specification(0);
  for(int y=0;y<side;++y)for(int x=0;x<side;++x){
   Vec3 p={center.x+2.0*(double(x)/cells-.5),
           center.y+2.0*(double(y)/cells-.5),
           center.z+1.5+.04*std::sin(double(x+y)*.13)};
   ids[size_t(y)*side+x]=node(p,y==cells&&(x==0||x==cells),s.damping);
  }
  for(int y=0;y<side;++y)for(int x=0;x<side;++x){
   const int a=ids[size_t(y)*side+x];
   if(x<cells)bond(a,ids[size_t(y)*side+x+1],s.strength);
   if(y<cells)bond(a,ids[size_t(y+1)*side+x],s.strength);
   if(x<cells&&y<cells){
    const int b=ids[size_t(y)*side+x+1],c=ids[size_t(y+1)*side+x],d=ids[size_t(y+1)*side+x+1];
    bond(a,d,s.strength*.45);bond(b,c,s.strength*.45);
    element({a,b,d},0);element({a,d,c},0);
   }
  }
 }
 void volume(int material,int target,Vec3 center){
  // For sponge use near-cubic lattice; cushion is a wide, shallow slab.
  const int cells=std::max(2,int(std::ceil(std::cbrt(double(target)))));
  const int zcells=material==2?std::max(2,cells/3):cells;
  const int xy=material==2?std::max(2,int(std::ceil(std::sqrt(double(target)/zcells)))):cells;
  const int nx=xy+1,ny=xy+1,nz=zcells+1;
  auto index=[&](int x,int y,int z){return (z*ny+y)*nx+x;};
  std::vector<int> ids(size_t(nx)*ny*nz);
  const auto s=specification(material);
  for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
   Vec3 p={center.x+1.5*(double(x)/xy-.5),
           center.y+1.5*(double(y)/xy-.5),
           center.z+.12+(material==2?.48:1.15)*double(z)/zcells};
   ids[index(x,y,z)]=node(p,z==0,s.damping);
  }
  for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
   int a=ids[index(x,y,z)];
   if(x<xy)bond(a,ids[index(x+1,y,z)],s.strength);
   if(y<xy)bond(a,ids[index(x,y+1,z)],s.strength);
   if(z<zcells)bond(a,ids[index(x,y,z+1)],s.strength);
   if(x<xy&&y<xy&&z<zcells)
    element({a,ids[index(x+1,y,z)],ids[index(x,y+1,z)],ids[index(x+1,y+1,z)],
             ids[index(x,y,z+1)],ids[index(x+1,y,z+1)],
             ids[index(x,y+1,z+1)],ids[index(x+1,y+1,z+1)]},material);
  }
 }
 void ball(int target,Vec3 center){
  const int rings=std::max(4,int(std::ceil(std::sqrt(double(target)/2.))));
  const int sectors=rings*2;
  const auto s=specification(3);
  std::vector<int> ids(size_t(rings+1)*(sectors+1));
  const auto idx=[&](int i,int j){return size_t(i)*(sectors+1)+j;};
  for(int i=0;i<=rings;++i)for(int j=0;j<=sectors;++j){
   const double theta=pi*double(i)/rings,phi=2*pi*double(j)/sectors;
   Vec3 p={center.x+.74*std::sin(theta)*std::cos(phi),
           center.y+.74*std::sin(theta)*std::sin(phi),
           center.z+1.05+.74*std::cos(theta)};
   ids[idx(i,j)]=node(p,i==0,s.damping);
  }
  for(int i=0;i<=rings;++i)for(int j=0;j<=sectors;++j){
   int a=ids[idx(i,j)];
   if(i<rings)bond(a,ids[idx(i+1,j)],s.strength);
   if(j<sectors)bond(a,ids[idx(i,j+1)],s.strength);
   if(i<rings&&j<sectors){
    const int b=ids[idx(i,j+1)],c=ids[idx(i+1,j)],d=ids[idx(i+1,j+1)];
    bond(a,d,.2*s.strength);
    element({a,b,d},3);element({a,d,c},3);
   }
  }
 }
 void spring(int target,Vec3 center){
  const int turns=std::max(2,int(target/18)),segments=std::max(12,turns*18);
  const auto s=specification(4);
  std::vector<int> ids(size_t(segments+1));
  for(int i=0;i<=segments;++i){
   const double t=double(i)/segments,theta=2*pi*turns*t;
   Vec3 p={center.x+.32*std::cos(theta),center.y+.32*std::sin(theta),center.z+.2+1.7*t};
   ids[i]=node(p,i==0||i==segments,s.damping);
  }
  for(int i=0;i<segments;++i){
   bond(ids[i],ids[i+1],s.strength);
   if(i+3<=segments)bond(ids[i],ids[i+3],s.strength*.06);
   element({ids[i],ids[i+1]},4);
  }
 }
public:
 World(const std::string& kind,int requested,std::uint32_t seed,std::string regime="baseline"):
  seed_(seed),regime_(std::move(regime)){
  if(regime_!="baseline"&&regime_!="impact"&&regime_!="shear"&&
     regime_!="compression"&&regime_!="rapid"&&regime_!="relax")
   throw std::invalid_argument("unknown material deformation regime");
  if(requested<16||requested>120000)throw std::invalid_argument("material element budget 16..120000");
  if(kind=="mixed"){
   const int each=std::max(16,requested/5);
   cloth(each,{-1.0,0,0});volume(1,each,{.1,0,0});
   volume(2,each,{1.0,0,0});ball(each,{-.25,.6,0});
   spring(each,{.5,-.4,0});
  }else{
   const int code=materialCode(kind);
   if(code==0)cloth(requested,{0,0,0});
   else if(code==1||code==2)volume(code,requested,{0,0,0});
   else if(code==3)ball(requested,{0,0,0});
   else spring(requested,{0,0,0});
  }
  if(elements_.size()>150000)throw std::invalid_argument("generated material fixture exceeds safety limit");
 }
 size_t elements()const{return elements_.size();}
 size_t nodes()const{return nodes_.size();}
 size_t bonds()const{return bonds_.size();}
 // Read-only positions/connectivity for scientific mesh visualizations.
 // Exporters must not mutate the solver or substitute envelopes for meshes.
 const std::vector<Node>& visualNodes()const noexcept{return nodes_;}
 const std::vector<Bond>& visualBonds()const noexcept{return bonds_;}
 const std::vector<Element>& visualElements()const noexcept{return elements_;}
 void step(){
  ++step_;
  // Lightweight damped Verlet dynamics and structural position constraints.
  // Different materials have different damping, stiffness and spatial
  // topology. Results are computational fixtures, not measured real materials.
  const double t=.023*step_+double(seed_%101)*.007;
  for(auto& n:nodes_){
   if(n.pin)continue;
   const Vec3 velocity=mul(sub(n.x,n.previous),n.damping);
   n.previous=n.x;
   Vec3 drive={.00003*std::sin(t),.00003*std::cos(t),-.00015};
   if(regime_=="relax")drive={0,0,-.00006};
   if(regime_=="impact"&&step_%48==0)drive.z+=.022;
   if(regime_=="shear")drive.x+=.00045*n.x.y*std::sin(t*2.4);
   if(regime_=="compression")drive.z-=.00045*(1.+std::sin(t*.8));
   if(regime_=="rapid"&&step_%40<20)drive.x+=.0015*std::cos(t*4.0);
   n.x=add(n.x,add(velocity,drive));
  }
  for(int iter=0;iter<3;++iter){
   for(const auto& b:bonds_){
    auto& a=nodes_[b.a];auto& c=nodes_[b.b];
    const auto d=sub(c.x,a.x);
    const double distance=length(d);
    if(distance<1e-10)continue;
    const double invA=a.pin?0.:1.,invB=c.pin?0.:1.;
    if(invA+invB==0)continue;
    const double amount=std::clamp((distance-b.rest)*b.strength/(distance*(invA+invB)),-.07,.07);
    const Vec3 correction=mul(d,amount);
    if(!a.pin)a.x=add(a.x,mul(correction,invA));
    if(!c.pin)c.x=sub(c.x,mul(correction,invB));
   }
   // Kinematic impactor gives repeatable transient contact-rich deformation.
   const double jump=(regime_=="impact"||regime_=="rapid")?
    (step_%60<30?-.56:.56):0.;
   const Vec3 center={.52*std::sin(.31*t)+jump,
    .32*std::cos(.27*t),1.20+.18*std::sin(.47*t)};
   const double obstacleRadius=regime_=="relax"?0.0:
     regime_=="compression"?.53:.35;
   for(auto& n:nodes_){
    if(n.pin)continue;
    Vec3 offset=sub(n.x,center);
    const double d=length(offset);
    if(d<obstacleRadius&&d>1e-8)
     n.x=add(n.x,mul(offset,(obstacleRadius-d)/d*.65));
    if(n.x.z<.03)n.x.z=.03;
    const auto displacement=sub(n.x,n.previous);
    const double move=length(displacement);
    if(move>.12)n.x=add(n.previous,mul(displacement,.12/move));
   }
  }
  for(const auto& n:nodes_)
   if(!std::isfinite(n.x.x)||!std::isfinite(n.x.y)||!std::isfinite(n.x.z))
    throw std::runtime_error("nonfinite material state");
 }
 // The region partition is tied to element topology and stays stable across
 // deformation. 'span' can change on an adaptive controller decision.
 std::vector<Primitive> snapshot(int span) const{
  if(span<1)throw std::invalid_argument("invalid material partition span");
  std::vector<Primitive> items;items.reserve(elements_.size());
  for(size_t i=0;i<elements_.size();++i){
   const auto& e=elements_[i];
   auto low=nodes_[e.vertices[0]].x,high=low;
   for(int k=1;k<e.count;++k){
    const auto p=nodes_[e.vertices[k]].x;
    low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
    high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
   }
   // A spring filament has nonzero physical thickness for broad-phase tests.
   if(e.material==4){
    const Vec3 radius{.012,.012,.012};
    low=sub(low,radius);high=add(high,radius);
   }
   ccp::research::Bounds box{int(i),low,high,1u,1u,true};
   items.push_back({box,int(i/size_t(span)),-1});
  }
  return items;
 }
};
}

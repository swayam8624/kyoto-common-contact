// Diagnostic evidence of real multi-cloth numerical stability.
// Reports actual unbounded particle travel and constraints rather than
// assuming that finite coordinates imply physically reasonable motion.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using ccp::research::MultiCloth;
using ccp::research::Policy;
static double dist(ccp::research::Vec3 a,ccp::research::Vec3 b) {
 const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
 return std::sqrt(x*x+y*y+z*z);
}
int main(int argc,char** argv){
 try{
  const int frames=argc>1?std::stoi(argv[1]):260;
  if(frames<1||frames>1000)throw std::invalid_argument("frames in [1,1000]");
  MultiCloth sim(3,2,Policy::OnlineContactAware);
  std::vector<ccp::research::Vec3> initial=sim.positions(),last=initial;
  double maxWorld=0,maxTravel=0,maxDelta=0,maxEdgeRatio=0,minHeight=1e9;
  std::size_t totalContacts=0;
  std::cout<<std::fixed<<std::setprecision(5);
  for(int step=1;step<=frames;++step){
   sim.step();
   const auto& points=sim.positions();
   double world=0,travel=0,delta=0,ratio=0,minZ=1e9;
   for(std::size_t i=0;i<points.size();++i){
    const auto p=points[i];
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
     throw std::runtime_error("NONFINITE POSITION frame="+std::to_string(step));
    world=std::max(world,std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)}));
    travel=std::max(travel,dist(p,initial[i]));
    delta=std::max(delta,dist(p,last[i]));
    minZ=std::min(minZ,p.z);
   }
   // All primary grid edges should remain within a reasonable stretch
   // ratio relative to initial rest shape (conservative stability metric).
   constexpr int side=13;
   for(std::size_t sheet=0;sheet<3;++sheet)for(int y=0;y<side;++y)for(int x=0;x<side;++x){
    const std::size_t id=sheet*side*side+y*side+x;
    if(x+1<side){
     const double rest=dist(initial[id],initial[id+1]);
     ratio=std::max(ratio,dist(points[id],points[id+1])/rest);
    }
    if(y+1<side){
     const double rest=dist(initial[id],initial[id+side]);
     ratio=std::max(ratio,dist(points[id],points[id+side])/rest);
    }
   }
   maxWorld=std::max(maxWorld,world);
   maxTravel=std::max(maxTravel,travel);
   maxDelta=std::max(maxDelta,delta);
   maxEdgeRatio=std::max(maxEdgeRatio,ratio);
   minHeight=std::min(minHeight,minZ);
   totalContacts+=sim.metrics().interSheetContacts;
   if(step==1||step%20==0||step==frames)
    std::cout<<"FRAME "<<step<<" abs "<<world<<" travel "<<travel
             <<" delta "<<delta<<" stretch "<<ratio<<" minZ "<<minZ
             <<" contacts "<<sim.metrics().interSheetContacts
             <<" broadMs "<<sim.metrics().broadMs<<"\n";
   last=points;
  }
  std::cout<<"MAX abs "<<maxWorld<<" travel "<<maxTravel
           <<" frameDelta "<<maxDelta<<" structuralStretch "<<maxEdgeRatio
           <<" minZ "<<minHeight<<" interSheetCorrections "<<totalContacts<<"\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

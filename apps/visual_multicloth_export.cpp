// Honest visual evidence: exports ACTUAL solver meshes and per-frame diagnostics.
// Identical initial conditions and physics across seven certified broad phases.
// If exact candidate pairs match, all seven meshes SHOULD match; an apparent
// geometry improvement from the broad phase alone would be a red flag.
// This is a correctness/qualitative artifact export, not a timed performance
// benchmark: disk I/O, auditing, and serialization are outside the solver.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::research::Vec3;
using ccp::research::MultiCloth;
using ccp::research::Policy;
const char* names[]={
 "sap_rebuild","global_guarded","region_fixed","region_adaptive",
 "region_sweep_fixed","region_sweep_adaptive","online_contact_work"
};
void vertices(std::ostream& out,const std::vector<Vec3>& p) {
 out<<"[";
 for(size_t i=0;i<p.size();++i){
  if(i)out<<',';
  out<<'['<<p[i].x<<','<<p[i].y<<','<<p[i].z<<']';
 }
 out<<"]";
}
double distance(const Vec3& a,const Vec3& b){
 return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);
}
struct Quality {
 std::vector<std::pair<int,int>> edges;
 std::vector<double> rest;
 std::vector<double> restAreas;
 explicit Quality(const MultiCloth& sim){
  std::set<std::pair<int,int>> unique;
  const auto& p=sim.positions();
  for(auto t:sim.triangles()){
   for(int j=0;j<3;++j){
    int a=t[j],b=t[(j+1)%3];
    if(a>b)std::swap(a,b);
    unique.insert({a,b});
   }
   restAreas.push_back(area(p[t[0]],p[t[1]],p[t[2]]));
  }
  edges.assign(unique.begin(),unique.end());
  for(auto [a,b]:edges)rest.push_back(distance(p[a],p[b]));
 }
 static double area(Vec3 a,Vec3 b,Vec3 c){
  const double ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z;
  const double vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
  return .5*std::hypot(std::hypot(uy*vz-uz*vy,uz*vx-ux*vz),ux*vy-uy*vx);
 }
 std::pair<double,int> measure(const MultiCloth& sim)const{
  const auto& p=sim.positions();
  double squared=0.;
  int n=0;
  for(size_t i=0;i<edges.size();++i){
   const auto [a,b]=edges[i];
   if(rest[i]<=1e-12)continue;
   const double e=distance(p[a],p[b])/rest[i]-1.;
   squared+=e*e;++n;
  }
  int collapsed=0;
  for(size_t i=0;i<restAreas.size();++i){
   const auto t=sim.triangles()[i];
   if(restAreas[i]>1e-12 &&
      area(p[t[0]],p[t[1]],p[t[2]])<.05*restAreas[i])
    ++collapsed;
  }
  return {n?std::sqrt(squared/n):0.,collapsed};
 }
};
MultiCloth configure(int method,int sheets,int balls){
 MultiCloth sim(sheets,balls,method==0?Policy::RebuildSAP:
                method==1?Policy::GlobalGuard:
                method==6?Policy::OnlineContactAware:Policy::RegionalCertified);
 sim.setAdaptive(method==3||method==5||method==6);
 if(method!=6)sim.setHierarchicalRegions(method>=4);
 sim.setAuditing(true);
 sim.setSelfCollisions(true);
 return sim;
}
void writeHeader(std::ostream& out,const MultiCloth& sim,int method){
 out<<"{\"type\":\"mesh_header\",\"schema\":1,\"policy\":\""<<names[method]
    <<"\",\"interpretation\":\"same physical solver; visual differences must be investigated\","
      "\"units\":\"simulation units\",\"triangles\":[";
 for(size_t i=0;i<sim.triangles().size();++i){
  if(i)out<<',';
  auto t=sim.triangles()[i];
  out<<'['<<t[0]<<','<<t[1]<<','<<t[2]<<']';
 }
 out<<"],\"triangle_sheets\":[";
 for(size_t i=0;i<sim.triangleSheets().size();++i){
  if(i)out<<',';
  out<<sim.triangleSheets()[i];
 }
 out<<"],\"rest_positions\":";
 vertices(out,sim.positions());
 out<<"}\n";
}
void writeFrame(std::ostream& out,const MultiCloth& sim,int frame,
                double oracleError,const Quality& q){
 const auto& m=sim.metrics();
 auto [strain,collapsed]=q.measure(sim);
 out<<"{\"type\":\"frame\",\"frame\":"<<frame
    <<",\"policy\":\""<<(sim.policy()==Policy::OnlineContactAware?
       "online_contact_work":"other")<<"\""
    <<",\"positions\":";
 vertices(out,sim.positions());
 out<<",\"spheres\":[";
 for(size_t i=0;i<sim.spheres().size();++i){
  if(i)out<<',';
  const auto& s=sim.spheres()[i];
  out<<"{\"center\":["<<s.center.x<<','<<s.center.y<<','<<s.center.z
     <<"],\"radius\":"<<s.radius<<"}";
 }
 out<<"],\"metrics\":{"
    <<"\"broad_ms\":"<<m.broadMs
    <<",\"narrow_ms\":"<<m.narrowMs
    <<",\"step_ms\":"<<m.stepMs
    <<",\"candidate_pairs\":"<<m.pairs
    <<",\"self_contacts\":"<<m.selfContacts
    <<",\"self_crossing_responses\":"<<m.selfCrossings
    <<",\"sphere_contacts\":"<<m.sphereContacts
    <<",\"precise_tests\":"<<m.preciseTests
    <<",\"max_position_difference_from_SAP\":"<<oracleError
    <<",\"rms_relative_edge_strain\":"<<strain
    <<",\"near_degenerate_triangles\":"<<collapsed
    <<",\"velocity_clamps\":"<<m.velocityClamps
    <<",\"oracle_misses\":"<<m.oracleMisses
    <<",\"oracle_checked\":"<<(m.oracleChecked?"true":"false")
    <<"}}\n";
}
}
int main(int argc,char** argv){
 try{
  if(argc<2||argc>5)
   throw std::runtime_error("Usage: ccp_visual_multicloth_export OUT_DIR [frames=72] [sheets=2] [spheres=2]");
  int frames=argc>=3?std::stoi(argv[2]):72;
  int sheets=argc>=4?std::stoi(argv[3]):2;
  int balls=argc>=5?std::stoi(argv[4]):2;
  if(frames<2||frames>360||sheets<1||sheets>3||balls<1||balls>8)
   throw std::invalid_argument("frames must be 2..360, sheets 1..3, balls 1..8");
  std::filesystem::path dir(argv[1]);
  std::filesystem::create_directories(dir);
  auto reference=configure(0,sheets,balls);
  std::vector<std::vector<Vec3>> states;
  states.reserve(frames);
  for(int f=0;f<frames;++f){reference.step();states.push_back(reference.positions());}
  for(int method=0;method<7;++method){
   auto sim=configure(method,sheets,balls);
   Quality quality(sim);
   const auto path=dir/(std::string(names[method])+".jsonl");
   std::ofstream out(path);
   if(!out)throw std::runtime_error("cannot create "+path.string());
   out<<std::fixed<<std::setprecision(8);
   writeHeader(out,sim,method);
   for(int f=0;f<frames;++f){
    sim.step();
    if(sim.positions().size()!=states[size_t(f)].size())
     throw std::runtime_error("policy changed mesh vertex count");
    double maxError=0.;
    for(size_t i=0;i<states[size_t(f)].size();++i)
     maxError=std::max(maxError,distance(sim.positions()[i],states[size_t(f)][i]));
    if(maxError>1e-7)
     throw std::runtime_error(std::string(names[method])+
                              " altered solver positions; frame="+std::to_string(f));
    writeFrame(out,sim,f,maxError,quality);
   }
   if(!out)throw std::runtime_error("failed to flush "+path.string());
   std::cout<<"VISUAL_EVIDENCE "<<names[method]<<" "<<path<<"\n";
  }
  std::cout<<"PASS: seven independently stepped coupled-solver policies; "
              "all geometry agreed with SAP reference to <1e-7.\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"VISUAL_EXPORT FAILED: "<<e.what()<<'\n';return 1;
 }
}

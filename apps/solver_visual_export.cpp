// Offline visual evidence from ACTUAL continuously simulated XPBD multicloth.
// Stream one JSON object per line, not fabricated geometry or an AABB replay.
// Coordinates are world-space positions of solver-owned vertices and spheres.
// Report *detected during-solver* contacts distinctly from post-step geometry.
// The optional exact audit is enabled, and errors FAIL the export.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using ccp::research::MultiCloth;
using ccp::research::Policy;
using ccp::research::Vec3;
const char* name(Policy p){
 switch(p){
  case Policy::RebuildSAP:return "sap_rebuild";
  case Policy::GlobalGuard:return "global_guarded";
  case Policy::RegionalCertified:return "region_hierarchical";
  case Policy::OnlineContactAware:return "online_contact_work";
  case Policy::FinalBounded:return "guard_sap_bounded_trial";
 }
 return "unknown";
}
Policy policy(const std::string& s){
 for(auto p:{Policy::RebuildSAP,Policy::GlobalGuard,Policy::RegionalCertified,
             Policy::OnlineContactAware,Policy::FinalBounded})
  if(s==name(p))return p;
 throw std::invalid_argument("policy must be sap_rebuild, global_guarded, "
   "region_hierarchical, online_contact_work or guard_sap_bounded_trial");
}
int positive(const char* s,int lo,int hi){
 std::size_t end=0;long long v=std::stoll(s,&end);
 if(end!=std::string(s).size()||v<lo||v>hi)
  throw std::invalid_argument("integer out of range");
 return int(v);
}
void vec(std::ostream& out,Vec3 p){
 if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
  throw std::runtime_error("nonfinite solver position");
 out<<'['<<p.x<<','<<p.y<<','<<p.z<<']';
}
double area(Vec3 a,Vec3 b,Vec3 c){
 const double abx=b.x-a.x,aby=b.y-a.y,abz=b.z-a.z;
 const double acx=c.x-a.x,acy=c.y-a.y,acz=c.z-a.z;
 const double x=aby*acz-abz*acy,y=abz*acx-abx*acz,z=abx*acy-aby*acx;
 return .5*std::sqrt(x*x+y*y+z*z);
}
struct AreaQuality {
 size_t degenerate{};
 double minRatio{1.},maxRatio{1.};
};
AreaQuality quality(const std::vector<Vec3>& positions,
                    const std::vector<std::array<int,3>>& tris,
                    const std::vector<double>& rest){
 AreaQuality q;
 for(size_t i=0;i<tris.size();++i){
  const auto& t=tris[i];
  const double current=area(positions[size_t(t[0])],
                            positions[size_t(t[1])],positions[size_t(t[2])]);
  const double ratio=current/std::max(rest[i],1e-12);
  q.minRatio=std::min(q.minRatio,ratio);
  q.maxRatio=std::max(q.maxRatio,ratio);
  if(ratio<.05)++q.degenerate;
 }
 return q;
}
}
int main(int argc,char** argv){
 try{
  if(argc<3||argc>6)throw std::invalid_argument(
    "usage: ccp_solver_visual_export OUTPUT.jsonl POLICY "
    "[FRAMES=48] [SHEETS=2] [SPHERES=2]");
  const std::filesystem::path output=argv[1];
  if(std::filesystem::exists(output))
   throw std::runtime_error("refuse overwriting existing solver visual evidence");
  const Policy selected=policy(argv[2]);
  const int frames=argc>=4?positive(argv[3],2,600):48;
  const int sheets=argc>=5?positive(argv[4],1,3):2;
  const int balls=argc>=6?positive(argv[5],1,8):2;
  MultiCloth sim(sheets,balls,selected);
  sim.setAuditing(true);
  if(selected==Policy::RegionalCertified)sim.setHierarchicalRegions(true);
  const auto& triangles=sim.triangles();
  const auto& triangleSheets=sim.triangleSheets();
  const auto initial=sim.positions();
  std::vector<double> rest;rest.reserve(triangles.size());
  for(auto t:triangles)
   rest.push_back(area(initial[size_t(t[0])],initial[size_t(t[1])],
                       initial[size_t(t[2])]));
  if(!output.parent_path().empty())
   std::filesystem::create_directories(output.parent_path());
  std::ofstream stream(output);
  if(!stream)throw std::runtime_error("can't create solver JSONL");
  stream<<std::setprecision(10);
  stream<<"{\"type\":\"header\",\"schema\":\"kyoto-solver-visual-v1\","
       <<"\"policy\":\""<<name(selected)<<"\",\"sheets\":"<<sheets
       <<",\"balls\":"<<balls<<",\"frames\":"<<frames<<",\"triangles\":[";
  for(size_t i=0;i<triangles.size();++i){
   if(i)stream<<',';
   auto t=triangles[i];stream<<'['<<t[0]<<','<<t[1]<<','<<t[2]<<']';
  }
  stream<<"],\"triangle_sheets\":[";
  for(size_t i=0;i<triangleSheets.size();++i){
   if(i)stream<<',';
   stream<<triangleSheets[i];
  }
  stream<<"]}\n";
  for(int frame=0;frame<frames;++frame){
   sim.step();
   const auto& p=sim.positions();
   const auto& m=sim.metrics();
   const auto q=quality(p,triangles,rest);
   stream<<"{\"type\":\"frame\",\"frame\":"<<frame
         <<",\"positions\":[";
   for(size_t i=0;i<p.size();++i){
    if(i)stream<<',';
    vec(stream,p[i]);
   }
   stream<<"],\"spheres\":[";
   const auto& ss=sim.spheres();
   for(size_t i=0;i<ss.size();++i){
    if(i)stream<<',';
    stream<<"{\"center\":";
    vec(stream,ss[i].center);
    stream<<",\"radius\":"<<ss[i].radius<<'}';
   }
   stream<<"],\"quality\":{\"degenerate_area_triangles\":"<<q.degenerate
         <<",\"minimum_area_ratio\":"<<q.minRatio
         <<",\"maximum_area_ratio\":"<<q.maxRatio
         <<"},\"metrics\":{\"broad_ms\":"<<m.broadMs
         <<",\"narrow_ms\":"<<m.narrowMs
         <<",\"step_ms\":"<<m.stepMs
         <<",\"candidate_pairs\":"<<m.pairs
         <<",\"detected_self_crossings_during_solver\":"<<m.selfCrossings
         <<",\"self_contacts\":"<<m.selfContacts
         <<",\"inter_sheet_contacts\":"<<m.interSheetContacts
         <<",\"oracle_misses\":"<<m.oracleMisses
         <<",\"oracle_checked\":"<<(m.oracleChecked?"true":"false")
         <<",\"maximum_strain\":"<<m.maxStructuralEdgeStretch
         <<",\"max_vertex_travel\":"<<m.maxFrameParticleTravel
         <<",\"velocity_clamps\":"<<m.velocityClamps
         <<"}}\n";
   if(!stream)throw std::runtime_error("could not write all solver frames");
  }
  stream.flush();
  if(!stream)throw std::runtime_error("failed to flush solver visual evidence");
  std::cout<<"[VISUAL] real solver geometry exported: "<<output
           <<" policy="<<name(selected)<<" frames="<<frames
           <<" triangles="<<triangles.size()<<"\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"SOLVER VISUAL EXPORT FAILED: "<<e.what()<<'\n';
  return 1;
 }
}

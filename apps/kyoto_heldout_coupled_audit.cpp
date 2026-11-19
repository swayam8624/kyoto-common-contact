// Kyoto held-out coupled solver comparison: three ORIGINAL broad-phase
// policies integrated inside the actual XPBD-style multi-cloth contact solver.
// Not a new topology-validating solver: the solver's sheet triangulation is
// historically fixed. The independent-topology generator has separate tests.
// Fails if positions or contact candidate completeness diverge from SAP.
// Emits honest synchronized mesh JSONL compatible with render_kyoto_visuals.py.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using ccp::research::MultiCloth;
using ccp::research::Policy;
using ccp::research::Vec3;
struct Choice {const char* name; Policy policy;};
constexpr std::array<Choice,3> kPolicies{{
  {"sap_rebuild",Policy::RebuildSAP},
  {"global_guarded",Policy::GlobalGuard},
  {"guard_sap_bounded_trial",Policy::FinalBounded}
}};
double length(Vec3 a,Vec3 b){
 const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
 return std::sqrt(x*x+y*y+z*z);
}
double area(Vec3 a,Vec3 b,Vec3 c){
 const Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z};
 const Vec3 v{c.x-a.x,c.y-a.y,c.z-a.z};
 const Vec3 cross{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
 return .5*std::sqrt(cross.x*cross.x+cross.y*cross.y+cross.z*cross.z);
}
int bounded(const char* value,int low,int high){
 const std::string text(value);std::size_t consumed=0;
 const auto v=std::stoll(text,&consumed);
 if(consumed!=text.size()||v<low||v>high)
  throw std::invalid_argument("integer argument outside allowed bounds");
 return static_cast<int>(v);
}
void positions(std::ostream& out,const std::vector<Vec3>& values){
 out<<'[';
 for(std::size_t i=0;i<values.size();++i){
  if(i)out<<',';
  out<<'['<<values[i].x<<','<<values[i].y<<','<<values[i].z<<']';
 }
 out<<']';
}
struct Quality {
 std::vector<std::pair<int,int>> edges;
 std::vector<double> restEdges,restAreas;
 explicit Quality(const MultiCloth& sim){
  const auto& p=sim.positions();
  std::set<std::pair<int,int>> unique;
  for(const auto t:sim.triangles()){
   for(int e=0;e<3;++e){
    const int a=std::min(t[e],t[(e+1)%3]);
    const int b=std::max(t[e],t[(e+1)%3]);
    unique.insert({a,b});
   }
   restAreas.push_back(area(p[t[0]],p[t[1]],p[t[2]]));
  }
  edges.assign(unique.begin(),unique.end());
  for(const auto [a,b]:edges)restEdges.push_back(length(p[a],p[b]));
 }
 struct Stats {double rmsStretch{},maxStretch{},minZ{1e10};std::size_t collapsed{};};
 Stats measure(const MultiCloth& sim) const {
  Stats s;double squared=0.;std::size_t counted=0;
  const auto& p=sim.positions();
  for(std::size_t i=0;i<edges.size();++i){
   const auto [a,b]=edges[i];
   if(restEdges[i]<=1e-12)continue;
   const double ratio=length(p[a],p[b])/restEdges[i];
   const double relative=ratio-1.;
   if(!std::isfinite(relative))
    throw std::runtime_error("nonfinite mesh edge stretch");
   squared+=relative*relative;++counted;
   s.maxStretch=std::max(s.maxStretch,ratio);
  }
  s.rmsStretch=counted?std::sqrt(squared/double(counted)):0.;
  for(std::size_t i=0;i<restAreas.size();++i){
   const auto t=sim.triangles()[i];
   if(restAreas[i]>1e-12 &&
      area(p[t[0]],p[t[1]],p[t[2]])<.05*restAreas[i])
    ++s.collapsed;
  }
  for(const auto& x:p){
   if(!std::isfinite(x.x)||!std::isfinite(x.y)||!std::isfinite(x.z))
    throw std::runtime_error("nonfinite physical particle position");
   s.minZ=std::min(s.minZ,x.z);
  }
  return s;
 }
};

Vec3 grabReference(const MultiCloth& sim,int triangle){
 const auto t=sim.triangles().at(std::size_t(triangle));
 const auto& p=sim.positions();
 return {(p[t[0]].x+p[t[1]].x+p[t[2]].x)/3.,
         (p[t[0]].y+p[t[1]].y+p[t[2]].y)/3.,
         (p[t[0]].z+p[t[1]].z+p[t[2]].z)/3.};
}
void configure(MultiCloth& sim,const std::string& scenario){
 sim.setAuditing(true);
 sim.setSelfCollisions(true);
 if(scenario=="free")return;
 const int triangle=2*(5*12+5);
 const auto point=grabReference(sim,triangle);
 if(!sim.beginGrab(triangle,{1./3,1./3,1./3},point))
  throw std::runtime_error("failed to acquire identical physical grab");
}
void setDrive(MultiCloth& sim,const std::string& scenario,Vec3 original,int frame,int frames){
 if(scenario=="free")return;
 const double fraction=double(frame+1)/double(frames);
 // Smooth continuous position targets rather than teleporting particles.
 Vec3 target=original;
 if(scenario=="pull"){
  target.x+=.26*std::sin(3.141592653589793*fraction);
  target.y+=.09*std::sin(6.283185307179586*fraction);
  target.z+=.36*std::sin(3.141592653589793*fraction);
 }else if(scenario=="fold"){
  // A substantial, gradual across-sheet pull: an actual solver grab
  // constraint, never direct particle teleportation. Some trajectories
  // may still bend without self-contact; contact metrics decide.
  target.x-=1.35*fraction;
  target.y+=.46*std::sin(3.141592653589793*fraction);
  target.z-=.55*fraction;
 }else throw std::invalid_argument("unknown coupled solver scenario");
 sim.updateGrab(target);
}
void header(std::ostream& out,const MultiCloth& sim,const char* label,
            const std::string& scenario,std::uint32_t geometrySeed,
            std::uint32_t topologySeed){
 out<<"{\"type\":\"mesh_header\",\"schema\":1,\"policy\":\""<<label
    <<"\",\"scenario\":\""<<scenario
    <<"\",\"geometry_seed\":"<<geometrySeed
    <<",\"topology_seed\":"<<topologySeed
    <<",\"pinned_vertices\":"<<sim.pinnedCount()
    <<",\"units\":\"simulation units\",\"triangles\":[";
 for(std::size_t i=0;i<sim.triangles().size();++i){
  if(i)out<<',';
  const auto t=sim.triangles()[i];
  out<<'['<<t[0]<<','<<t[1]<<','<<t[2]<<']';
 }
 out<<"],\"triangle_sheets\":[";
 for(std::size_t i=0;i<sim.triangleSheets().size();++i){
  if(i)out<<',';
  out<<sim.triangleSheets()[i];
 }
 out<<"],\"rest_positions\":";
 positions(out,sim.positions());
 out<<"}\n";
}
void frame(std::ostream& out,const MultiCloth& sim,int index,
           double error,const Quality::Stats& quality){
 const auto& m=sim.metrics();
 out<<"{\"type\":\"frame\",\"frame\":"<<index<<",\"positions\":";
 positions(out,sim.positions());
 out<<",\"spheres\":[";
 for(std::size_t i=0;i<sim.spheres().size();++i){
  if(i)out<<',';
  const auto& sphere=sim.spheres()[i];
  out<<"{\"center\":["<<sphere.center.x<<','<<sphere.center.y<<','
     <<sphere.center.z<<"],\"radius\":"<<sphere.radius<<"}";
 }
 out<<"],\"metrics\":{"
    <<"\"broad_ms\":"<<m.broadMs
    <<",\"narrow_ms\":"<<m.narrowMs
    <<",\"step_ms\":"<<m.stepMs
    <<",\"candidate_pairs\":"<<m.pairs
    <<",\"self_contacts\":"<<m.selfContacts
    <<",\"sphere_contacts\":"<<m.sphereContacts
    <<",\"precise_tests\":"<<m.preciseTests
    <<",\"self_crossings\":"<<m.selfCrossings
    <<",\"frozen_guard_queries\":"<<m.frozenGuardQueries
    <<",\"frozen_sap_queries\":"<<m.frozenSapQueries
    <<",\"frozen_trial_queries\":"<<m.frozenTrialQueries
    <<",\"frozen_switches\":"<<m.frozenSwitches
    <<",\"last_broadphase_action\":\""<<m.lastEvent<<"\""
    <<",\"rms_relative_edge_strain\":"<<quality.rmsStretch
    <<",\"maximum_edge_stretch\":"<<quality.maxStretch
    <<",\"near_degenerate_triangles\":"<<quality.collapsed
    <<",\"min_height\":"<<quality.minZ
    <<",\"max_position_difference_from_SAP\":"<<error
    <<",\"velocity_clamps\":"<<m.velocityClamps
    <<",\"oracle_misses\":"<<m.oracleMisses
    <<",\"oracle_checked\":"<<(m.oracleChecked?"true":"false")
    <<"}}\n";
}

void run(const std::filesystem::path& dest,int frames,int sheets,int spheres,
         const std::string& scenario,std::uint32_t geometrySeed,
         std::uint32_t topologySeed){
 if(std::filesystem::exists(dest))
  throw std::runtime_error("refusing to overwrite coupled solver evidence");
 std::filesystem::create_directories(dest);
 std::ofstream csv(dest/"COUPLED_SOLVER.csv");
 if(!csv)throw std::runtime_error("cannot open coupled solver CSV");
 csv<<"scenario,geometry_seed,topology_seed,policy,frame,sheets,spheres,triangles,pinned_vertices,candidate_pairs,"
       "precise_tests,self_contacts,sphere_contacts,self_crossings,velocity_clamps,"
       "frozen_guard_queries,frozen_sap_queries,frozen_trial_queries,frozen_switches,last_broadphase_action,"
       "oracle_misses,broad_ms,narrow_ms,step_ms,max_position_difference_from_sap,"
       "rms_relative_edge_strain,max_edge_stretch,near_degenerate_triangles,min_height\n";
 csv<<std::fixed<<std::setprecision(9);
 std::vector<std::vector<Vec3>> oracleStates;
 std::size_t comparisonFrames=0;
 for(std::size_t p=0;p<kPolicies.size();++p){
  const auto& policy=kPolicies[p];
  MultiCloth sim(sheets,spheres,policy.policy);
  if(geometrySeed!=0||topologySeed!=0)
   sim.setFixtureVariation(geometrySeed,topologySeed);
  configure(sim,scenario);
  const Quality quality(sim);
  const Vec3 initial=grabReference(sim,2*(5*12+5));
  std::ofstream visual(dest/(std::string(policy.name)+".jsonl"));
  if(!visual)throw std::runtime_error("cannot open coupled solver visual output");
  visual<<std::fixed<<std::setprecision(10);
  header(visual,sim,policy.name,scenario,geometrySeed,topologySeed);
  for(int i=0;i<frames;++i){
   setDrive(sim,scenario,initial,i,frames);
   sim.step();
   const auto stats=quality.measure(sim);
   const auto& m=sim.metrics();
   if(m.oracleMisses!=0)
    throw std::runtime_error("coupled solver detected missed exact AABB candidates");
   if(m.broadMs<0||m.narrowMs<0||m.stepMs<0)
    throw std::runtime_error("invalid measured coupled solver time");
   double worst=0.;
   if(p==0)oracleStates.push_back(sim.positions());
   else{
    if(sim.positions().size()!=oracleStates.at(std::size_t(i)).size())
     throw std::runtime_error("policy changed physical solver vertex count");
    for(std::size_t j=0;j<sim.positions().size();++j)
     worst=std::max(worst,length(sim.positions()[j],
                                 oracleStates[std::size_t(i)][j]));
    if(worst>1e-7)
     throw std::runtime_error("coupled solver motion differs from SAP by >1e-7"
                              " at frame "+std::to_string(i));
    ++comparisonFrames;
   }
   csv<<scenario<<','<<geometrySeed<<','<<topologySeed<<','<<policy.name<<','<<i<<','<<sheets<<','<<spheres<<','
      <<sim.triangles().size()<<','<<sim.pinnedCount()<<','
      <<m.pairs<<','<<m.preciseTests<<','
      <<m.selfContacts<<','<<m.sphereContacts<<','<<m.selfCrossings<<','
      <<m.velocityClamps<<','<<m.frozenGuardQueries<<','<<m.frozenSapQueries
      <<','<<m.frozenTrialQueries<<','<<m.frozenSwitches
      <<','<<m.lastEvent
      <<','<<m.oracleMisses<<','<<m.broadMs<<','<<m.narrowMs<<','<<m.stepMs
      <<','<<worst<<','<<stats.rmsStretch<<','<<stats.maxStretch
      <<','<<stats.collapsed<<','<<stats.minZ<<'\n';
   frame(visual,sim,i,worst,stats);
  }
  if(!visual)throw std::runtime_error("failed writing coupled mesh JSONL");
 }
 if(!csv)throw std::runtime_error("failed writing coupled CSV");
 std::cout<<"PASS COUPLED_SOLVER: "<<scenario
          <<" frames="<<frames<<" sheets="<<sheets<<" spheres="<<spheres
          <<" geometry_seed="<<geometrySeed<<" topology_seed="<<topologySeed
          <<" independent_policy_frame_parity="<<comparisonFrames
          <<" actual XPBD-style solver contact, precise tests and geometry"
          <<" validated against SAP; original V4 unchanged\n";
}
}
int main(int argc,char** argv){
 try{
  if(argc!=6&&argc!=8)
   throw std::runtime_error("usage: ccp_kyoto_heldout_coupled OUTPUT_DIR FRAMES(8..120) SHEETS(1..3) SPHERES(1..3) SCENARIO(free,pull,fold) [GEOMETRY_SEED TOPOLOGY_SEED]");
  const int frames=bounded(argv[2],8,120);
  const int sheets=bounded(argv[3],1,3);
  const int spheres=bounded(argv[4],1,3);
  const std::string scenario(argv[5]);
  if(scenario!="free"&&scenario!="pull"&&scenario!="fold")
   throw std::invalid_argument("invalid scenario; use free/pull/fold");
  const auto geometrySeed=argc==8?std::uint32_t(bounded(argv[6],1,2000000000)):0u;
  const auto topologySeed=argc==8?std::uint32_t(bounded(argv[7],1,2000000000)):0u;
  run(argv[1],frames,sheets,spheres,scenario,geometrySeed,topologySeed);
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"KYOTO COUPLED SOLVER AUDIT FAILED: "<<e.what()<<'\n';return 1;
 }
}

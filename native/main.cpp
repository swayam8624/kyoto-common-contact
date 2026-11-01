#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include "ccp/live_cloth.hpp"
#include "ccp/research_multicloth.hpp"
#include "ccp/cloth_interaction.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>
using V=ccp::live::Vec3;
V add(V a,V b){return{a.x+b.x,a.y+b.y,a.z+b.z};} V sub(V a,V b){return{a.x-b.x,a.y-b.y,a.z-b.z};} V mul(V a,double k){return{a.x*k,a.y*k,a.z*k};} double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;} V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};} V norm(V a){double l=std::sqrt(dot(a,a));return l>1e-12?mul(a,1/l):V{0,0,1};}
struct M{float a[16]{};};
M identity(){M m;for(int i=0;i<4;i++)m.a[i*5]=1;return m;}
M product(M a,M b){M o;for(int c=0;c<4;c++)for(int r=0;r<4;r++)for(int k=0;k<4;k++)o.a[c*4+r]+=a.a[k*4+r]*b.a[c*4+k];return o;}
M perspective(float f,float asp,float n,float z){M m;float q=1/std::tan(f/2);m.a[0]=q/asp;m.a[5]=q;m.a[10]=(z+n)/(n-z);m.a[11]=-1;m.a[14]=2*z*n/(n-z);return m;}
M look(V eye,V target,V up){V f=norm(sub(target,eye)),s=norm(cross(f,up)),u=cross(s,f);M m=identity();m.a[0]=s.x;m.a[4]=s.y;m.a[8]=s.z;m.a[1]=u.x;m.a[5]=u.y;m.a[9]=u.z;m.a[2]=-f.x;m.a[6]=-f.y;m.a[10]=-f.z;m.a[12]=-dot(s,eye);m.a[13]=-dot(u,eye);m.a[14]=dot(f,eye);return m;}
M ortho(float l,float r,float b,float t,float n,float f){M m=identity();m.a[0]=2/(r-l);m.a[5]=2/(t-b);m.a[10]=-2/(f-n);m.a[12]=-(r+l)/(r-l);m.a[13]=-(t+b)/(t-b);m.a[14]=-(f+n)/(f-n);return m;}
GLuint shader(const char*src,GLenum type){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof(log),nullptr,log);throw std::runtime_error(log);}return s;}
GLuint program(const char*v,const char*f){GLuint p=glCreateProgram();GLuint vs=shader(v,GL_VERTEX_SHADER),fs=shader(f,GL_FRAGMENT_SHADER);glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Shader link failed");glDeleteShader(vs);glDeleteShader(fs);return p;}
const char*vs=R"(#version 330 core
layout(location=0) in vec3 position;layout(location=1) in vec3 normal;layout(location=2) in vec3 color;
uniform mat4 vp;uniform mat4 lightVP;out vec3 P;out vec3 N;out vec3 C;out vec4 LP;
void main(){P=position;N=normal;C=color;LP=lightVP*vec4(P,1);gl_Position=vp*vec4(P,1);})";
const char*fs=R"(#version 330 core
in vec3 P;in vec3 N;in vec3 C;in vec4 LP;out vec4 frag;uniform vec3 eye;uniform sampler2D shadowMap;
float shadow(vec3 n,vec3 l){vec3 q=LP.xyz/LP.w*.5+.5;if(q.z>1.0)return 0;float v=0;float bias=max(.001*(1-dot(n,l)),.0004);vec2 texel=1.0/textureSize(shadowMap,0);for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)v+=q.z-bias>texture(shadowMap,q.xy+vec2(x,y)*texel).r?1:0;return v/9;}
void main(){vec3 n=normalize(N);if(!gl_FrontFacing)n=-n;vec3 l=normalize(vec3(4,-3,7)-P),v=normalize(eye-P),h=normalize(l+v);vec3 base=C;if(abs(P.z)<.002){vec2 f=abs(fract(P.xy)-.5);float grid=1-smoothstep(.485,.495,max(f.x,f.y));base*=mix(.87,1.0,grid);}float diff=max(dot(n,l),0);float spec=pow(max(dot(n,h),0),40)*.14;float sh=shadow(n,l);vec3 col=base*(.34+diff*(1-sh)*.8)+vec3(spec*(1-sh));col+=base*max(dot(n,normalize(vec3(-3,2,3))),0)*.17;col=col/(col+vec3(.25));col=pow(col,vec3(1.0/2.2));frag=vec4(col,1);})";
const char*dvs=R"(#version 330 core
layout(location=0) in vec3 position;uniform mat4 vp;void main(){gl_Position=vp*vec4(position,1);})";
const char*dfs=R"(#version 330 core
void main(){})";
const char*lvs=R"(#version 330 core
layout(location=0) in vec3 position;layout(location=2) in vec3 color;uniform mat4 vp;out vec3 C;void main(){gl_Position=vp*vec4(position,1);C=color;})";
const char*lfs=R"(#version 330 core
in vec3 C;out vec4 frag;void main(){frag=vec4(C,1);})";
struct Vertex{float x,y,z,nx,ny,nz,r,g,b;};
void vertex(std::vector<Vertex>&v,V p,V n,V c){v.push_back({(float)p.x,(float)p.y,(float)p.z,(float)n.x,(float)n.y,(float)n.z,(float)c.x,(float)c.y,(float)c.z});}
void triangle(std::vector<Vertex>&v,V a,V b,V c,V color){V n=norm(cross(sub(b,a),sub(c,a)));vertex(v,a,n,color);vertex(v,b,n,color);vertex(v,c,n,color);}
void segment(std::vector<Vertex>&v,V a,V b,V color){vertex(v,a,{0,0,1},color);vertex(v,b,{0,0,1},color);}
void box(std::vector<Vertex>&v,V lo,V hi,V color){std::array<V,8>p;for(int i=0;i<8;i++)p[i]={i&1?hi.x:lo.x,i&2?hi.y:lo.y,i&4?hi.z:lo.z};for(int i=0;i<8;i++)for(int k=0;k<3;k++)if(!(i&(1<<k)))segment(v,p[i],p[i|(1<<k)],color);}
void sphere(std::vector<Vertex>&v,V center,double radius){const int rings=24,slices=40;auto p=[&](int j,int i){double th=M_PI*j/rings,ph=2*M_PI*i/slices;return V{std::sin(th)*std::cos(ph),std::sin(th)*std::sin(ph),std::cos(th)};};for(int j=0;j<rings;j++)for(int i=0;i<slices;i++){V a=p(j,i),b=p(j+1,i),c=p(j+1,i+1),d=p(j,i+1);for(V n:{a,b,c,a,c,d})vertex(v,add(center,mul(n,radius)),n,{.16,.21,.26});}}
struct EvidenceBar { std::string scene,algorithm; double meanMs{},extraCandidates{}; };
struct PatchBenchRow {
 int span{};
 std::string frontier,phase;
 double meanStepMs{},maxPositionError{};
};
struct MultiBenchRow {
 int sheets{};
 std::string policy;
 double broadMs{},preciseMs{},stepMs{},candidates{},interContacts{},rejected{},positionError{};
};
struct App {
 ccp::live::ClothSimulation sim;
 ccp::research::MultiCloth multi{3,2};
 bool multiMode=false,multiAudit=false,multiAdaptive=true,multiHierarchy=true;
 bool paused=false,wire=true,cutaway=false,drag=false,evidenceMode=false;
  bool clothMouseGrab=false;
  V dragOrigin{},dragPlaneNormal{};
 int stage=1,evidenceScene=5;
 double yaw=-.93,pitch=.53,distance=8,lastX{},lastY{},inspectorScroll=0;
 V target{0,0,1.2};
 std::ofstream trace,events;
 std::vector<std::string> recentEvents;
 std::vector<EvidenceBar> broadphaseResults;
 std::vector<MultiBenchRow> multiResults;
 std::vector<PatchBenchRow> patchResults;
 std::vector<double> historyCandidates,historyInterContacts,historyBroadMs;
 double rebuildMean=std::numeric_limits<double>::quiet_NaN();
 double guardedMean=std::numeric_limits<double>::quiet_NaN();
 double rebuildBroad{},guardedBroad{},rebuildNarrow{},guardedNarrow{};
 double rebuildPrecise{},guardedPrecise{};
};
void pushInspectorEvent(App& a,const std::string& event) {
 a.recentEvents.push_back(event);
 if(a.recentEvents.size()>65) a.recentEvents.erase(a.recentEvents.begin());
}
// Read only real benchmark measurements generated on THIS checkout.
// If no CSV exists the UI prints "NO VERIFIED LOCAL DATA", not mock timings.
void loadEvidence(App& a) {
 const std::filesystem::path base(CCP_OUTPUT_DIR);
 a.broadphaseResults.clear();
 a.multiResults.clear();
 a.patchResults.clear();
 {
  std::ifstream input(base/"patch-sweep.csv");
  std::string line;std::getline(input,line);
  while(std::getline(input,line)){
   std::stringstream stream(line);std::string cell;std::vector<std::string> cells;
   while(std::getline(stream,cell,','))cells.push_back(cell);
   if(cells.size()<13)continue;
   try {
    PatchBenchRow row;
    row.span=std::stoi(cells[0]);row.frontier=cells[1];
    row.phase=cells[2];row.meanStepMs=std::stod(cells[4]);
    row.maxPositionError=std::stod(cells[12]);
    if(std::isfinite(row.meanStepMs)&&row.meanStepMs>=0 &&
       std::isfinite(row.maxPositionError)&&row.maxPositionError>=0 &&
       row.maxPositionError<1e-8)a.patchResults.push_back(row);
   } catch(const std::exception&) {}
  }
 }
 {
  std::ifstream input(base/"multicloth-real.csv");
  std::string line;std::getline(input,line);
  while(std::getline(input,line)){
   std::stringstream ss(line);std::string field;std::vector<std::string> cells;
   while(std::getline(ss,field,','))cells.push_back(field);
   if(cells.size()<15)continue;
   try{
    MultiBenchRow m;
    m.sheets=std::stoi(cells[0]);m.policy=cells[2];
    m.broadMs=std::stod(cells[4]);m.preciseMs=std::stod(cells[5]);
    m.stepMs=std::stod(cells[6]);m.candidates=std::stod(cells[7]);
    m.interContacts=std::stod(cells[8]);m.rejected=std::stod(cells[12]);
    m.positionError=std::stod(cells[14]);
    if(m.positionError<1e-7)a.multiResults.push_back(m);
   }catch(const std::exception&){}
  }
 }
 a.rebuildMean=a.guardedMean=std::numeric_limits<double>::quiet_NaN();
 {
  std::ifstream input(base/"broadphase-leaderboard.csv");
  std::string line;std::getline(input,line);
  while(std::getline(input,line)) {
   std::stringstream row(line);std::vector<std::string> cells;std::string field;
   while(std::getline(row,field,','))cells.push_back(field);
   if(cells.size()<9)continue;
   try {a.broadphaseResults.push_back({cells[0],cells[1],std::stod(cells[4]),std::stod(cells[8])});}
   catch(const std::exception&) {}
  }
 }
 {
  std::ifstream input(base/"live_benchmark-postfix.csv");
  std::string line;std::getline(input,line);
  double buildSum=0,reuseSum=0,buildBroad=0,reuseBroad=0;
  double buildNarrow=0,reuseNarrow=0,buildPrecise=0,reusePrecise=0;
  int buildCount=0,reuseCount=0;
  while(std::getline(input,line)) {
   std::stringstream row(line);std::vector<std::string> cells;std::string field;
   while(std::getline(row,field,','))cells.push_back(field);
   if(cells.size()<5)continue;
   try {
    const double ms=std::stod(cells[3]);
    if(cells[2]=="baseline_rebuild"){
     buildSum+=ms;buildBroad+=std::stod(cells[4]);buildNarrow+=std::stod(cells[5]);
     buildPrecise+=std::stod(cells[14]);++buildCount;
    }
    if(cells[2]=="guarded_reuse"){
     reuseSum+=ms;reuseBroad+=std::stod(cells[4]);reuseNarrow+=std::stod(cells[5]);
     reusePrecise+=std::stod(cells[14]);++reuseCount;
    }
   }catch(const std::exception&){}
  }
  if(buildCount>0){
   a.rebuildMean=buildSum/buildCount;
   a.rebuildBroad=buildBroad/buildCount;a.rebuildNarrow=buildNarrow/buildCount;
   a.rebuildPrecise=buildPrecise/buildCount;
  }
  if(reuseCount>0){
   a.guardedMean=reuseSum/reuseCount;
   a.guardedBroad=reuseBroad/reuseCount;a.guardedNarrow=reuseNarrow/reuseCount;
   a.guardedPrecise=reusePrecise/reuseCount;
  }
 }
 pushInspectorEvent(a,"EVIDENCE: LOCAL CSV "+std::string(a.broadphaseResults.empty()?"MISSING":"LOADED"));
}
// Configure a STATIC candidate policy using calibration-only measurements.
// Evaluation frames never enter the policy selection; no online learning
// or prior-art novelty is claimed.
void applyCalibratedPatch(App& a){
 // Require the entire 12-variant, two-phase matrix plus SAP, all with
 // validated paired positions. Partial/stale benchmark output is rejected.
 constexpr std::array<int,6> spans{1,2,3,4,6,12};
 const auto count=[&](int span,const std::string& mode,const std::string& phase){
  return std::count_if(a.patchResults.begin(),a.patchResults.end(),
   [&](const PatchBenchRow& r){
    return r.span==span && r.frontier==mode && r.phase==phase;
   });
 };
 bool valid=a.patchResults.size()==26;
 for(const auto& phase:{"calibration","evaluation"}){
  if(count(0,"sap_rebuild",phase)!=1)valid=false;
  for(const int span:spans)
   for(const auto& mode:{"all_region_pairs","two_tier"})
    if(count(span,mode,phase)!=1)valid=false;
 }
 if(!valid){
  pushInspectorEvent(a,"INCOMPLETE CALIBRATION: RUN prepare_meeting.sh");
  return;
 }
 const PatchBenchRow* selected=nullptr;
 for(const auto& row:a.patchResults){
  if(row.phase!="calibration")continue;
  if(!selected || row.meanStepMs<selected->meanStepMs)selected=&row;
 }
 if(!selected)return;
 if(selected->frontier=="sap_rebuild"){
  a.multi.setPolicy(ccp::research::Policy::RebuildSAP);
  pushInspectorEvent(a,"CALIBRATION CHOSE SAP FALLBACK");
 }else{
  a.multi.setPolicy(ccp::research::Policy::RegionalCertified);
  a.multi.setAdaptive(false);
  a.multiAdaptive=false;
  a.multiHierarchy=selected->frontier=="two_tier";
  a.multi.setHierarchicalRegions(a.multiHierarchy);
  a.multi.setPatchCellSpan(selected->span);
  pushInspectorEvent(a,"CALIBRATION APPLIED PATCH "+
     std::to_string(selected->span)+" "+selected->frontier);
 }
 a.historyCandidates.clear();
 a.historyInterContacts.clear();
 a.historyBroadMs.clear();
}
void fitScene(App&a,GLFWwindow*w){
 const auto& points=a.multiMode?a.multi.positions():a.sim.positions();
 V lo=points.front(),hi=lo;
 for(V p:points){
  lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
  hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
 }
 const auto includeSphere=[&](V p,double r){
  lo={std::min(lo.x,p.x-r),std::min(lo.y,p.y-r),std::min(0.0,lo.z)};
  hi={std::max(hi.x,p.x+r),std::max(hi.y,p.y+r),std::max(hi.z,p.z+r)};
 };
 if(a.multiMode)for(const auto& b:a.multi.spheres())includeSphere(b.center,b.radius);
 else includeSphere(a.sim.sphereCenter(),a.sim.sphereRadius());
 a.target=mul(add(lo,hi),.5);
 const double radius=std::sqrt(dot(sub(hi,lo),sub(hi,lo)))*.5;
 int width,height;glfwGetFramebufferSize(w,&width,&height);
 const double aspect=(width-350.*height/920)/height;
 const double angle=std::min(.35,std::atan(std::tan(.35)*aspect));
 a.distance=(radius+.12)/std::sin(angle)*1.13;
 a.yaw=-.93;a.pitch=.40;
}
double stateHash(App&a){double hash=0;for(auto p:a.sim.positions())hash+=p.x*.37+p.y*.71+p.z*1.13;return hash;}
void logEvent(App&a,const std::string&event){auto&s=a.sim.stats();std::ostringstream line;line<<"LIVE "<<event<<" frame="<<s.frame<<" broad="<<(s.sap?"SAP":"EXHAUSTIVE")<<" response="<<s.collisions<<" guarded="<<s.guardedReuse<<" margin="<<a.sim.queryRequest().margin<<" candidates="<<s.candidateCount<<" minGap="<<s.minGap<<" stateHash="<<std::setprecision(14)<<stateHash(a)<<'\n';std::cerr<<line.str();a.events<<line.str();a.events.flush();a.trace.flush();
 pushInspectorEvent(a,event+" F"+std::to_string(s.frame));}
std::string number(double d,int precision);
void stepSimulation(App&a){
 if(a.multiMode){
  a.multi.step();
  const auto& m=a.multi.metrics();
  if(m.oracleMisses)a.paused=true;
  // Never let a numerical explosion continue invisibly in the viewer.
  if(m.maxFrameParticleTravel>.8||m.maxStructuralEdgeStretch>2.6||
     m.maxWorldCoordinate>5.0){
   a.paused=true;
   pushInspectorEvent(a,"PHYSICS STABILITY LIMIT: PAUSED");
   pushInspectorEvent(a,"FRAME TRAVEL "+number(m.maxFrameParticleTravel,3)+
     " EDGE STRETCH "+number(m.maxStructuralEdgeStretch,2));
  }
  a.historyCandidates.push_back(double(m.pairs));
  a.historyInterContacts.push_back(double(m.interSheetContacts));
  a.historyBroadMs.push_back(m.broadMs);
  if(a.historyCandidates.size()>160){
   a.historyCandidates.erase(a.historyCandidates.begin());
   a.historyInterContacts.erase(a.historyInterContacts.begin());
   a.historyBroadMs.erase(a.historyBroadMs.begin());
  }
  if(m.onlineSplits||m.onlineMerges||m.onlineStrategySwitches){
   std::ostringstream changed;
   changed<<"ONLINE FRAME "<<a.multi.frame()
          <<" SPLIT "<<m.onlineSplits<<" MERGE "<<m.onlineMerges
          <<" MODE-SWITCH "<<m.onlineStrategySwitches
          <<" PATCHES "<<m.onlinePatches
          <<" HEAT "<<number(m.onlineContactHeat,1);
   pushInspectorEvent(a,changed.str());
  }
  if(a.multi.frame()==1||a.multi.frame()%6==0||m.oracleMisses){
   std::ostringstream line;
   line<<"F"<<a.multi.frame()<<" "
       <<(a.multi.policy()==ccp::research::Policy::OnlineContactAware?"ONLINE":
          a.multi.policy()==ccp::research::Policy::RegionalCertified?"REGIONAL":
          a.multi.policy()==ccp::research::Policy::GlobalGuard?"GLOBAL":"SAP")
       <<" PAIRS "<<m.pairs<<" CONTACTS "<<m.contacts
       <<" INTER "<<m.interSheetContacts<<" SPHERE "<<m.sphereContacts;
   pushInspectorEvent(a,line.str());
   std::ostringstream event;
   event<<"REGION DIRTY "<<m.dirtyRegions<<" BLOCKS "<<m.regionalPairRebuilds
        <<"/"<<m.regionalPairReuses<<" REJECTED "<<m.cachedPairsRejected;
   event<<" FRONTIER "<<m.regionTopLevelOverlaps
        <<" SKIPPED "<<m.regionPairsSkipped
        <<" PATCHES "<<m.onlinePatches;
   pushInspectorEvent(a,event.str());
   std::ostringstream cost;
   cost<<"BROAD "<<number(m.broadMs,3)<<"ms PRECISE "
       <<number(m.narrowMs,3)<<"ms STEP "<<number(m.stepMs,3)<<"ms";
   pushInspectorEvent(a,cost.str());
   a.events<<line.str()<<'\n';
  }
  return;
 }
 a.sim.step();auto&s=a.sim.stats();auto q=a.sim.queryRequest();a.trace<<s.frame<<','<<(s.sap?"SAP":"EXHAUSTIVE")<<','<<s.collisions<<','<<(s.guardedReuse?"GUARDED":"REBUILD")<<','<<q.margin<<','<<q.displacementBudget<<','<<s.queryBuilds<<','<<s.queryReuses<<','<<s.candidateCount<<','<<s.proximityCount<<','<<std::setprecision(14)<<s.minGap<<','<<s.broadMs<<','<<s.narrowMs<<','<<s.oracleMs<<','<<s.diagnosticMs<<','<<s.solverMs<<','<<s.preciseTests<<','<<s.oracleTests<<','<<s.oracleChecked<<','<<s.stepMs<<','<<(s.oracleChecked?std::to_string(s.oracleMisses):"NA")<<','<<stateHash(a)<<','<<s.cacheDecision<<'\n';if(s.frame%30==0)a.trace.flush();
 if(s.frame==1 || s.frame%15==0 || s.queryBuilds>0 || a.paused) {
  std::ostringstream event;
  event<<"F"<<s.frame<<" "<<s.cacheDecision<<" "<<s.queryBuilds<<"B/"<<s.queryReuses<<"R";
  pushInspectorEvent(a,event.str());
  std::ostringstream workload;
  workload<<"PAIRS "<<s.candidateCount<<" / PRECISE "<<s.preciseTests<<" / NEAR "<<s.proximityCount;
  pushInspectorEvent(a,workload.str());
  std::ostringstream ids;ids<<"S-T IDS:";
  for(std::size_t i=0;i<std::min<std::size_t>(4,s.candidateTriangles.size());++i)
    ids<<" "<<s.candidateTriangles[i];
  if(s.candidateTriangles.size()>4)ids<<" ...";
  pushInspectorEvent(a,ids.str());
 }
 if(s.oracleMisses){a.paused=true;logEvent(a,"ORACLE_FAILURE");}}
void key(GLFWwindow*w,int key,int,int action,int){
 if(action!=GLFW_PRESS)return;
 auto&a=*static_cast<App*>(glfwGetWindowUserPointer(w));
 if(key==GLFW_KEY_ESCAPE){glfwSetWindowShouldClose(w,1);return;}
 if(key==GLFW_KEY_K){
  a.multi.endGrab();a.clothMouseGrab=false;a.drag=false;
  a.multiMode=!a.multiMode;
  a.historyCandidates.clear();a.historyInterContacts.clear();a.historyBroadMs.clear();
  a.paused=false;a.evidenceMode=false;a.inspectorScroll=0;
  pushInspectorEvent(a,a.multiMode?"DYNAMIC 3-SHEET SCENE":"SINGLE CLOTH BASELINE");
  if(a.multiMode)a.multi.setHierarchicalRegions(a.multiHierarchy);
  fitScene(a,w);return;
 }
 if(a.multiMode){
  if(key==GLFW_KEY_SPACE)a.paused=!a.paused;
  else if(key==GLFW_KEY_S){
   a.multi.setSelfCollisions(!a.multi.selfCollisions());
   pushInspectorEvent(a,a.multi.selfCollisions()?
     "SELF COLLISION ENABLED":"SELF COLLISION DISABLED");
  }
  else if(key==GLFW_KEY_N){a.paused=true;stepSimulation(a);}
  else if(key==GLFW_KEY_R){a.clothMouseGrab=false;a.drag=false;
    a.multi.reset(a.multi.sheetCount(),a.multi.sphereCount());a.historyCandidates.clear();a.historyInterContacts.clear();a.historyBroadMs.clear();fitScene(a,w);}
  else if(key==GLFW_KEY_L){a.clothMouseGrab=false;a.drag=false;
    a.multi.reset(a.multi.sheetCount()%3+1,a.multi.sphereCount());a.historyCandidates.clear();a.historyInterContacts.clear();a.historyBroadMs.clear();fitScene(a,w);}
  else if(key==GLFW_KEY_O){a.clothMouseGrab=false;a.drag=false;
    a.multi.reset(a.multi.sheetCount(),a.multi.sphereCount()==1?2:1);a.historyCandidates.clear();a.historyInterContacts.clear();a.historyBroadMs.clear();fitScene(a,w);}
  else if(key==GLFW_KEY_Q){
   using P=ccp::research::Policy;
   a.multi.setPolicy(a.multi.policy()==P::RebuildSAP?P::GlobalGuard:
                     a.multi.policy()==P::GlobalGuard?P::RegionalCertified:
                     a.multi.policy()==P::RegionalCertified?P::OnlineContactAware:P::RebuildSAP);
   pushInspectorEvent(a,"CHANGED BROAD PHASE POLICY");
  } else if(key==GLFW_KEY_A){
   a.multiAudit=!a.multiAudit;a.multi.setAuditing(a.multiAudit);
   pushInspectorEvent(a,a.multiAudit?"EXHAUSTIVE COVERAGE AUDIT ON":"COVERAGE AUDIT OFF");
  } else if(key==GLFW_KEY_T){
   using P=ccp::research::Policy;
   a.multi.setPolicy(a.multi.onlineEnabled()?P::RegionalCertified:P::OnlineContactAware);
   pushInspectorEvent(a,a.multi.onlineEnabled()?"ONLINE SPLIT/MERGE ON":"ONLINE CONTROL OFF");
  } else if(key==GLFW_KEY_Z){
   if(a.multi.onlineEnabled())a.multi.setPolicy(ccp::research::Policy::RegionalCertified);
   a.multiAdaptive=!a.multiAdaptive;
   a.multi.setAdaptive(a.multiAdaptive);
   pushInspectorEvent(a,a.multiAdaptive?"ADAPTIVE REGION BUDGET":"FIXED REGION BUDGET");
  } else if(key==GLFW_KEY_J){
   if(a.multi.onlineEnabled())a.multi.setPolicy(ccp::research::Policy::RegionalCertified);
   a.multiHierarchy=!a.multiHierarchy;
   a.multi.setHierarchicalRegions(a.multiHierarchy);
   pushInspectorEvent(a,a.multiHierarchy?"TWO-TIER REGION FRONTIER":"ALL-REGION-PAIRS ABLATION");
  } else if(key==GLFW_KEY_Y){
   if(a.multi.onlineEnabled())a.multi.setPolicy(ccp::research::Policy::RegionalCertified);
   loadEvidence(a);
   applyCalibratedPatch(a);
  } else if(key==GLFW_KEY_P){
   if(a.multi.onlineEnabled())a.multi.setPolicy(ccp::research::Policy::RegionalCertified);
   const std::array<int,6> spans={1,2,3,4,6,12};
   const auto it=std::find(spans.begin(),spans.end(),a.multi.patchCellSpan());
   const std::size_t next=(std::size_t(it-spans.begin())+1)%spans.size();
   a.multi.setPatchCellSpan(spans[next]);
   pushInspectorEvent(a,"MESH PATCH SPAN "+std::to_string(spans[next]));
  } else if(key==GLFW_KEY_W)a.wire=!a.wire;
  else if(key==GLFW_KEY_F)fitScene(a,w);
  else if(key==GLFW_KEY_G){a.evidenceMode=!a.evidenceMode;a.inspectorScroll=0;loadEvidence(a);}
  else if(key>=GLFW_KEY_1&&key<=GLFW_KEY_4)a.stage=key-GLFW_KEY_0;
  return;
 }
if(key==GLFW_KEY_SPACE)a.paused=!a.paused;if(key==GLFW_KEY_N){a.paused=true;stepSimulation(a);}if(key==GLFW_KEY_R){bool mode=a.sim.stats().sap,collide=a.sim.stats().collisions;auto request=a.sim.queryRequest();a.sim.reset();a.sim.setQueryRequest(request);a.sim.setBroadPhase(mode);a.sim.setCollisions(collide);a.cutaway=false;fitScene(a,w);}if(key==GLFW_KEY_F)fitScene(a,w);if(key==GLFW_KEY_B)a.sim.setBroadPhase(!a.sim.stats().sap);if(key==GLFW_KEY_C)a.sim.setCollisions(!a.sim.stats().collisions);if(key==GLFW_KEY_V)a.sim.setOracleEnabled(!a.sim.oracleEnabled());
if(key==GLFW_KEY_G){a.evidenceMode=!a.evidenceMode;a.inspectorScroll=0;loadEvidence(a);}
if(key==GLFW_KEY_H){a.evidenceScene=(a.evidenceScene+1)%6;a.inspectorScroll=0;}if(key==GLFW_KEY_Q){auto q=a.sim.queryRequest();q.updatePolicy=q.updatePolicy==ccp::live::UpdatePolicy::GuardedReuse?ccp::live::UpdatePolicy::RebuildEveryIteration:ccp::live::UpdatePolicy::GuardedReuse;a.sim.setQueryRequest(q);}if(key==GLFW_KEY_M){auto q=a.sim.queryRequest();q.margin=q.margin<.03?.05:q.margin<.08?.10:.02;a.sim.setQueryRequest(q);}if(key==GLFW_KEY_W)a.wire=!a.wire;if(key==GLFW_KEY_X)a.cutaway=!a.cutaway;if(key>=GLFW_KEY_1&&key<=GLFW_KEY_4)a.stage=key-GLFW_KEY_0;logEvent(a,"KEY "+std::to_string(key));}
// Build a picking ray with the same view and .70-radian vertical FOV as
// the OpenGL camera. GLFW cursor coordinates are WINDOW pixels, not retina
// framebuffer pixels; ray aspect must therefore use glfwGetWindowSize().
ccp::interaction::Ray rayForCursor(const App& a,GLFWwindow* w,double x,double y){
 int width=0,height=0;glfwGetWindowSize(w,&width,&height);
 const double viewport=std::max(1.0,width-350.0*height/920.0);
 const V eye=add(a.target,{
   a.distance*std::cos(a.pitch)*std::cos(a.yaw),
   a.distance*std::cos(a.pitch)*std::sin(a.yaw),
   a.distance*std::sin(a.pitch)});
 return ccp::interaction::cameraRay(x,y,viewport,height,eye,a.target,.70);
}
void mouse(GLFWwindow*w,int button,int action,int mods){
 auto& a=*static_cast<App*>(glfwGetWindowUserPointer(w));
 glfwGetCursorPos(w,&a.lastX,&a.lastY);
 if(action==GLFW_RELEASE){
  if(button==GLFW_MOUSE_BUTTON_LEFT){
   a.drag=false;
   if(a.clothMouseGrab){
    a.multi.endGrab();a.clothMouseGrab=false;
    pushInspectorEvent(a,"MOUSE RELEASED CLOTH");
   }
  }
  if(button==GLFW_MOUSE_BUTTON_RIGHT)a.drag=false;
  return;
 }
 if(action!=GLFW_PRESS)return;
 if(button==GLFW_MOUSE_BUTTON_RIGHT){a.drag=true;return;}
 if(button!=GLFW_MOUSE_BUTTON_LEFT)return;
 int width=0,height=0;glfwGetWindowSize(w,&width,&height);
 const double viewport=width-350.0*height/920.0;
 if(a.lastX>=viewport)return; // do not pick through the right inspector
 // Right-drag or Shift-left drag orbits the camera.
 if(!a.multiMode||(mods&GLFW_MOD_SHIFT)){a.drag=true;return;}
 const auto ray=rayForCursor(a,w,a.lastX,a.lastY);
 const auto hit=ccp::interaction::pickTriangleRay(
   ray,a.multi.positions(),a.multi.triangles());
 if(hit&&a.multi.beginGrab(hit->triangle,hit->barycentric,hit->position)){
  a.clothMouseGrab=true;
  a.dragOrigin=hit->position;
  a.dragPlaneNormal=norm(sub(a.target,ray.origin));
  pushInspectorEvent(a,"MOUSE GRABBED SHEET "+
    std::to_string(a.multi.triangleSheets()[hit->triangle]+1)+
    " TRIANGLE "+std::to_string(hit->triangle));
 }else a.drag=true; // empty background still rotates the camera
}
void cursor(GLFWwindow*w,double x,double y){
 auto& a=*static_cast<App*>(glfwGetWindowUserPointer(w));
 if(a.clothMouseGrab){
  // Drag in a camera-facing plane at the selected cloth depth. This
  // allows lifting in 3D, not just sliding along the floor.
  const auto ray=rayForCursor(a,w,x,y);
  const auto target=ccp::interaction::intersectDragPlane(
     ray,a.dragOrigin,a.dragPlaneNormal);
  if(target)a.multi.updateGrab(*target);
 }else if(a.drag){
  a.yaw+=(x-a.lastX)*.006;
  a.pitch=std::clamp(a.pitch+(y-a.lastY)*.005,-.1,1.35);
 }
 a.lastX=x;a.lastY=y;
}
void scroll(GLFWwindow*w,double,double dy){
 auto&a=*static_cast<App*>(glfwGetWindowUserPointer(w));
 int windowWidth=0,windowHeight=0;
 glfwGetWindowSize(w,&windowWidth,&windowHeight);
 const double panelWidth=350.0*windowHeight/920.0;
 // Mouse wheel over right inspector scrolls benchmark evidence; over the
 // 3D viewport it still zooms the camera.
 if(a.evidenceMode&&a.lastX>windowWidth-panelWidth) {
  a.inspectorScroll=std::clamp(a.inspectorScroll-dy*65.0,0.0,1230.0);
 } else a.distance=std::clamp(a.distance-dy*.3,2.2,12.);
}
extern "C" unsigned char* ccpFontAtlas(int*,int*,float*);
float fontAdvance[96]{};std::vector<Vertex>glyphVertices;
void rect(std::vector<Vertex>&v,float x,float y,float w,float h,V c){triangle(v,{x,y,0},{x+w,y,0},{x+w,y+h,0},c);triangle(v,{x,y,0},{x+w,y+h,0},{x,y+h,0},c);}
void text(std::vector<Vertex>&,std::string str,float x,float y,float size,V color){float f=size*8/32;for(unsigned char ch:str){int i=ch-32;if(i<0||i>=96)continue;float u=(i%16)/16.f,v=(5-i/16)/6.f,du=1/16.f,dv=1/6.f;float w=48*f,h=56*f;float top=y-12*f;
 vertex(glyphVertices,{x,top,0},{u,v,0},color);vertex(glyphVertices,{x+w,top,0},{u+du,v,0},color);vertex(glyphVertices,{x+w,top+h,0},{u+du,v+dv,0},color);vertex(glyphVertices,{x,top,0},{u,v,0},color);vertex(glyphVertices,{x+w,top+h,0},{u+du,v+dv,0},color);vertex(glyphVertices,{x,top+h,0},{u,v+dv,0},color);x+=fontAdvance[i]*f;}}
const char*tvs=R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 uv;layout(location=2)in vec3 color;uniform mat4 vp;out vec2 UV;out vec3 C;void main(){UV=uv.xy;C=color;gl_Position=vp*vec4(position,1);})";
const char*tfs=R"(#version 330 core
in vec2 UV;in vec3 C;uniform sampler2D atlas;out vec4 frag;void main(){frag=vec4(C,texture(atlas,UV).a);})";
std::string number(double d,int precision=3){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<d;return s.str();}
void uploadDraw(GLuint vao,GLuint buffer,const std::vector<Vertex>&v,GLenum type){glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(Vertex),v.data(),GL_DYNAMIC_DRAW);glDrawArrays(type,0,(GLsizei)v.size());}
void capture(int w,int h,const std::string&path){std::vector<unsigned char> pixels(w*h*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";for(int y=h-1;y>=0;y--)out.write(reinterpret_cast<char*>(pixels.data()+y*w*3),w*3);std::cout<<"GPU_FRAMEBUFFER_CAPTURE "<<path<<'\n';}


// All charts are generated exclusively from measured C++ CSVs. No fabricated
// "research speedups" or animations are displayed in this evidence inspector.
void drawMulticlothInspector(const App&a,float viewport,float panel,float scale,int width,int height,
 GLuint vao,GLuint buffer,GLuint lines,GLuint fontProgram,GLuint fontTexture,const M& ui){
 std::vector<Vertex> mesh,spark;glyphVertices.clear();
 const float x=viewport+14*scale,scroll=float(a.inspectorScroll*scale),maxBar=panel-28*scale;
 const V ink{.12,.17,.23},muted{.37,.44,.52},blue{.13,.37,.68},teal{.05,.50,.41},amber{.72,.42,.13},red{.75,.22,.19};
 rect(mesh,viewport,0,panel,height,{.969,.976,.987});
 auto line=[&](std::string content,float y,V c=V{.3,.38,.47},float size=1.4){
  if(content.size()>48)content.resize(48);
  text(mesh,content,x,y*scale-scroll,size*scale,c);
 };
 auto heading=[&](std::string content,float y){
  line(content,y,blue,1.75);
 };
 const auto& m=a.multi.metrics();
 heading("EXPERIMENTAL REGIONAL BROAD PHASE",26);
 line("K SINGLE CLOTH / L SHEETS / O BALLS",54,muted,1.20);
 heading("LIVE DYNAMIC PHYSICS / NOT REPLAY",94);
 line("FRAME "+std::to_string(a.multi.frame())+" / "+
     std::to_string(a.multi.sheetCount())+" SHEETS",124,ink);
 line("MOVING OBSTACLES "+std::to_string(a.multi.sphereCount())+
      (a.multi.onlineEnabled()?" / ONLINE PATCHES "+std::to_string(m.onlinePatches):
       " / FIXED PATCH "+std::to_string(a.multi.patchCellSpan())),150,ink,1.17);
 line("CONTACT CORRECTIONS / 5 ITER "+std::to_string(m.contacts),176,teal,1.15);
 line("INTERSHEET CORRECTIONS "+std::to_string(m.interSheetContacts),202,teal);
 line("LAST-QUERY PAIRS "+std::to_string(m.pairs)+" / TESTS "+std::to_string(m.preciseTests),228,ink,1.15);
 line("REGION INVALIDATIONS / STEP "+std::to_string(m.dirtyRegions),254,ink,1.20);
 line("LIVE SPLITS "+std::to_string(m.onlineSplits)+" MERGES "+
      std::to_string(m.onlineMerges)+" SWITCHES "+
      std::to_string(m.onlineStrategySwitches),380,teal,1.19);
 line("PAIR BLOCK BUILDS "+std::to_string(m.regionalPairRebuilds),280);
 line("PAIR BLOCK REUSES "+std::to_string(m.regionalPairReuses),306);
 line("GUARD EXTRAS REJECTED "+std::to_string(m.cachedPairsRejected),332);
 line("MEAN GUARD BUDGET "+number(m.meanRegionBudget,4),358);
 line("PHYSICAL STRAIN "+number(m.maxStructuralEdgeStretch,3)+
      " FRAME DELTA "+number(m.maxFrameParticleTravel,3),366,teal,1.12);
 line("PATCH FRONTIER "+std::to_string(m.regionTopLevelOverlaps)+
      " / SKIPPED "+std::to_string(m.regionPairsSkipped),404,ink,1.18);
 line("ONLINE WORK "+number(m.onlineMeasuredWork,0)+
      " HEAT "+number(m.onlineContactHeat,0),415,teal,1.12);
 line(m.oracleChecked?"EXHAUSTIVE AABB COVERAGE VERIFIED":"ORACLE NOT CHECKED",385,
      m.oracleChecked?teal:amber,1.26);
 heading("SCROLLABLE LIVE EVENT TRACE",427);
 const std::size_t count=std::min<std::size_t>(a.recentEvents.size(),12);
 for(std::size_t j=0;j<count;++j)
  line(a.recentEvents[a.recentEvents.size()-count+j],455+23*float(j),muted,1.18);
 heading("MEASURED 1/2/3 CLOTH COMPARISON",774);
 line("SAME FORCES + PRECISE SOLVER",799,muted,1.18);
 line("BENCHMARKED ON THIS MACHINE",821,muted,1.20);
 const std::array<std::string,7> policies={
  "sap_rebuild","global_guarded","region_fixed","region_adaptive",
  "region_sweep_fixed","region_sweep_adaptive","online_contact_work"};
 const std::array<V,7> colors={
  amber,blue,{.53,.36,.69},teal,{.17,.47,.69},{.14,.42,.34},{.83,.33,.14}};
 double maximum=0;
 for(const auto& row:a.multiResults)
  if(row.sheets==a.multi.sheetCount())maximum=std::max(maximum,row.stepMs);
 if(maximum>0){
  for(std::size_t i=0;i<policies.size();++i){
   auto it=std::find_if(a.multiResults.begin(),a.multiResults.end(),
      [&](const MultiBenchRow& row){return row.sheets==a.multi.sheetCount()&&row.policy==policies[i];});
   if(it==a.multiResults.end())continue;
   const float y=861+float(i)*72;
   line(it->policy+" "+number(it->stepMs,3)+"MS",y,ink,1.24);
   const float bw=float(std::clamp(it->stepMs/maximum,0.0,1.0))*std::max(12.0f,maxBar);
   rect(mesh,x,(y+19)*scale-scroll,bw,12*scale,colors[i]);
   line("BROAD "+number(it->broadMs,3)+" PRECISE "+number(it->preciseMs,3),y+47,muted,1.12);
  }
 }else{
  line("NO VERIFIED LOCAL MULTICLOTH CSV",869,red,1.22);
  line("RUN scripts/prepare_meeting.sh",896);
 }
 // This is an OFFLINE chosen static region size: calibration is earlier
 // physics frames; evaluation consists of later frames. No online learning.
 if(!a.patchResults.empty()){
  auto winner=a.patchResults.end();
  for(auto it=a.patchResults.begin();it!=a.patchResults.end();++it)
   if(it->phase=="calibration" &&
      (winner==a.patchResults.end() || it->meanStepMs<winner->meanStepMs))
    winner=it;
  if(winner!=a.patchResults.end()){
   line("PATCH PROFILE (EARLY TRAIN)",1300,blue,1.34);
   line("CHOSEN "+winner->frontier+" "+std::to_string(winner->span),1322,ink,1.13);
   const auto evaluated=std::find_if(a.patchResults.begin(),a.patchResults.end(),
     [&](const PatchBenchRow& e){
      return e.phase=="evaluation" && e.span==winner->span &&
             e.frontier==winner->frontier;
     });
   if(evaluated!=a.patchResults.end())
    line("LATER EVAL STEP "+number(evaluated->meanStepMs,3)+" MS",1346,teal,1.25);
  }
 }
 heading("PROPOSED ALGORITHM / AUDITABLE",1387);
 line("1  PARTITION STABLE MESH PATCHES",1417,ink,1.22);
 line("2  CERTIFY PATCH ENVELOPE",1442,ink,1.22);
 line("3  SWEEP REGIONAL ENVELOPE PAIRS",1467,ink,1.22);
 line("4  SELECTIVELY REBUILD BLOCKS",1492,ink,1.22);
 line("5  REVALIDATE CURRENT TIGHT AABBS",1692,ink,1.22);
 line("6  OPTIONAL LOCAL GUARD FEEDBACK",1539,ink,1.22);
 line("   MOTION / CANDIDATE OVERHEAD",1562,ink,1.22);
 heading("RESEARCH NOVELTY IS UNPROVEN",1609);
 line("PRIOR BVH REGION AND TEMPORAL",1639,amber,1.18);
 line("UPDATE METHODS ALREADY EXIST",1662,amber,1.18);
 line("NEW TESTABLE CLAIM: BETTER",1692,teal,1.17);
 line("TOTAL CONTACT WORK + NO MISSES",1715,teal,1.17);
 line("NOT CCD / NOT IPC OR OGC",1746,red,1.18);
 heading("CONTACT DENSITY / TIME SERIES",1781);
 line("TEAL: INTER-SHEET / BLUE: CANDIDATES",1804,muted,1.08);
 if(a.historyCandidates.size()>1){
  const auto maxC=*std::max_element(a.historyCandidates.begin(),a.historyCandidates.end());
  const auto maxI=*std::max_element(a.historyInterContacts.begin(),a.historyInterContacts.end());
  const double width=std::max(10.0f,maxBar-4*scale);
  const double base=1892.0*scale-scroll;
  const double heightRange=68.0*scale;
  for(std::size_t i=1;i<a.historyCandidates.size();++i){
   const double ax=x+width*double(i-1)/double(a.historyCandidates.size()-1);
   const double bx=x+width*double(i)/double(a.historyCandidates.size()-1);
   auto plot=[&](const std::vector<double>& data,double maxValue,V color){
    const double ay=base-heightRange*data[i-1]/std::max(1.0,maxValue);
    const double by=base-heightRange*data[i]/std::max(1.0,maxValue);
    segment(spark,{ax,ay,0},{bx,by,0},color);
   };
   plot(a.historyCandidates,maxC,blue);
   plot(a.historyInterContacts,maxI,teal);
  }
 }
 glEnable(GL_SCISSOR_TEST);glScissor((int)viewport,0,(int)panel,height);
 glUseProgram(lines);
 glUniformMatrix4fv(glGetUniformLocation(lines,"vp"),1,GL_FALSE,ui.a);
 uploadDraw(vao,buffer,mesh,GL_TRIANGLES);
 uploadDraw(vao,buffer,spark,GL_LINES);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glUseProgram(fontProgram);
 glUniformMatrix4fv(glGetUniformLocation(fontProgram,"vp"),1,GL_FALSE,ui.a);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,fontTexture);
 glUniform1i(glGetUniformLocation(fontProgram,"atlas"),0);
 uploadDraw(vao,buffer,glyphVertices,GL_TRIANGLES);
 glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);
}

// Evidence mode uses the same GPU framebuffer and glyph atlas as the native
// viewer. It is a scrollable scientific pane, NOT a separate webpage.
void drawEvidencePane(const App& a,float viewport,float panel,float scale,int width,int height,
                      GLuint vao,GLuint buffer,GLuint lines,GLuint fontProgram,GLuint fontTexture,const M& ui) {
 if(a.multiMode){
  drawMulticlothInspector(a,viewport,panel,scale,width,height,
                          vao,buffer,lines,fontProgram,fontTexture,ui);
  return;
 }
 std::vector<Vertex> shapes;
 glyphVertices.clear();
 const float x=viewport+13*scale;
 const float lineWidth=panel-26*scale;
 const float scroll=float(a.inspectorScroll*scale);
 const V ink{.12,.17,.24},muted{.35,.41,.49},teal{.07,.47,.41},blue{.12,.35,.66},amber{.68,.39,.10},purple{.43,.25,.67};
 rect(shapes,viewport,0,panel,height,{.965,.975,.985});
 auto heading=[&](std::string str,float offset,V color=V{.12,.35,.66}) {
  text(shapes,str,x,offset*scale-scroll,1.80*scale,color);
 };
 auto line=[&](std::string str,float offset,V color=V{.25,.33,.43},float sz=1.38) {
  if(str.size()>51)str.resize(51);
  text(shapes,str,x,offset*scale-scroll,sz*scale,color);
 };
 auto bar=[&](std::string name,double value,double maxValue,float top,V color) {
  line(name+" "+number(value,5)+" MS",top,ink,1.36);
  const float bw=float((std::max(0.0,value)/std::max(maxValue,1e-9)))*std::max(20.0f,lineWidth-8*scale);
  rect(shapes,x,(top+22)*scale-scroll,bw,10*scale,color);
 };
 heading("RESEARCH / SCROLL TO GRAPHS",25);
 line("G: INSPECTOR  H: SCENARIO  WHEEL: SCROLL",52,muted,1.18);
 heading("CANDIDATE CACHE / LIVE",90);
 const auto& stats=a.sim.stats();
 line("FRAME "+std::to_string(stats.frame)+" / "+stats.cacheDecision,119,teal,1.30);
 line("BUILDS "+std::to_string(stats.queryBuilds)+"  REUSES "+std::to_string(stats.queryReuses),145);
 line("FINAL-QUERY CANDIDATES "+std::to_string(stats.candidateCount),171);
 line("PRECISE TESTS / 8 ITER "+std::to_string(stats.preciseTests),197);
 line("WITHIN MARGIN "+std::to_string(stats.proximityCount),223);
 line("ORACLE "+std::string(stats.oracleChecked?"CHECKED":"NOT CHECKED"),249,stats.oracleChecked?teal:amber);
 heading("LIVE PAIR + CACHE EVENTS",291);
 std::size_t count=std::min<std::size_t>(a.recentEvents.size(),11);
 for(std::size_t j=0;j<count;++j)
  line(a.recentEvents[a.recentEvents.size()-count+j],319+22*float(j),muted,1.23);
 heading("REAL CLOTH CPU STEP / A-B",592);
 line("SAME SOLVER / REBUILD VS GUARDED",621,muted,1.12);
 if(std::isfinite(a.rebuildMean)&&std::isfinite(a.guardedMean)) {
  const double maximum=std::max(a.rebuildMean,a.guardedMean);
  bar("REBUILD",a.rebuildMean,maximum,655,amber);
  bar("GUARDED",a.guardedMean,maximum,710,teal);
  line("SAVED BUILD WORK + PRECISE WORK",763,muted,1.15);
  line("TIMED: FINAL DISPLAY SCAN INCLUDED",786,muted,1.12);
 } else {
  line("NO LOCAL LIVE BENCHMARK FOUND",662,amber);
  line("RUN scripts/prepare_meeting.sh",686);
 }
 heading("EXTERNAL 3D / BROAD PHASE ONLY",845);
 const std::array<std::string,6> cases={"sparse256","clustered512","dense256","sheets2","sheets3","simulated_cloths3"};
 const auto selected=cases[std::clamp(a.evidenceScene,0,4)];
 line("SCENE: "+selected+"  /  H TO CHANGE",874,muted,1.22);
 line("ACTUAL BULLET DBVT / NOT A CLAIM OF BEST",897,purple,1.13);
 const std::array<std::string,9> methods={
  "exhaustive_aabb","sweep_prune","bullet_dbvt","bullet_dbvt_dynamic",
  "guarded_sap","region_fixed","region_adaptive",
  "region_sweep_fixed","region_sweep_adaptive"};
 const std::array<V,9> colors={
  amber,blue,purple,{.24,.44,.68},teal,{.58,.31,.54},{.15,.42,.36},
  {.14,.51,.72},{.12,.38,.30}};
 double maxMs=0;
 for(const auto& row:a.broadphaseResults)
  if(row.scene==selected)maxMs=std::max(maxMs,row.meanMs);
 if(maxMs>0) {
  for(std::size_t i=0;i<methods.size();++i) {
   auto it=std::find_if(a.broadphaseResults.begin(),a.broadphaseResults.end(),
       [&](const EvidenceBar& row){return row.scene==selected&&row.algorithm==methods[i];});
   if(it!=a.broadphaseResults.end())
    bar(methods[i],it->meanMs,maxMs,935+float(i)*38,colors[i]);
  }
  line("GUARD MAY EMIT EXTRA PRECISE PAIRS",1280,muted,1.11);
 } else {
  line("NO PUBLISHED BASELINE CSV",947,amber);
  line("INSTALL BULLET; RUN MEETING PREP",974);
 }
 heading("DOWNSTREAM / REAL MEASURED CASCADE",1322);
 if(std::isfinite(a.rebuildMean)&&std::isfinite(a.guardedMean)) {
  line("MEAN REBUILD / GUARDED",1349,blue,1.18);
  line("BROAD  "+number(a.rebuildBroad,3)+" / "+number(a.guardedBroad,3)+" MS",1374,ink,1.22);
  line("PRECISE "+number(a.rebuildNarrow,3)+" / "+number(a.guardedNarrow,3)+" MS",1399,ink,1.22);
  line("TESTS "+number(a.rebuildPrecise,0)+" / "+number(a.guardedPrecise,0),1424,ink,1.22);
  line("STEP  "+number(a.rebuildMean,3)+" / "+number(a.guardedMean,3)+" MS",1449,ink,1.22);
 }
 line("EXTRA CACHED PAIRS MAY INCREASE",1479,muted,1.13);
 line("PRECISE TESTS. NOT FREE SPEEDUP.",1501,muted,1.12);
 heading("NOVELTY / RESEARCH BOUNDARY",1549);
 line("NOT NOVEL: SAP, BVH, FAT AABBS",1579,muted,1.18);
 line("HYPOTHESIS: METHOD-AWARE CERTIFIED",1603,teal,1.18);
 line("REUSE ACROSS DISTINCT CONTACT MODELS",1625,teal,1.16);
 line("NOT YET VERIFIED ACROSS IPC/OGC",1648,amber,1.15);
 // Clip ALL evidence drawing and text to the real right-side inspector.
 glEnable(GL_SCISSOR_TEST);
 glScissor((int)viewport,0,(int)panel,height);
 glUseProgram(lines);
 glUniformMatrix4fv(glGetUniformLocation(lines,"vp"),1,GL_FALSE,ui.a);
 uploadDraw(vao,buffer,shapes,GL_TRIANGLES);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glUseProgram(fontProgram);
 glUniformMatrix4fv(glGetUniformLocation(fontProgram,"vp"),1,GL_FALSE,ui.a);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,fontTexture);
 glUniform1i(glGetUniformLocation(fontProgram,"atlas"),0);
 uploadDraw(vao,buffer,glyphVertices,GL_TRIANGLES);
 glDisable(GL_BLEND);
 glDisable(GL_SCISSOR_TEST);
}
int main(int argc,char**argv){try{
 const auto runtimeDir=std::filesystem::temp_directory_path()/"ccp-native-lab";std::filesystem::create_directories(runtimeDir);
 int captureSteps=-1;std::string capturePath,tracePath=(runtimeDir/"native_trace.csv").string();bool closeCapture=false;App a;
 for(int i=1;i<argc;i++){std::string arg=argv[i];if(arg=="--multicloth"){a.multiMode=true;a.multi.setHierarchicalRegions(true);}
  else if(arg=="--online"){a.multiMode=true;a.multi.setPolicy(ccp::research::Policy::OnlineContactAware);}
  else if(arg=="--paused")a.paused=true;else if(arg=="--baseline"){auto q=a.sim.queryRequest();q.updatePolicy=ccp::live::UpdatePolicy::RebuildEveryIteration;a.sim.setQueryRequest(q);}else if(arg=="--reference")a.sim.setBroadPhase(false);else if(arg=="--trace"&&i+1<argc)tracePath=argv[++i];else if(arg=="--capture-after"&&i+1<argc)captureSteps=std::stoi(argv[++i]);else if(arg=="--capture"&&i+1<argc)capturePath=argv[++i];else if(arg=="--exit-after-capture")closeCapture=true;else if(arg=="--stage"&&i+1<argc)a.stage=std::stoi(argv[++i]);else if(arg=="--cutaway")a.cutaway=true;}
 glfwSetErrorCallback([](int,const char*e){std::cerr<<"GLFW "<<e<<'\n';});if(!glfwInit())throw std::runtime_error("glfwInit failed");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
#ifdef __APPLE__
 glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#endif
 glfwWindowHint(GLFW_SAMPLES,4);auto*w=glfwCreateWindow(1480,920,"CCP Native - LIVE C++ common broad phase",nullptr,nullptr);if(!w)throw std::runtime_error("Native OpenGL window creation failed");glfwMakeContextCurrent(w);glfwSwapInterval(1);glfwSetWindowUserPointer(w,&a);glfwSetKeyCallback(w,key);glfwSetMouseButtonCallback(w,mouse);glfwSetCursorPosCallback(w,cursor);glfwSetScrollCallback(w,scroll);
 std::cout<<"NATIVE_GPU "<<glGetString(GL_RENDERER)<<" OPENGL "<<glGetString(GL_VERSION)<<'\n';
 GLuint surface=program(vs,fs),depth=program(dvs,dfs),lines=program(lvs,lfs),fontProgram=program(tvs,tfs),vao,buffer;glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);for(int i=0;i<3;i++){glEnableVertexAttribArray(i);glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(i*3*sizeof(float)));}
 int aw,ah;unsigned char*atlas=ccpFontAtlas(&aw,&ah,fontAdvance);GLuint fontTexture;glGenTextures(1,&fontTexture);glBindTexture(GL_TEXTURE_2D,fontTexture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,aw,ah,0,GL_RGBA,GL_UNSIGNED_BYTE,atlas);std::free(atlas);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 GLuint fbo,shadow;glGenFramebuffers(1,&fbo);glGenTextures(1,&shadow);glBindTexture(GL_TEXTURE_2D,shadow);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,2048,2048,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);float white[4]={1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,white);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadow,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Shadow framebuffer incomplete");glBindFramebuffer(GL_FRAMEBUFFER,0);
 a.trace.open(tracePath);a.events.open(runtimeDir/"native_events.log",std::ios::app);if(!a.trace||!a.events)throw std::runtime_error("Cannot open runtime diagnostic logs");std::cout<<"RUNTIME_LOGS "<<runtimeDir<<std::endl;a.trace<<"frame,broadphase,response,policy,margin,displacementBudget,builds,reuses,candidates,proximity,minGap,broadMs,narrowMs,oracleMs,diagnosticMs,otherMs,preciseTests,oracleTests,oracleChecked,stepMs,oracleMisses,positionHash,cacheDecision\n";
 loadEvidence(a);
 auto advance=[&](){stepSimulation(a);};fitScene(a,w);
 if(captureSteps>=0){for(int i=0;i<captureSteps;i++)advance();a.paused=true;logEvent(a,"CAPTURE_STATE");}
 double previous=glfwGetTime(),accumulator=0,renderFps=0,fpsStart=previous;int fpsFrames=0;int rendered=0;M lightVP=product(ortho(-3.8,3.8,-3.8,3.8,.1,18),look({4,-3,7},{0,0,.8},{0,0,1}));
 while(!glfwWindowShouldClose(w)){
  double now=glfwGetTime(),elapsed=now-previous;previous=now;fpsFrames++;if(now-fpsStart>=.5){renderFps=fpsFrames/(now-fpsStart);fpsFrames=0;fpsStart=now;}glfwPollEvents();if(!a.paused){accumulator+=std::min(elapsed,.05);int budget=0;while(accumulator>=1./120&&budget++<6){advance();accumulator-=1./120;}}else accumulator=0;
  int width,height;glfwGetFramebufferSize(w,&width,&height);if(width<10||height<10)continue;float scale=height/920.f,panel=350*scale,viewport=width-panel;const auto&pos=a.multiMode?a.multi.positions():a.sim.positions();
  const auto&tris=a.multiMode?a.multi.triangles():a.sim.triangles();
  const auto&s=a.sim.stats();
  std::vector<V> normals(pos.size());for(auto tri:tris){V n=cross(sub(pos[tri[1]],pos[tri[0]]),sub(pos[tri[2]],pos[tri[0]]));for(int id:tri)normals[id]=add(normals[id],n);}for(auto&n:normals)n=norm(n);std::vector<bool>cand(tris.size()),near(tris.size());if(a.multiMode){
   for(const auto pair:a.multi.metrics().candidatePairs){
    if(pair.a>=0&&pair.a<(int)cand.size())cand[pair.a]=true;
    if(pair.b>=0&&pair.b<(int)cand.size())cand[pair.b]=true;
   }
   for(const auto pair:a.multi.metrics().contactPairs){
    if(pair.a>=0&&pair.a<(int)near.size())near[pair.a]=true;
    if(pair.b>=0&&pair.b<(int)near.size())near[pair.b]=true;
   }
  }else{
   for(int id:s.candidateTriangles)if(id>=0&&id<(int)cand.size())cand[id]=true;
   for(int id:s.contactTriangles)if(id>=0&&id<(int)near.size())near[id]=true;
  }
  std::vector<Vertex>solid,overlay;V floor{.54,.59,.63};triangle(solid,{-5,-5,0},{5,-5,0},{5,5,0},floor);triangle(solid,{-5,-5,0},{5,5,0},{-5,5,0},floor);if(a.multiMode){
   for(const auto& b:a.multi.spheres())sphere(solid,b.center,b.radius);
  }else sphere(solid,a.sim.sphereCenter(),a.sim.sphereRadius());
  for(size_t i=0;i<tris.size();i++){auto t=tris[i];V center=mul(add(add(pos[t[0]],pos[t[1]]),pos[t[2]]),1./3);if(a.cutaway&&center.x>0&&center.y<.2)continue;V color{.025,.24,.31};
    if(a.multiMode){
      const auto sheet=a.multi.triangleSheets()[i];
      const std::array<V,3> sheetColors={V{.04,.31,.47},V{.68,.30,.13},V{.23,.55,.34}};
      color=sheetColors[std::clamp(sheet,0,2)];
    }
    if(a.stage==3)color=cand[i]?V{.88,.38,.055}:V{.33,.39,.43};if(a.stage==4)color=near[i]?V{.045,.51,.25}:V{.33,.39,.43};for(int id:t)vertex(solid,pos[id],normals[id],color);if(a.wire)for(int j=0;j<3;j++)segment(overlay,add(pos[t[j]],mul(normals[t[j]],.001)),add(pos[t[(j+1)%3]],mul(normals[t[(j+1)%3]],.001)),{.07,.13,.15});}
  if(a.stage>=2){
   if(a.multiMode){
    if(a.stage==2){
     int shown=0;
     for(const auto& bound:a.multi.metrics().regionGuards){
      if(shown++>=60)break;
      box(overlay,bound.min,bound.max,{.72,.40,.12});
     }
    }
   }else{
    V c=a.sim.sphereCenter();double r=a.sim.sphereRadius()+a.sim.queryRequest().margin*.5;
    box(overlay,sub(c,{r,r,r}),add(c,{r,r,r}),{.16,.45,.94});
    if(a.stage==2){int shown=0;for(const auto& bound:s.queryBounds){
      if(bound.id==(int)tris.size())continue;
      if(shown++>=40)break;
      box(overlay,bound.min,bound.max,{.87,.48,.12});
    }}
   }
  }

  V eye=add(a.target,{a.distance*std::cos(a.pitch)*std::cos(a.yaw),a.distance*std::cos(a.pitch)*std::sin(a.yaw),a.distance*std::sin(a.pitch)});M vp=product(perspective(.70f,viewport/height,.05,40),look(eye,a.target,{0,0,1}));glEnable(GL_DEPTH_TEST);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1,1);glViewport(0,0,2048,2048);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glClear(GL_DEPTH_BUFFER_BIT);glUseProgram(depth);glUniformMatrix4fv(glGetUniformLocation(depth,"vp"),1,GL_FALSE,lightVP.a);uploadDraw(vao,buffer,solid,GL_TRIANGLES);glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_POLYGON_OFFSET_FILL);glViewport(0,0,width,height);glClearColor(.79,.82,.84,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glViewport(0,0,(int)viewport,height);glUseProgram(surface);glUniformMatrix4fv(glGetUniformLocation(surface,"vp"),1,GL_FALSE,vp.a);glUniformMatrix4fv(glGetUniformLocation(surface,"lightVP"),1,GL_FALSE,lightVP.a);glUniform3f(glGetUniformLocation(surface,"eye"),eye.x,eye.y,eye.z);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,shadow);glUniform1i(glGetUniformLocation(surface,"shadowMap"),0);uploadDraw(vao,buffer,solid,GL_TRIANGLES);glUseProgram(lines);glUniformMatrix4fv(glGetUniformLocation(lines,"vp"),1,GL_FALSE,vp.a);uploadDraw(vao,buffer,overlay,GL_LINES);
  glDisable(GL_DEPTH_TEST);glViewport(0,0,width,height);std::vector<Vertex>hud;glyphVertices.clear();V ink{.14,.19,.24},muted{.39,.45,.50},blue{.12,.37,.71},green{.06,.48,.27},red{.75,.17,.12};rect(hud,viewport,0,panel,height,{.965,.97,.975});rect(hud,0,0,viewport,78*scale,{.96,.97,.975});text(hud,a.multiMode?"CCP RESEARCH / MOVING 3D CLOTH":"CCP NATIVE / LIVE C++",
       28*scale,22*scale,2.7*scale,ink);
  text(hud,a.multiMode?"REGIONAL BROAD PHASE / MULTIPLE INTERACTING CLOTH SHEETS":
       "LIVE C++ SOLVER -> SHARED QUERY -> PRECISE CONTACT",
       28*scale,52*scale,1.55*scale,muted);
  float x=viewport+22*scale,y=27*scale;auto label=[&](std::string str,V color=V{.14,.19,.24},float sz=1.9){text(hud,str,x,y,sz*scale,color);y+=26*scale;};auto gap=[&](){y+=11*scale;};
  if(a.multiMode){
   const auto& m=a.multi.metrics();
   const std::string policy=a.multi.policy()==ccp::research::Policy::RegionalCertified?
      "REGIONAL CERTIFIED":a.multi.policy()==ccp::research::Policy::OnlineContactAware?
      "ONLINE CONTACT-AWARE":a.multi.policy()==ccp::research::Policy::GlobalGuard?
      "GLOBAL GUARDED SAP":"FULL SAP REBUILD";
   label(a.paused?"PAUSED / N TO STEP":"RUNNING / SPACE PAUSES",a.paused?muted:green);
   gap();label("MULTICLOTH / LIVE CONTACT",blue,1.7);
   label("SHEETS "+std::to_string(a.multi.sheetCount())+
         " / MOVING BALLS "+std::to_string(a.multi.sphereCount()),blue,1.6);
   label("Q POLICY: "+policy,blue,1.5);
   label("T: ONLINE PARTITION CONTROLLER",green,1.25);
   label("L SHEETS / O BALLS / R RESET",blue,1.4);
   label(a.multiAdaptive?"Z: ADAPTIVE REGION BUDGET":"Z: FIXED REGION BUDGET",blue,1.28);
   label(a.multiHierarchy?"J: TWO-TIER REGION SWEEP":"J: FLAT REGION-PAIR ABLATION",blue,1.22);
   label("Y: APPLY MEASURED CALIBRATION",blue,1.18);
   label("P: PATCH CELLS "+std::to_string(a.multi.patchCellSpan())+" x "+
         std::to_string(a.multi.patchCellSpan()),blue,1.22);
   label("K SINGLE CLOTH / A PAIR AUDIT",blue,1.35);
   label("FRAME "+std::to_string(a.multi.frame()));
   label("CANDIDATES "+std::to_string(m.pairs),ink,1.6);
   label("INTERSHEET CORRECTIONS "+std::to_string(m.interSheetContacts),green,1.38);
   label("SPHERE CORRECTIONS "+std::to_string(m.sphereContacts),green,1.42);
   label("REGION INVALIDATIONS / STEP "+std::to_string(m.dirtyRegions),ink,1.26);
   label("PAIR BLOCKS REBUILT "+std::to_string(m.regionalPairRebuilds),ink,1.45);
   label("PAIR BLOCKS REUSED "+std::to_string(m.regionalPairReuses),ink,1.45);
   label("EXTRA GUARD PAIRS REJECTED "+std::to_string(m.cachedPairsRejected),ink,1.25);
   label("REGION BUDGET "+number(m.meanRegionBudget,4),muted,1.5);
   if(a.multi.onlineEnabled()){
    label("LIVE PATCHES "+std::to_string(m.onlinePatches),green,1.42);
    label("SPLIT "+std::to_string(m.onlineSplits)+
          " / MERGE "+std::to_string(m.onlineMerges)+
          " / SWITCH "+std::to_string(m.onlineStrategySwitches),green,1.27);
    label("CONTROL COST "+number(m.onlineDecisionMs,3)+" MS",muted,1.28);
   }
   label("PATCH PAIRS SKIPPED "+std::to_string(m.regionPairsSkipped),muted,1.35);
   label("PRECISE TESTS "+std::to_string(m.preciseTests),muted,1.5);
   gap();
   label("BROAD CPU "+number(m.broadMs)+" MS",muted,1.6);
   label("PRECISE CPU "+number(m.narrowMs)+" MS",muted,1.6);
   label("FULL STEP "+number(m.stepMs)+" MS",muted,1.65);
   label("MAX EDGE STRETCH "+number(m.maxStructuralEdgeStretch,2)+"X",green,1.30);
   label("FRAME PARTICLE TRAVEL "+number(m.maxFrameParticleTravel,3),green,1.23);
   label(m.oracleChecked?"A PAIR COVERAGE CHECKED":"A ORACLE UNCHECKED",m.oracleChecked?green:muted,1.4);
   label("RESEARCH DEMO / NO CCD",red,1.35);
   gap();
   label("1 GEOMETRY / 2 REGION GUARDS",blue,1.4);
   label("3 AABB CANDIDATES / 4 CONTACTS",blue,1.35);
   label("G SCROLLABLE RESEARCH LOG",blue,1.35);
   label("SPACE PAUSE / N STEP / F FRAME",muted,1.3);
   label("K SWITCH TO ORIGINAL CLOTH",muted,1.3);
  } else {
  label(a.paused?"PAUSED / N TO STEP":"RUNNING / SPACE PAUSES",a.paused?muted:green);gap();label("REQUEST-AWARE BROAD PHASE",blue,1.7);label(s.guardedReuse?"Q: GUARDED ENVELOPE REUSE":"Q: REBUILD EVERY ITERATION",blue,1.65);label("M: QUERY MARGIN "+number(a.sim.queryRequest().margin,2),blue,1.7);label(s.sap?"B: SWEEP AND PRUNE":"B: EXHAUSTIVE REFERENCE",blue,1.7);label("BUILDS "+std::to_string(s.queryBuilds)+" / REUSES "+std::to_string(s.queryReuses),s.guardedReuse?green:muted,1.8);label(s.collisions?"C: COLLISION RESPONSE ON":"C: COLLISION RESPONSE OFF",s.collisions?green:red,1.6);label("FRAME "+std::to_string(s.frame));label("CANDIDATES "+std::to_string(s.candidateCount));label("WITHIN MARGIN "+std::to_string(s.proximityCount));label(s.oracleChecked?"V: ORACLE MISSES "+std::to_string(s.oracleMisses):"V: ORACLE NOT CHECKED",s.oracleMisses?red:(s.oracleChecked?green:muted),1.65);label(std::isfinite(s.minGap)?"MIN GAP "+number(s.minGap,4):"MIN GAP / STEP TO EVALUATE",ink,1.7);gap();label("CPU STEP "+number(s.stepMs)+" MS",muted,1.8);label("BROAD "+number(s.broadMs)+" MS",muted,1.8);label("PRECISE "+number(s.narrowMs)+" MS / "+std::to_string(s.preciseTests)+" TESTS",muted,1.45);label("ORACLE "+number(s.oracleMs)+" MS",muted,1.45);label("DISPLAY DIAG "+number(s.diagnosticMs)+" MS",muted,1.4);label(renderFps>0?"RENDER FPS "+number(renderFps,1):"RENDER FPS WARMUP",muted,1.7);gap();label("INSPECTION",blue,1.7);label((a.stage==1?"+ ":"  ")+std::string("1 GEOMETRY"));label((a.stage==2?"+ ":"  ")+std::string("2 CERTIFIED BOUNDS"));label((a.stage==3?"+ ":"  ")+std::string("3 CANDIDATES"));label((a.stage==4?"+ ":"  ")+std::string("4 PRECISE PROXIMITY"));label("F FRAME / R RESET / V ORACLE",muted,1.5);label(a.cutaway?"X VISUAL CUTAWAY ON":"X VISUAL CUTAWAY OFF",muted,1.6);label("W WIRE / DRAG ORBIT / ZOOM",muted,1.5);
 label("G LOG + CHARTS / H NEXT SCENE",blue,1.3);gap();label("FIXED DT 1/120 SECOND",muted,1.55);label("DEMO XPBD / NO SELF CONTACT",muted,1.45);if(a.stage==2)label("BOUNDS OVERLAY CAPPED AT 40",muted,1.35);
   label("K: OPEN DYNAMIC MULTICLOTH",blue,1.35);
  }

  text(hud,a.cutaway?"VISUAL CUTAWAY / PHYSICS USES FULL MESH":"LIVE STATE / NO FILE PLAYBACK",26*scale,height-27*scale,1.6*scale,ink);M ui=ortho(0,width,height,0,-1,1);glUseProgram(lines);glUniformMatrix4fv(glGetUniformLocation(lines,"vp"),1,GL_FALSE,ui.a);uploadDraw(vao,buffer,hud,GL_TRIANGLES);
  glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(fontProgram);glUniformMatrix4fv(glGetUniformLocation(fontProgram,"vp"),1,GL_FALSE,ui.a);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,fontTexture);glUniform1i(glGetUniformLocation(fontProgram,"atlas"),0);uploadDraw(vao,buffer,glyphVertices,GL_TRIANGLES);glDisable(GL_BLEND);
  if(a.evidenceMode)
   drawEvidencePane(a,viewport,panel,scale,width,height,vao,buffer,lines,fontProgram,fontTexture,ui);
  if(!capturePath.empty()&&rendered>=45&&renderFps>0){capture(width,height,capturePath);a.trace.flush();capturePath.clear();if(closeCapture)glfwSetWindowShouldClose(w,1);}glfwSwapBuffers(w);rendered++;
 }
 a.trace.flush();glfwDestroyWindow(w);glfwTerminate();return 0;
 }catch(const std::exception&e){std::cerr<<"NATIVE_ERROR "<<e.what()<<'\n';glfwTerminate();return 1;}}

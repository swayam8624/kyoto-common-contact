// Verified real mass-spring node/bond/triangle geometry for existing Phase-7 runs.
// Re-simulates ONLY a bounded representative subset of independent Worlds and
// compares EVERY selected element's Float32 AABB with the original immutable
// KYOTOM01 snapshots on EVERY frame. It does not rerun broad-phase benchmarks.
//
// Mesh coordinates are truthful solver nodes, never decorative shape proxies.
// Neighbor interactions between independent Worlds remain unsolved.
#include "ccp/material_scenes.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
using ccp::materials::World;
constexpr std::array<const char*,5> kinds={
 "cloth","sponge","cushion","soft_ball","spring"};
constexpr std::array<const char*,6> regimes={
 "baseline","compression","shear","impact","rapid","relax"};
struct Meta {
 int id{},kind{},first{},count{};
 std::string regime;
};
struct Selected {
 Meta info;
 std::unique_ptr<World> world;
};
int parse(const char* s,int lo,int hi){
 std::string str=s;std::size_t end=0;
 const auto value=std::stoll(str,&end);
 if(end!=str.size()||value<lo||value>hi)
  throw std::invalid_argument("Invalid bounded integer");
 return static_cast<int>(value);
}
double origin(int component,int side,int layout,int frame,int frames,int id,
              int totalInstances){
 const double t=double(frame)/double(std::max(1,frames-1));
 if(layout==3||layout==4){
  const int group=id/8,member=id%8;
  const int groups=(totalInstances+7)/8;
  const int width=static_cast<int>(std::ceil(std::cbrt(double(groups))));
  const int x=group%width,y=(group/width)%width,z=group/(width*width);
  const int bit=(member>>component)&1;
  const double local=layout==3?.20:
    (2.6-2.4*std::clamp((t-.20)/.62,0.,1.));
  const double index=component==0?x:component==1?y:z;
  return (index-width*.5)*6.7+(bit?local:-local)+
    .025*std::sin((id%19)*.45+.09*frame);
 }
 const int x=id%side,y=(id/side)%side,z=id/(side*side);
 const double spacing=layout==0?3.8:layout==1?2.3:3.3;
 const double front=layout==2?(.95*std::clamp((t-.30)/.6,0.,1.)):0.;
 if(component==0)
  return (x-side*.5)*(spacing-front)+.13*std::sin(.075*frame+id*.31);
 if(component==1)
  return (y-side*.5)*(spacing-.4*front)+.12*std::cos(.06*frame+id*.2);
 return z*(layout==0?3.3:2.6)+.05*std::sin(.1*frame+id*.11);
}
std::vector<Meta> readOwners(const fs::path& file){
 std::ifstream in(file);
 if(!in)throw std::runtime_error("Cannot open instance ownership");
 std::string first;
 if(!std::getline(in,first)||
    first!="id,kind,regime,first_primitive,primitive_count")
  throw std::runtime_error("Invalid material ownership schema");
 std::vector<Meta> result;
 std::string line;
 int next=0;
 while(std::getline(in,line)){
  if(line.empty())continue;
  std::stringstream tokens(line);std::array<std::string,5> value;
  for(auto& item:value)
   if(!std::getline(tokens,item,','))
    throw std::runtime_error("Incomplete material ownership");
  const int id=std::stoi(value[0]),lo=std::stoi(value[3]),
            count=std::stoi(value[4]);
  const auto kind=std::find_if(kinds.begin(),kinds.end(),
    [&](const char* name){return value[1]==name;});
  if(id!=static_cast<int>(result.size())||lo!=next||count<=0||
     kind==kinds.end())
   throw std::runtime_error("Noncontiguous material ownership");
  result.push_back({id,static_cast<int>(kind-kinds.begin()),lo,count,value[2]});
  next+=count;
 }
 if(result.size()<12||result.size()>1000)
  throw std::runtime_error("Mesh preview supports 12 to 1000 original Worlds");
 return result;
}
struct Binary {
 std::ifstream stream;
 std::uint32_t n{},frames{},seed{},material{};
 explicit Binary(const fs::path& path):stream(path,std::ios::binary){
  if(!stream)throw std::runtime_error("Cannot open original KYOTOM01");
  char signature[8]{};
  stream.read(signature,8);
  if(!stream||std::string(signature,8)!="KYOTOM01")
   throw std::runtime_error("Invalid KYOTOM01 signature");
  for(auto* value:{&n,&frames,&seed,&material})
   stream.read(reinterpret_cast<char*>(value),4);
  if(!stream||n==0||n>65536||frames<12||frames>240||material!=5)
   throw std::runtime_error("Unsupported material snapshot schema");
  const auto pos=stream.tellg();stream.seekg(0,std::ios::end);
  if(stream.tellg()-pos!=std::streamoff(frames)*n*24)
   throw std::runtime_error("Original binary length mismatch");
  stream.seekg(pos);
 }
 std::vector<std::array<float,6>> frame(){
  std::vector<std::array<float,6>> data(n);
  stream.read(reinterpret_cast<char*>(data.data()),
              static_cast<std::streamsize>(n)*24);
  if(!stream)throw std::runtime_error("Incomplete binary source frame");
  return data;
 }
};
void value(std::ostream& o,double x){
 if(!std::isfinite(x))throw std::runtime_error("Nonfinite simulated node");
 o<<static_cast<float>(x);
}
}
int main(int argc,char** argv){
 try{
  if(argc<4||argc>5)
   throw std::invalid_argument(
    "Usage: ccp_cbp_mesh_truth_export ORIGINAL_CASE_DIR OUT.json [max_worlds=48]");
  const fs::path folder(argv[1]),output(argv[2]);
  const int cap=argc==4?parse(argv[3],1,256):48;
  if(fs::exists(output)||fs::exists(output.string()+".part"))
   throw std::runtime_error("Preserving existing mesh evidence; choose fresh output");
  const auto metas=readOwners(folder/"instances.csv");
  Binary source(folder/"snapshots.kmcb");
  const int total=static_cast<int>(metas.size());
  const int primitives=metas.back().first+metas.back().count;
  if(primitives!=static_cast<int>(source.n))
   throw std::runtime_error("Original instance counts differ from binary");
  std::ifstream receipts(folder/"frames.csv");
  std::string head,first;
  if(!receipts||!std::getline(receipts,head)||
     !std::getline(receipts,first))
   throw std::runtime_error("Missing frame metadata");
  std::stringstream raw(first);std::vector<std::string> fields;std::string token;
  while(std::getline(raw,token,','))fields.push_back(token);
  if(fields.size()!=6||std::stoi(fields[1])!=total||
     std::stoi(fields[2])!=primitives||std::stoul(fields[4])!=source.seed)
   throw std::runtime_error("Original geometry receipt mismatch");
  const std::string layout=fields[3];
  const int layoutCode=layout=="grid"?0:layout=="clusters"?1:
    layout=="contact_front"?2:layout=="crowded"?3:
    layout=="collision_wave"?4:-1;
  if(layoutCode<0)throw std::runtime_error("Unsupported layout");
  const int side=static_cast<int>(std::ceil(std::cbrt(double(total))));
  // Select whole eight-world contact clusters, sampled across the scene,
  // instead of a single member from each independent cluster.
  const int groups=(total+7)/8;
  const int quota=std::min(total,cap);
  const int chosenGroups=std::max(1,quota/8);
  std::set<int> ids;
  if(total<=quota){
   for(int id=0;id<total;++id)ids.insert(id);
  }else{
   for(int groupIndex=0;groupIndex<chosenGroups;++groupIndex){
    const int group=chosenGroups==1?0:
      int(std::llround(double(groupIndex)*(groups-1)/(chosenGroups-1)));
    for(int member=0;member<8;++member)
     if(group*8+member<total)ids.insert(group*8+member);
   }
  }
  std::vector<Selected> selected;
  for(int id:ids){
   const auto& meta=metas[static_cast<std::size_t>(id)];
   auto w=std::make_unique<World>(kinds[static_cast<std::size_t>(meta.kind)],
        16,source.seed+static_cast<std::uint32_t>(id)*9719U,meta.regime);
   if(static_cast<int>(w->elements())!=meta.count)
    throw std::runtime_error("Mesh topology differs from original element count");
   selected.push_back({meta,std::move(w)});
  }
  if(!output.parent_path().empty())fs::create_directories(output.parent_path());
  std::ofstream out(output.string()+".part");
  if(!out)throw std::runtime_error("Cannot write visual truth data");
  out<<std::setprecision(9)<<std::fixed;
  out<<"{\"schema\":\"CCP_MESH_TRUTH_V1\",\"source\":\"KYOTOM01\","
      <<"\"instancesTotal\":"<<total<<",\"framesTotal\":"<<source.frames
      <<",\"verifiedElementsPerFrame\":";
  std::uint64_t elementsVerified=0;
  for(const auto& sample:selected)elementsVerified+=sample.info.count;
  out<<elementsVerified<<",\"selected\":[";
  for(std::size_t i=0;i<selected.size();++i){
   const auto& item=selected[i];const auto& w=*item.world;
   if(i)out<<',';
   out<<"{\"id\":"<<item.info.id<<",\"kind\":"<<item.info.kind
      <<",\"regime\":\""<<item.info.regime<<"\",\"nodeCount\":"
      <<w.nodes()<<",\"edges\":[";
   for(std::size_t j=0;j<w.visualBonds().size();++j){
    if(j)out<<',';
    const auto& b=w.visualBonds()[j];
    out<<'['<<b.a<<','<<b.b<<']';
   }
   out<<"],\"triangles\":[";
   bool firstTriangle=true;
   for(const auto& element:w.visualElements()){
    if(element.count!=3)continue;
    if(!firstTriangle)out<<',';
    firstTriangle=false;
    out<<'['<<element.vertices[0]<<','<<element.vertices[1]
       <<','<<element.vertices[2]<<']';
   }
   out<<"]}";
  }
  out<<"],\"positions\":[";
  for(std::uint32_t frame=0;frame<source.frames;++frame){
   const auto expected=source.frame();
   if(frame)out<<',';
   out<<'[';
   for(std::size_t i=0;i<selected.size();++i){
    auto& item=selected[i];item.world->step();
    const int id=item.info.id;
    const double ox=origin(0,side,layoutCode,int(frame),int(source.frames),id,total);
    const double oy=origin(1,side,layoutCode,int(frame),int(source.frames),id,total);
    const double oz=origin(2,side,layoutCode,int(frame),int(source.frames),id,total);
    const auto elements=item.world->snapshot(16);
    for(int k=0;k<item.info.count;++k){
     const auto& b=elements[static_cast<std::size_t>(k)].box;
     const std::array<double,6> v{b.min.x+ox,b.min.y+oy,b.min.z+oz,
                                  b.max.x+ox,b.max.y+oy,b.max.z+oz};
     const auto& reference=expected[static_cast<std::size_t>(item.info.first+k)];
     for(int c=0;c<6;++c)
      if(std::bit_cast<std::uint32_t>(static_cast<float>(v[static_cast<std::size_t>(c)]))!=
         std::bit_cast<std::uint32_t>(reference[static_cast<std::size_t>(c)]))
       throw std::runtime_error(
        "Replayed node geometry AABB does not bit-match original Float32 binary: "
        "instance="+std::to_string(id)+" frame="+std::to_string(frame));
    }
    if(i)out<<',';
    out<<'[';
    const auto& nodes=item.world->visualNodes();
    for(std::size_t n=0;n<nodes.size();++n){
     if(n)out<<',';
     value(out,nodes[n].x.x+ox);out<<',';
     value(out,nodes[n].x.y+oy);out<<',';
     value(out,nodes[n].x.z+oz);
    }
    out<<']';
   }
   out<<']';
   if((frame+1)%std::max(1u,source.frames/8u)==0)
    std::cerr<<"[mesh truth] frame "<<frame+1<<'/'<<source.frames
             <<" selected authentic Worlds "<<selected.size()<<"\n";
  }
  out<<"],\"validation\":\"all selected element Float32 AABBs bit-match immutable source\"}\n";
  out.flush();
  if(!out)throw std::runtime_error("Incomplete verified mesh export");
  out.close();fs::rename(output.string()+".part",output);
  std::cout<<"PASS verified mesh geometry: "<<selected.size()<<" real Worlds, "
           <<source.frames<<" frames, "<<elementsVerified
           <<" element Float32 bounds per frame bit-exact to original source\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP MESH VISUAL ERROR: "<<e.what()<<'\n';return 1;
 }
}

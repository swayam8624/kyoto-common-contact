// Appended to the original detector translation unit by prepare_gaia.py.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace {
using namespace GAIA;
using Clock=std::chrono::steady_clock;
static void vector_out(std::ostream& o,const Vec3& v){for(int k=0;k<3;++k)o<<','<<v(k);}
static void vf_out(std::ostream& o,int kind,int m,int i,ClothVFContactQueryResult& r){
    o<<"Q,"<<kind<<','<<m<<','<<i<<','<<r.found<<','<<r.minDisToPrimitives<<','<<r.contactPts.size()<<'\n';
    for(size_t j=0;j<r.contactPts.size();++j){auto& p=r.contactPts[j];o<<"VF,"<<kind<<','<<m<<','<<i<<','<<p.contactVertexSideMeshId<<','<<p.contactVertexId<<','<<p.contactFaceSideMeshId<<','<<p.contactFaceId<<','<<int(p.closestPtType)<<','<<int(p.primitiveType)<<','<<p.primitiveId<<','<<p.d;vector_out(o,p.contactPoint);vector_out(o,p.contactPointNormal);vector_out(o,p.barycentrics);o<<'\n';}}
static void ee_out(std::ostream& o,int m,int i,ClothEEContactQueryResult& r){
    o<<"Q,2,"<<m<<','<<i<<','<<r.found<<','<<r.minDisToPrimitives<<','<<r.contactPts.size()<<'\n';
    for(size_t j=0;j<r.contactPts.size();++j){auto& p=r.contactPts[j];o<<"EE,"<<m<<','<<i<<','<<p.contactMeshId1<<','<<p.contactEdgeId1<<','<<p.contactMeshId2<<','<<p.contactEdgeId2<<','<<p.d<<','<<p.mu1<<','<<p.mu2;vector_out(o,p.c1);vector_out(o,p.c2);o<<'\n';}}
static void query_all(ClothContactDetector& d,std::ostream* o){
    for(int m=0;m<d.numSimMeshes;++m){auto& mesh=*d.targetMeshes[m];
        for(int i=0;i<mesh.numVertices();++i){ClothVFContactQueryResult r;d.contactQueryVF(m,i,&r);if(o)vf_out(*o,0,m,i,r);}
        for(int i=0;i<mesh.numFaces();++i){ClothVFContactQueryResult r;d.contactQueryFV(m,i,&r);if(o)vf_out(*o,1,m,i,r);}
        for(int i=0;i<mesh.numEdges();++i){ClothEEContactQueryResult r;d.contactQueryEE(m,i,&r);if(o)ee_out(*o,m,i,r);}}
    if(o)for(unsigned m=0;m<d.targetMeshes.size();++m)for(int i=0;i<d.targetMeshes[m]->numFaces();++i){auto& links=d.getFaceContactInfo(m,i);*o<<"F,"<<m<<','<<i<<','<<d.getFaceMinDis(m,i)<<','<<links.size()<<'\n';for(size_t j=0;j<links.size();++j){auto& p=links[j];*o<<"L,"<<m<<','<<i<<','<<p.meshIdVSide<<','<<p.vertexId<<','<<p.contactId<<'\n';}}
}
}
int main(int argc,char** argv){
    try{if(argc!=5)throw std::runtime_error("usage: gaia_probe meshA.obj meshB.obj sim_count output_prefix");
        std::vector<GAIA::TriMeshFEM::SharedPtr> meshes;
        for(int i=1;i<=2;++i){auto p=std::make_shared<GAIA::TriMeshParams>();p->path=argv[i];auto m=std::make_shared<GAIA::TriMeshFEM>();m->initialize(p,true);if(!m->numFaces())throw std::runtime_error("empty mesh");meshes.push_back(m);}
        auto p=std::make_shared<GAIA::ClothContactDetectorParameters>();p->maxQueryDis=.12f;p->supportFVQuery=true;
        GAIA::ClothContactDetector d(p);d.initialize(meshes,std::stoi(argv[3]));kyoto_gaia::Selector sel;
        std::string prefix=argv[4];std::ofstream timing(prefix+".csv");timing<<"frame,baseline_query_ms,kyoto_capture_batch_replay_ms,strict_equal\n";bool all=true;
        std::vector<GAIA::TVerticesMat> original;for(auto& m:meshes)original.emplace_back(m->positions());
        for(int f=0;f<8;++f){for(size_t m=0;m<meshes.size();++m){meshes[m]->positions()=original[m];for(int i=0;i<meshes[m]->numVertices();++i)meshes[m]->vertex(i)(2)+=.005f*f*std::sin(float(i+m));meshes[m]->updated=true;}d.updateBVH();
            std::ostringstream a,b;a<<std::setprecision(17);b<<std::setprecision(17);
            d.resetFaceContactInfo();auto t=Clock::now();query_all(d,nullptr);auto t1=Clock::now();
            d.resetFaceContactInfo();kyoto_gaia::capture();auto t2=Clock::now();query_all(d,nullptr);kyoto_gaia::prepare(d,sel);d.resetFaceContactInfo();query_all(d,nullptr);kyoto_gaia::finish();auto t3=Clock::now();
            // Untimed full-payload audit. Do not run a second selector build here.
            d.resetFaceContactInfo();query_all(d,&a);d.resetFaceContactInfo();kyoto_gaia::phase=2;kyoto_gaia::cursor.fill(0);query_all(d,&b);kyoto_gaia::finish();
            std::ofstream(prefix+"_"+std::to_string(f)+"_baseline.txt")<<a.str();std::ofstream(prefix+"_"+std::to_string(f)+"_kyoto.txt")<<b.str();
            bool ok=a.str()==b.str();all&=ok;timing<<f<<','<<std::chrono::duration<double,std::milli>(t1-t).count()<<','<<std::chrono::duration<double,std::milli>(t3-t2).count()<<','<<ok<<'\n';std::cout<<"frame "<<f<<" strict parity "<<ok<<std::endl;
        }return all?0:2;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

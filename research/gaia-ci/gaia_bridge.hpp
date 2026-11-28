#pragma once
// Included inside the pinned Gaia detector .cpp, after its existing callbacks.
// This diagnostic adapter batches the original RTCPointQuery arguments. It never
// replaces the closest-feature tests, topology filters, or contact callbacks.
#include <dlfcn.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace kyoto_gaia {
using Box=std::array<double,6>;
using Id=std::pair<unsigned,unsigned>;
using Make=void*(*)();using Drop=void(*)(void*);
using Run=int(*)(void*,const double*,size_t,const double*,size_t,int,double,const unsigned**,size_t*,unsigned*,unsigned*);
struct Selector {
    void* lib=nullptr; void* ctx[3]={}; Drop drop=nullptr; Run run=nullptr;
    Selector(){const char* p=std::getenv("KYOTO_SELECTOR_LIBRARY");if(!p)throw std::runtime_error("missing KYOTO_SELECTOR_LIBRARY");
        lib=dlopen(p,RTLD_NOW|RTLD_LOCAL);if(!lib)throw std::runtime_error(dlerror());
        auto make=reinterpret_cast<Make>(dlsym(lib,"make"));drop=reinterpret_cast<Drop>(dlsym(lib,"drop"));run=reinterpret_cast<Run>(dlsym(lib,"run"));
        if(!make||!drop||!run)throw std::runtime_error("missing selector API");for(auto& c:ctx)if(!(c=make()))throw std::runtime_error("selector initialization failed");}
    ~Selector(){if(drop)for(auto c:ctx)if(c)drop(c);if(lib)dlclose(lib);}
    Selector(const Selector&)=delete;Selector& operator=(const Selector&)=delete;
    std::vector<std::vector<Id>> query(int kind,const std::vector<Box>& a,const std::vector<Box>& b,const std::vector<Id>& ids){
        const unsigned* out=nullptr;size_t n=0;unsigned path=0,hit=0;
        // C ABI uses a flat six-double array. Copy explicitly instead of relying on array layout.
        std::vector<double> af,bf;af.reserve(a.size()*6);bf.reserve(b.size()*6);
        for(auto& x:a)af.insert(af.end(),x.begin(),x.end());for(auto& x:b)bf.insert(bf.end(),x.begin(),x.end());
        if(run(ctx[kind],af.data(),a.size(),bf.data(),b.size(),6,.016,&out,&n,&path,&hit))throw std::runtime_error("selector failed");
        std::vector<std::vector<Id>> result(a.size());for(size_t k=0;k<n;++k){auto i=out[2*k],j=out[2*k+1];if(i>=a.size()||j>=ids.size())throw std::runtime_error("invalid candidate index");result[i].push_back(ids[j]);}return result;}
};
struct Request {int mesh,id;Box bounds;};
static Box query_box(const RTCPointQuery& q){
    Box b{};const float p[3]={q.x,q.y,q.z};
    if(!std::isfinite(q.radius)||q.radius<0)throw std::runtime_error("invalid Gaia query radius");
    // Conservative cube around the actual original float-radius query.
    for(int k=0;k<3;++k){b[k]=std::nextafter(double(p[k])-double(q.radius),-std::numeric_limits<double>::infinity());b[k+3]=std::nextafter(double(p[k])+double(q.radius),std::numeric_limits<double>::infinity());}return b;
}
static int phase=0; // 0 original, 1 capture requests, 2 replay original callbacks
static std::array<std::vector<Request>,3> requests;
static std::array<std::vector<std::vector<Id>>,3> candidates;
static std::array<size_t,3> cursor{};
static void capture(){for(auto& r:requests)r.clear();cursor.fill(0);phase=1;}
static void prepare(GAIA::ClothContactDetector& detector,Selector& selector){
    if(phase!=1)throw std::runtime_error("invalid capture phase");
    for(int kind=0;kind<3;++kind){std::vector<Box> a,b;std::vector<Id> ids;
        for(auto& q:requests[kind])a.push_back(q.bounds);
        for(unsigned mi=0;mi<detector.targetMeshes.size();++mi){auto& m=*detector.targetMeshes[mi];
            const int count=kind==0?m.numFaces():kind==1?m.numVertices():m.numEdges();
            for(int i=0;i<count;++i){int v[3]={i,i,i};int nv=1;
                if(kind==0){nv=3;for(int k=0;k<3;++k)v[k]=m.facePosVId(i,k);}else if(kind==2){nv=2;auto& e=m.pTopology->edgeInfos[i];v[0]=e.eV1;v[1]=e.eV2;}
                Box x{};for(int k=0;k<3;++k){x[k]=x[k+3]=double(m.vertex(v[0])(k));for(int j=1;j<nv;++j){double z=m.vertex(v[j])(k);x[k]=std::min(x[k],z);x[k+3]=std::max(x[k+3],z);}}
                b.push_back(x);ids.emplace_back(mi,unsigned(i));}}
        candidates[kind]=selector.query(kind,a,b,ids);
    }cursor.fill(0);phase=2;
}
static void finish(){if(phase!=2)throw std::runtime_error("invalid replay phase");for(int k=0;k<3;++k)if(cursor[k]!=requests[k].size())throw std::runtime_error("incomplete replay");phase=0;}
template<class Result>static bool dispatch(int kind,GAIA::ClothContactDetector* d,RTCPointQuery& query,RTCPointQueryContext& context,Result* result){
    if(!phase)return false;auto box=query_box(query);
    if(phase==1){requests[kind].push_back({int(result->queryMeshId),int(result->queryPrimitiveId),box});return true;}
    const size_t i=cursor[kind]++;if(i>=requests[kind].size())throw std::runtime_error("extra query");auto& r=requests[kind][i];
    if(r.mesh!=result->queryMeshId||r.id!=result->queryPrimitiveId||r.bounds!=box)throw std::runtime_error("query changed between capture and replay");
    for(auto p:candidates[kind][i]){RTCPointQueryFunctionArguments args{};args.query=&query;args.context=&context;args.userPtr=result;args.geomID=p.first;args.primID=p.second;args.similarityScale=1.f;
        auto* mesh=d->targetMeshes[p.first].get();bool changed=false;
        if(kind==0){if(p.first>=unsigned(d->numSimMeshes))changed=triMeshVFRadiusQueryWithTopologyFilteringAndFaceMinDisCaculatingFuncCollider(&args);
            else if(!mesh->sewingVertices.empty())changed=triMeshVFRadiusQueryWithTopologyFilteringAndFaceMinDisCaculatingFuncWithSewing(&args);
            else changed=triMeshVFRadiusQueryWithTopologyFilteringAndFaceMinDisCaculatingFunc(&args);}
        else if(kind==1)changed=triMeshFVRadiusQueryWithTopologyFilteringFunc(&args);
        else changed=mesh->sewingVertices.empty()?triMeshEdgeContactQueryFunc(&args):triMeshEdgeContactQueryFuncWithSewing(&args);
        if(changed||query_box(query)!=box)throw std::runtime_error("unsupported shrinking radius");}
    return true;
}
}

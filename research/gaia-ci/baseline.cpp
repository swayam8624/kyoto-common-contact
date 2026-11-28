#include "CollisionDetector/ClothContactDetector.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tbb/global_control.h>
int main(int argc,char** argv) {
    if(argc!=4) return 1;
    tbb::global_control limit(tbb::global_control::max_allowed_parallelism,1);
    std::vector<GAIA::TriMeshFEM::SharedPtr> meshes;
    for(int i=1;i<=2;++i){
        auto p=std::make_shared<GAIA::TriMeshParams>();p->path=argv[i];
        auto m=std::make_shared<GAIA::TriMeshFEM>();m->initialize(p,true);
        if(!m->numFaces()||!m->numEdges())throw std::runtime_error("empty mesh");
        meshes.push_back(m);
    }
    auto p=std::make_shared<GAIA::ClothContactDetectorParameters>();
    p->maxQueryDis=.12f;p->supportFVQuery=true;
    GAIA::ClothContactDetector d(p);d.initialize(meshes,std::stoi(argv[3]));
    std::vector<GAIA::TVerticesMat> original;for(auto& m:meshes)original.emplace_back(m->positions());
    size_t total=0;
    for(int f=0;f<8;++f){
        for(size_t m=0;m<meshes.size();++m){meshes[m]->positions()=original[m];for(int i=0;i<meshes[m]->numVertices();++i)meshes[m]->vertex(i)(2)+=.005f*f*std::sin(float(i+m));meshes[m]->updated=true;}
        d.updateBVH();d.resetFaceContactInfo();
        size_t vf=0,fv=0,ee=0;
        for(int m=0;m<d.numSimMeshes;++m){
            auto& mesh=*d.targetMeshes[m];
            for(int i=0;i<mesh.numVertices();++i){GAIA::ClothVFContactQueryResult r;d.contactQueryVF(m,i,&r);vf+=r.contactPts.size();}
            for(int i=0;i<mesh.numFaces();++i){GAIA::ClothVFContactQueryResult r;d.contactQueryFV(m,i,&r);fv+=r.contactPts.size();}
            for(int i=0;i<mesh.numEdges();++i){GAIA::ClothEEContactQueryResult r;d.contactQueryEE(m,i,&r);ee+=r.contactPts.size();}
        }
        total+=vf+fv+ee;std::cout<<"frame="<<f<<" VF="<<vf<<" FV="<<fv<<" EE="<<ee<<std::endl;
    }
    if(!total)throw std::runtime_error("no contacts: invalid smoke");
    std::cout<<"BASELINE PASSED "<<total<<" contacts\n";
}

// Reproducible geometry export from the SAME C++ cubic motion definitions
// as the broad-phase candidate comparison. KINEMATIC TEST, not dynamics.
// Two triangles coincide analytically at t=1/2 despite disjoint endpoints.
#include "kyoto/motion_envelopes.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace kyoto::contact;
static CubicVertex cv(Vec3 a,Vec3 b,Vec3 c,Vec3 d){return {{a,b,c,d}};}
static MovingTriangle scene_triangle(std::uint64_t id,double bend,double x){
    MovingTriangle t;t.id=id;t.thickness=1e-4;
    t.vertices={cv({x,0,0},{x+bend,0,0},{x+bend,0,0},{x,0,0}),
                cv({x,1,0},{x+bend,1,0},{x+bend,1,0},{x,1,0}),
                cv({x,0,1},{x+bend,0,1},{x+bend,0,1},{x,0,1})};
    return t;
}
int main(int argc,char**argv){
    try {
        if(argc!=2)throw std::invalid_argument("usage: kyoto_motion_export OUTPUT_DIR");
        const std::filesystem::path dir=argv[1];
        std::filesystem::create_directories(dir);
        const std::vector<MovingTriangle> objects={
            scene_triangle(11,4.0,0),scene_triangle(19,0.0,3.0)};
        const auto correct=as_primitives(objects);
        const auto wrong=as_primitives(objects,true);
        const auto oracle=exhaustive(correct);
        const auto sap=sweep_and_prune(correct);
        const auto endpoints=exhaustive(wrong);
        if(oracle!=std::vector<Pair>{{11,19}} || sap!=oracle ||
           !endpoints.empty()) throw std::runtime_error("oracle disagreement");
        std::ofstream data(dir/"trajectory.csv");data<<std::setprecision(17);
        data<<"frame,t,object,vertex,x,y,z\n";
        constexpr int samples=120;
        for(int frame=0;frame<=samples;frame++) {
            const double t=double(frame)/samples;
            for(const auto& obj:objects) for(int k=0;k<3;k++){
                const auto p=evaluate(obj.vertices[k],t);
                data<<frame<<','<<t<<','<<obj.id<<','<<k<<','
                    <<p.x<<','<<p.y<<','<<p.z<<'\n';
            }
        }
        if(!data.good()) throw std::runtime_error("could not export CSV");
        data.close();
        std::ofstream cps(dir/"control_points.csv");cps<<std::setprecision(17);
        cps<<"object,vertex,control,x,y,z\n";
        for(const auto& o:objects)for(int k=0;k<3;k++)
          for(int i=0;i<4;i++){
            const auto p=o.vertices[k].control[i];
            cps<<o.id<<','<<k<<','<<i<<','
               <<p.x<<','<<p.y<<','<<p.z<<'\n';
          }
        cps.close();
        std::ofstream boxes(dir/"envelopes.csv");boxes<<std::setprecision(17);
        boxes<<"object,mode,xmin,ymin,zmin,xmax,ymax,zmax\n";
        for(const auto& o:objects){
            const auto e=envelope(o);
            for(int mode=0;mode<2;mode++){
                const auto& b=mode?e.endpoint_only:e.complete;
                boxes<<o.id<<','
                     <<(mode?"endpoint_only":"complete_control_hull")<<','
                     <<b.min.x<<','<<b.min.y<<','<<b.min.z<<','
                     <<b.max.x<<','<<b.max.y<<','<<b.max.z<<'\n';
            }
        }
        boxes.close();
        std::ofstream audit(dir/"candidate_audit.csv");
        audit<<"contract,candidates,verified_midstep_coincidence,scope\n";
        audit<<"incorrect_endpoint_only,"<<endpoints.size()
             <<",yes,diagnostic_only\n";
        audit<<"complete_cubic_control_hull,"<<oracle.size()
             <<",yes,correct_broadphase_candidate\n";
        audit.close();
        const auto half=evaluate(objects[0].vertices[0],.5);
        const auto fixed=evaluate(objects[1].vertices[0],.5);
        if(std::abs(half.x-fixed.x)>1e-14)
            throw std::runtime_error("analytical contact invalid");
        std::ofstream manifest(dir/"manifest.json");
        manifest<<"{\n"
            <<"  \"dataset\": \"cubic_midstep_triangle_collision_v1\",\n"
            <<"  \"provenance\": \"C++23 cubic kinematic test, NOT a solver\",\n"
            <<"  \"frames\": 121,\n"
            <<"  \"candidate_oracle\": \"independent exhaustive swept-AABB\",\n"
            <<"  \"independent_sap_parity\": true,\n"
            <<"  \"analytical_contact_at_t\": 0.5,\n"
            <<"  \"general_continuous_collision_detector_used\": false,\n"
            <<"  \"limitations\": [\"not physics dynamics\", \"two triangles only\", "
              "\"sampled frames are not a proof of general CCD\"]\n"
            <<"}\n";
        manifest.close();
        std::cout<<"PASS: 121 kinematic frames; one complete swept candidate, "
                    "zero endpoint-only; analytic midstep coincidence\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
    }
}

// Source-backed 3D mesh visualization fixture, NOT a physics solver.
// A full moving toroidal shell passes through a triangulated breathing
// spherical cage. Every frame and candidate is evaluated in native C++23.
// The shared feature-level SAP produces VF/EE *instantaneous* AABB candidates;
// they are NOT CCD events or true physical contacts.

#include "kyoto/feature_broadphase.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace kyoto::contact;
using namespace kyoto::contact::features;

struct SceneData {
    Mesh mesh;
    std::map<std::uint64_t,std::uint64_t> objects;
    std::map<std::uint64_t,std::uint64_t> face_objects;
};
static constexpr double tau=2*std::numbers::pi;
static Vec3 torus(double theta,double phi,double center,double twist,double z) {
    const double R=1.83,r=.46;
    const double u=theta+twist*std::sin(phi);
    return {(R+r*std::cos(phi))*std::cos(u)+center,
            (R+r*std::cos(phi))*std::sin(u),
            r*std::sin(phi)+z};
}
static Vec3 sphere(double theta,double phi,double radius,double x) {
    return {radius*std::sin(phi)*std::cos(theta)+x,
            radius*std::sin(phi)*std::sin(theta),
            radius*std::cos(phi)};
}
static SceneData make_scene(){
    SceneData scene;
    scene.mesh.topology_epoch=1;
    scene.mesh.thickness=.035; // world-unit proximity radius, NOT calibrated material.
    const int torus_major=22,torus_minor=9;
    const int sphere_longitudes=20,sphere_latitudes=11;
    auto add_vertex=[&](std::uint64_t id,std::uint64_t object,
                        const std::array<Vec3,4>& controls){
        scene.mesh.vertices.push_back({id,CubicVertex{controls},object});
        scene.objects[id]=object;
    };
    for(int i=0;i<torus_major;i++)for(int j=0;j<torus_minor;j++){
        const double a=tau*double(i)/torus_major,b=tau*double(j)/torus_minor;
        add_vertex(std::uint64_t(1+i*torus_minor+j),1,{
            torus(a,b,-1.35,0,0),
            torus(a,b,-.45,.36,.17*std::sin(a)),
            torus(a,b,+.6,-.31,-.15*std::sin(a)),
            torus(a,b,+1.35,.1,0)
        });
    }
    auto tid=[&](int i,int j)->std::uint64_t{
        i=(i+torus_major)%torus_major;
        j=(j+torus_minor)%torus_minor;
        return std::uint64_t(1+i*torus_minor+j);
    };
    std::uint64_t next_face=9000;
    for(int i=0;i<torus_major;i++)for(int j=0;j<torus_minor;j++){
        const auto a=tid(i,j),b=tid(i+1,j),
                   c=tid(i+1,j+1),d=tid(i,j+1);
        scene.mesh.faces.push_back({next_face,{a,b,c}});
        scene.face_objects[next_face++]=1;
        scene.mesh.faces.push_back({next_face,{a,c,d}});
        scene.face_objects[next_face++]=1;
    }
    // A closed geodesic-like sphere with unique north/south pole
    // vertices and latitude rings. No degenerate repeated pole faces.
    const std::uint64_t pole_n=2000,pole_s=2001;
    add_vertex(pole_n,2,{sphere(0,0,1.53,0),sphere(0,0,1.57,0),
                         sphere(0,0,1.48,0),sphere(0,0,1.53,0)});
    add_vertex(pole_s,2,{sphere(0,std::numbers::pi,1.53,0),
                         sphere(0,std::numbers::pi,1.57,0),
                         sphere(0,std::numbers::pi,1.48,0),
                         sphere(0,std::numbers::pi,1.53,0)});
    auto sid=[&](int lat,int lon)->std::uint64_t{
        lon=(lon+sphere_longitudes)%sphere_longitudes;
        return std::uint64_t(2100+(lat-1)*sphere_longitudes+lon);
    };
    for(int lat=1;lat<=sphere_latitudes;lat++)
        for(int lon=0;lon<sphere_longitudes;lon++){
            const double a=tau*double(lon)/sphere_longitudes;
            const double phi=std::numbers::pi*double(lat)/
                             double(sphere_latitudes+1);
            add_vertex(sid(lat,lon),2,{
                sphere(a,phi,1.53,0),
                sphere(a,phi,1.57,.03*std::sin(a)),
                sphere(a,phi,1.48,-.05*std::sin(a)),
                sphere(a,phi,1.53,0)
            });
        }
    for(int lon=0;lon<sphere_longitudes;lon++){
        scene.mesh.faces.push_back(
            {next_face,{pole_n,sid(1,lon),sid(1,lon+1)}});
        scene.face_objects[next_face++]=2;
    }
    for(int lat=1;lat<sphere_latitudes;lat++)
        for(int lon=0;lon<sphere_longitudes;lon++){
            const auto a=sid(lat,lon),b=sid(lat,lon+1),
                       c=sid(lat+1,lon+1),d=sid(lat+1,lon);
            scene.mesh.faces.push_back({next_face,{a,b,c}});
            scene.face_objects[next_face++]=2;
            scene.mesh.faces.push_back({next_face,{a,c,d}});
            scene.face_objects[next_face++]=2;
        }
    for(int lon=0;lon<sphere_longitudes;lon++){
        scene.mesh.faces.push_back({
            next_face,{sid(sphere_latitudes,lon),
                       pole_s,sid(sphere_latitudes,lon+1)}});
        scene.face_objects[next_face++]=2;
    }
    std::set<std::pair<std::uint64_t,std::uint64_t>> edge_set;
    for(const auto& f:scene.mesh.faces){
        for(int j=0;j<3;j++){
            const auto a=f.vertices[j],b=f.vertices[(j+1)%3];
            edge_set.emplace(std::min(a,b),std::max(a,b));
        }
    }
    std::uint64_t next_edge=5000;
    for(const auto& [a,b]:edge_set)
        scene.mesh.edges.push_back({next_edge++,{a,b}});
    return scene;
}
static Mesh static_at(const Mesh& source,double t) {
    Mesh current=source;
    for(auto& v:current.vertices) {
        const auto p=evaluate(v.path,t);
        v.path.control={p,p,p,p};
    }
    return current;
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::invalid_argument(
            "usage: kyoto_feature_scene OUTPUT_DIR");
        const std::filesystem::path out=argv[1];
        std::filesystem::create_directories(out);
        const auto scene=make_scene();
        if(scene.mesh.vertices.size()<400 || scene.mesh.faces.size()<700)
            throw std::runtime_error("unexpected full-scene topology");
        // Check exact native typed SAP/oracle parity on the first and
        // central frames before exporting the complete sequence.
        for(double t:{0.,.5,1.}) {
            const auto m=static_at(scene.mesh,t);
            if(sweep_and_prune(m).pairs!=exhaustive(m).pairs)
                throw std::runtime_error("feature pair oracle mismatch");
        }
        std::ofstream positions(out/"mesh_states.csv");
        positions<<std::setprecision(17);
        positions<<"frame,time,vertex_id,object_id,x,y,z,displacement\n";
        std::ofstream faces(out/"faces.csv");
        faces<<"face_id,object_id,a,b,c\n";
        for(const auto& f:scene.mesh.faces)
            faces<<f.id<<','<<scene.face_objects.at(f.id)<<','
                 <<f.vertices[0]<<','<<f.vertices[1]<<','
                 <<f.vertices[2]<<'\n';
        std::ofstream edges(out/"edges.csv");
        edges<<"edge_id,a,b\n";
        for(const auto& e:scene.mesh.edges)
            edges<<e.id<<','<<e.vertices[0]<<','<<e.vertices[1]<<'\n';
        std::ofstream pair_csv(out/"typed_candidates.csv");
        pair_csv<<"frame,type,first,second\n";
        std::ofstream summary(out/"frame_metrics.csv");
        summary<<"frame,time,vertices,edges,faces,vertex_face_candidates,"
                  "edge_edge_candidates,total_candidates,aabb_tests\n";
        for(int frame=0;frame<=60;frame++) {
            const double t=double(frame)/60.;
            const auto m=static_at(scene.mesh,t);
            const auto result=sweep_and_prune(m);
            std::size_t vf=0,ee=0;
            for(const auto& p:result.pairs) {
                if(p.kind==Kind::VertexFace)vf++;
                else ee++;
                pair_csv<<frame<<','
                        <<(p.kind==Kind::VertexFace?"VF":"EE")<<','
                        <<p.first<<','<<p.second<<'\n';
            }
            for(std::size_t i=0;i<m.vertices.size();i++){
                const auto& original=scene.mesh.vertices[i];
                const auto p=m.vertices[i].path.control[0];
                const auto q=scene.mesh.vertices[i].path.control[0];
                const double dx=p.x-q.x,dy=p.y-q.y,dz=p.z-q.z;
                positions<<frame<<','<<t<<','<<original.id<<','
                         <<original.object<<','<<p.x<<','<<p.y<<','<<p.z
                         <<','<<std::sqrt(dx*dx+dy*dy+dz*dz)<<'\n';
            }
            summary<<frame<<','<<t<<','<<m.vertices.size()<<','
                   <<m.edges.size()<<','<<m.faces.size()<<','
                   <<vf<<','<<ee<<','<<result.pairs.size()<<','
                   <<result.candidate_aabb_tests<<'\n';
        }
        if(!positions.good()||!faces.good()||!edges.good()||
           !pair_csv.good()||!summary.good())
            throw std::runtime_error("scene source write failed");
        positions.close();faces.close();edges.close();
        pair_csv.close();summary.close();
        std::ofstream manifest(out/"scene_manifest.json");
        manifest<<"{\n"
            <<"  \"scene\": \"toroidal_shell_and_breathing_triangulated_sphere\",\n"
            <<"  \"source\": \"C++23 kinematic cubic trajectories, not a physical solver\",\n"
            <<"  \"frames\": 61,\n"
            <<"  \"vertex_count\": "<<scene.mesh.vertices.size()<<",\n"
            <<"  \"edge_count\": "<<scene.mesh.edges.size()<<",\n"
            <<"  \"face_count\": "<<scene.mesh.faces.size()<<",\n"
            <<"  \"topology_epoch\": 1,\n"
            <<"  \"proximity_radius\": "<<scene.mesh.thickness<<",\n"
            <<"  \"candidates\": \"instantaneous swept-AABB VF and EE only\",\n"
            <<"  \"independent_oracle\": \"exhaustive at frames 0,30,60\",\n"
            <<"  \"continuous_CCD\": false,\n"
            <<"  \"physics_contact_response\": false,\n"
            <<"  \"displacement_is_strain\": false\n"
            <<"}\n";
        manifest.close();
        std::cout<<"PASS: source-derived 3D shell/cage animation, 61 frames, "
                    "typed VF/EE candidates, 3 independent oracle frames\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
    }
}

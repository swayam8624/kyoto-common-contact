// Pass 20: controlled feature-level motion-sparsity COST CURVE.
// This is an adversarial synthetic broadphase control, NOT a physical
// FEM trajectory, NOT real collision response and NOT paper evidence of
// whole-solver performance. The real physical 3D source is measured by
// apps/pass15_official_ipc_fem_benchmark.cpp separately.
//
// All three methods pay the full query cost, and each of their complete
// VF/EE candidate sets must equal an independent exhaustive oracle.
// A/A full-refit instances bound same-algorithm timing instability.
#include "kyoto/refit_feature_hierarchy.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace f=kyoto::contact::features;
using kyoto::contact::Vec3;
using kyoto::contact::atlas::RefitHierarchy;
using Clock=std::chrono::steady_clock;

static f::Mesh source(std::size_t triangles){
    f::Mesh m;
    m.thickness=.0005;
    for(std::size_t t=0;t<triangles;++t){
        const auto id=std::uint64_t(3*t);
        const double x=.17*double(t%16);
        const double y=.17*double(t/16);
        const double z=.01*std::sin(.9*double(t));
        for(int k=0;k<3;++k) {
            const Vec3 p{x+(k==1?.14:0.),y+(k==2?.14:0.),z};
            f::Vertex v;v.id=id+std::uint64_t(k);
            v.path.control={p,p,p,p};
            m.vertices.push_back(v);
        }
        m.faces.push_back({std::uint64_t(t),{id,id+1,id+2}});
        for(int k=0;k<3;++k)
            m.edges.push_back({std::uint64_t(m.edges.size()),
                {id+std::uint64_t(k),
                 id+std::uint64_t((k+1)%3)}});
    }
    (void)f::build(m);
    return m;
}
static std::int64_t ns(Clock::time_point a,Clock::time_point b){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count();
}
int main(int argc,char**argv){
 try{
    if(argc!=2)throw std::invalid_argument(
        "usage: kyoto_pass20_synthetic_motion_sparsity OUTPUT_DIR");
    const auto dest=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(dest);
    std::ofstream csv(dest/"synthetic_feature_motion_sparsity.csv");
    if(!csv)throw std::runtime_error("cannot write feature stress receipt");
    csv<<std::setprecision(17);
    csv<<"source_kind,seed,moving_triangles,total_triangles,"
          "trial,frame,moving_vertex_paths,oracle_candidates,"
          "first_method,full_A_paid_ns,full_B_paid_ns,"
          "selective_paid_ns,selective_dirty_vertices,"
          "selective_unchanged_refits,selective_partial_refits\n";

    constexpr std::size_t N=128;
    constexpr std::array<double,5> FRACTIONS{0.0,.025,.10,.50,1.0};
    constexpr int trials=3;
    constexpr int frames=28;
    std::size_t checked=0;
    for(const auto share:FRACTIONS) {
        for(int trial=0;trial<trials;++trial){
            constexpr unsigned SEED=20261123;
            std::mt19937_64 random(SEED+103*trial);
            std::vector<std::size_t> shuffled(N);
            std::iota(shuffled.begin(),shuffled.end(),std::size_t{0});
            std::shuffle(shuffled.begin(),shuffled.end(),random);
            const std::size_t moved=static_cast<std::size_t>(
                std::round(share*double(N)));
            std::vector<unsigned char> active(N,0);
            for(std::size_t i=0;i<moved;++i)active[shuffled[i]]=1;
            const auto base=source(N);
            RefitHierarchy full_A,full_B;
            RefitHierarchy selective(true,false,false,4,true);
            for(int frame=0;frame<frames;++frame){
                auto mesh=base;
                for(std::size_t t=0;t<N;++t) {
                    if(!active[t])continue;
                    for(std::size_t k=0;k<3;++k) {
                        const std::size_t i=3*t+k;
                        auto& control=mesh.vertices[i].path.control;
                        const double time=double(frame+1);
                        const double j=.01*std::sin(time*.41+double(t)*.21);
                        const double h=.012*std::cos(time*.17+double(t)*.15);
                        const Vec3 p{base.vertices[i].path.control[0].x+j,
                                     base.vertices[i].path.control[0].y+h,
                                     base.vertices[i].path.control[0].z+j*.25};
                        const Vec3 q{p.x+.006,p.y-.004,p.z+.003};
                        control={p,{p.x,p.y,p.z+.015},
                                 {q.x,q.y,q.z-.015},q};
                    }
                }
                const auto truth=f::exhaustive(mesh).pairs;
                f::Result a,b,s;
                std::int64_t an=0,bn=0,sn=0;
                auto run_A=[&](){
                    const auto t=Clock::now();
                    a=full_A.query(mesh);an=ns(t,Clock::now());
                };
                auto run_B=[&](){
                    const auto t=Clock::now();
                    b=full_B.query(mesh);bn=ns(t,Clock::now());
                };
                auto run_S=[&](){
                    const auto t=Clock::now();
                    s=selective.query(mesh);sn=ns(t,Clock::now());
                };
                const int rotation=(frame+trial)%3;
                if(rotation==0){run_A();run_S();run_B();}
                else if(rotation==1){run_B();run_A();run_S();}
                else{run_S();run_B();run_A();}
                if(a.pairs!=truth||b.pairs!=truth||s.pairs!=truth)
                    throw std::runtime_error(
                        "source-feature candidate oracle parity failed at motion fraction "+
                        std::to_string(share)+" frame "+std::to_string(frame));
                if(an<=0||bn<=0||sn<=0)
                    throw std::runtime_error("nonpositive native paid clock");
                const int first=rotation==0?0:(rotation==1?1:2);
                csv<<"synthetic_not_physical_FEM,"<<SEED+103*trial<<','
                   <<moved<<','<<N<<','<<trial<<','<<frame<<','
                   <<3*moved<<','<<truth.size()<<','
                   <<(first==0?"full_A":first==1?"full_B":"selective")
                   <<','<<an<<','<<bn<<','<<sn<<','
                   <<selective.dirty_vertex_count()<<','
                   <<selective.unchanged_refits()<<','
                   <<selective.partial_refits()<<'\n';
                ++checked;
            }
        }
    }
    if(!csv.good())throw std::runtime_error("incomplete benchmark write");
    std::cout<<"PASS20 synthetic 3D source feature stress frames="<<checked
             <<" across 0%, 2.5%, 10%, 50%, and 100% active faces, "
             <<"3 seeded feature scenes each; exact oracle parity; "
             <<"no original solver or physics speedup claimed.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS20 SYNTHETIC STRESS FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

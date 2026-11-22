// Stage 2 paid CPU study. Synthetic moving triangles, not solver dynamics.
// The analytic continuous-contact labels apply ONLY to the congruent
// triangle family moving along X (not a general CCD implementation).
#include "kyoto/temporal_refinement.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
using namespace kyoto::contact;
using namespace kyoto::contact::temporal;

static MovingTriangle triangle(std::uint64_t id,double amplitude,double x){
    MovingTriangle t;t.id=id;
    t.category=id==1?std::uint64_t{1}:std::uint64_t{2};
    t.mask=id==1?std::uint64_t{2}:std::uint64_t{1};
    for(int k=0;k<3;k++){
        const double y=k==1?1:0,z=k==2?1:0;
        t.vertices[k].control={{{x,y,z},{x+amplitude,y,z},
                                {x+amplitude,y,z},{x,y,z}}};
    }
    return t;
}
static std::vector<MovingTriangle> scene(int targets,double a){
    std::vector<MovingTriangle> objects;
    objects.push_back(triangle(1,a,0));
    for(int k=0;k<targets;k++){
        const double x=1.2*a*(k+.5)/targets;
        objects.push_back(triangle(static_cast<std::uint64_t>(k+2),0,x));
    }
    return objects;
}
static std::size_t true_contact_count(int targets) {
    // x(t)=3a*t*(1-t), so max x=3a/4. All same-YZ target
    // triangles inside [0,max] intersect at a common time.
    std::size_t n=0;
    for(int k=0;k<targets;k++)
        if(1.2*(k+.5)/targets<=.75)n++;
    return n;
}
int main(int argc,char** argv){
    try{
        if(argc!=2)throw std::invalid_argument(
            "usage: kyoto_temporal_experiment OUTPUT_DIR");
        const std::filesystem::path out=argv[1];
        std::filesystem::create_directories(out);
        std::ofstream csv(out/"temporal_analytical_study.csv");
        csv<<"method,count,amplitude,depth,slabs,true_events,"
              "candidate_unique,false_positive,missed_true,occurrences,"
              "boxes_built,mean_paid_us,p95_paid_us,repeat_count,scope\n";
        csv<<std::setprecision(11);
        for(int N:{32,128,512,2048})for(double A:{2.,4.,8.}){
            const auto objects=scene(N,A);
            const std::size_t truth=true_contact_count(N);
            for(unsigned depth=0;depth<=4;depth++){
                const auto exact=query(objects,depth,true);
                const auto full=query(objects,depth);
                const auto selected=selective_query(objects,depth);
                if(full.unique_pairs!=exact.unique_pairs ||
                   full.unique_pairs!=selected.unique_pairs)
                    throw std::runtime_error("candidate parity failure");
                if(full.unique_pairs.size()<truth)
                    throw std::runtime_error("missing analytic contact count");
                for(int k=0;k<N;k++){
                    const double X=1.2*A*(k+.5)/N;
                    if(X<=.75*A){
                        const Pair pair{1,static_cast<std::uint64_t>(k+2)};
                        if(!std::binary_search(full.unique_pairs.begin(),
                                               full.unique_pairs.end(),pair))
                            throw std::runtime_error("analytic contact missing");
                    }
                }
                const auto extras=full.unique_pairs.size()-truth;
                for(int which=0;which<2;which++){
                    const bool selective=which==1;
                    std::vector<double> elapsed;
                    // Three warmups and eleven repeated query timings of
                    // the SAME input, not eleven independent scenes.
                    for(int repeat=0;repeat<14;repeat++){
                        const auto start=std::chrono::steady_clock::now();
                        if(selective){
                            const auto r=selective_query(objects,depth);
                            if(r.unique_pairs!=full.unique_pairs)
                                throw std::runtime_error("nondeterministic output");
                        }else{
                            const auto r=query(objects,depth);
                            if(r.unique_pairs!=full.unique_pairs)
                                throw std::runtime_error("nondeterministic output");
                        }
                        const auto end=std::chrono::steady_clock::now();
                        if(repeat>=3)
                            elapsed.push_back(std::chrono::duration<double,std::micro>(
                                end-start).count());
                    }
                    std::sort(elapsed.begin(),elapsed.end());
                    const double mean=std::accumulate(elapsed.begin(),elapsed.end(),0.)/
                                      elapsed.size();
                    // Nearest rank 95th of eleven repeated timings.
                    csv<<(selective?"selective_pair":"global_slab")<<','
                       <<N<<','<<A<<','<<depth<<','<<full.slabs<<','
                       <<truth<<','<<full.unique_pairs.size()<<','<<extras
                       <<",0,"<<full.slab_occurrences<<','<<full.boxes_built<<','
                       <<mean<<','<<elapsed.back()<<",11,synthetic_analytic_geometry\n";
                    std::cout<<"method="<<(selective?"selective_pair":"global_slab")
                             <<" N="<<N<<" A="<<A<<" depth="<<depth
                             <<" analytic_events="<<truth
                             <<" candidates="<<full.unique_pairs.size()
                             <<" false_positives="<<extras
                             <<" paid_us="<<mean<<'\n';
                }
            }
        }
        if(!csv.good())throw std::runtime_error("cannot write experiment CSV");
        {
            const int n=32;const double amplitude=4.;
            const auto objects=scene(n,amplitude);
            std::ofstream graph(out/"candidate_graph.csv");
            graph<<"depth,object_id,target_x,analytical_contact,"
                    "coarse_candidate,global_candidate,selective_candidate\n";
            graph<<std::setprecision(17);
            const auto coarse=query(objects,0).unique_pairs;
            for(unsigned depth=0;depth<=4;depth++){
                const auto full=query(objects,depth).unique_pairs;
                const auto selected=selective_query(objects,depth).unique_pairs;
                if(full!=selected)throw std::runtime_error("graph disagreement");
                for(int k=0;k<n;k++){
                    const std::uint64_t id=static_cast<std::uint64_t>(k+2);
                    const Pair p{1,id};
                    const double x=1.2*amplitude*(k+.5)/n;
                    const auto has=[&](const std::vector<Pair>& pairs) {
                        return std::binary_search(pairs.begin(),pairs.end(),p);
                    };
                    graph<<depth<<','<<id<<','<<x<<','<<int(x<=3.)<<','
                         <<int(has(coarse))<<','<<int(has(full))<<','
                         <<int(has(selected))<<'\n';
                }
            }
        }
        {
            // Real interval boxes from the C++ de Casteljau implementation.
            std::vector<IntervalTriangle> slabs={initial(triangle(1,4.,0))};
            std::ofstream boxes(out/"slab_envelopes.csv");
            boxes<<"depth,slab,t0,t1,xmin,xmax,ymin,ymax,zmin,zmax\n";
            boxes<<std::setprecision(17);
            for(unsigned depth=0;depth<=4;depth++){
                const double dt=1./double(slabs.size());
                for(std::size_t k=0;k<slabs.size();k++){
                    const auto b=conservative_box(slabs[k]);
                    boxes<<depth<<','<<k<<','<<k*dt<<','<<(k+1)*dt<<','
                         <<b.min.x<<','<<b.max.x<<','<<b.min.y<<','
                         <<b.max.y<<','<<b.min.z<<','<<b.max.z<<'\n';
                }
                if(depth<4){
                    std::vector<IntervalTriangle> next;
                    for(const auto& s:slabs){
                        const auto [l,r]=bisect(s);
                        next.push_back(l);next.push_back(r);
                    }
                    slabs=std::move(next);
                }
            }
        }
        std::ofstream manifest(out/"manifest.json");
        manifest<<"{\n"
            <<"  \"phase\": \"2-temporal-certificate\",\n"
            <<"  \"family\": \"analytic_cubic_triangle_targets\",\n"
            <<"  \"semantic_model\": \"congruent_triangles_moving_only_in_X\",\n"
            <<"  \"numeric_model\": \"IEEE754_binary64_FE_TONEAREST_non_FTZ\",\n"
            <<"  \"independent_exact_candidate_oracle\": true,\n"
            <<"  \"independent_general_CCD_oracle\": false,\n"
            <<"  \"whole_solver_comparison\": false,\n"
            <<"  \"published_LBVH_comparison\": false,\n"
            <<"  \"timing_includes\": \"envelopes_candidates_normalization\",\n"
            <<"  \"repetitions_per_condition\": 11,\n"
            <<"  \"warning\": \"no novel performance or physics claim\"\n"
            <<"}\n";
        if(!manifest.good())throw std::runtime_error("manifest write failed");
        std::cout<<"PASS: 120 method/scene benchmark rows and analytic pair graph\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL "<<e.what()<<'\n';return 1;
    }
}

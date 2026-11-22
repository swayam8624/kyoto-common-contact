#include "kyoto/motion_envelopes.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
using namespace kyoto::contact;
static void require(bool ok,const char* msg) {
    if(!ok) throw std::runtime_error(msg);
}
static CubicVertex curve(Vec3 a,Vec3 b,Vec3 c,Vec3 d) {
    return {{a,b,c,d}};
}
static MovingTriangle specimen(std::uint64_t id,double bend,double x=0.0) {
    MovingTriangle p; p.id=id; p.thickness=1e-4;
    p.vertices={
        curve({x,0,0},{x+bend,0,0},{x+bend,0,0},{x,0,0}),
        curve({x,1,0},{x+bend,1,0},{x+bend,1,0},{x,1,0}),
        curve({x,0,1},{x+bend,0,1},{x+bend,0,1},{x,0,1})
    };
    return p;
}

int main() {
    try {
        const auto moving=specimen(11,4.0);
        const auto target=specimen(19,0.0,3.0);
        const auto full=as_primitives({moving,target});
        const auto endpoints=as_primitives({moving,target},true);
        require(exhaustive(full)==std::vector<Pair>{{11,19}},
                "cubic candidate missed by complete path");
        require(sweep_and_prune(full)==exhaustive(full),
                "SAP and exhaustive disagree");
        require(exhaustive(endpoints).empty(),
                "endpoint-only unsafe counterexample not reproduced");
        for(std::size_t v=0;v<3;v++) {
            const auto mid=evaluate(moving.vertices[v],0.5);
            const auto fixed=evaluate(target.vertices[v],0.5);
            require(std::abs(mid.x-3.0)<1e-14,
                    "analytical mid-step x not 3");
            require(std::abs(mid.x-fixed.x)<1e-14,
                    "stationary and moving triangle not coincident");
        }
        const auto bounds=envelope(moving);
        require(bounds.complete.max.x >= 4.0,
                "control hull not enclosed");
        require(bounds.endpoint_only.max.x < 1.0,
                "endpoint-only box failed counterexample");
        std::mt19937_64 rng(20261123);
        std::uniform_real_distribution<double> pos(-100.0,100.0);
        std::uniform_real_distribution<double> thick(0.0,1.0);
        for(int scene=0;scene<350;scene++) {
            MovingTriangle t;
            t.id=static_cast<std::uint64_t>(scene+1);
            t.thickness=thick(rng);
            for(auto& vertex:t.vertices)
                for(auto& p:vertex.control)
                    p={pos(rng),pos(rng),pos(rng)};
            const auto e=envelope(t);
            for(int k=0;k<=101;k++) {
                const double time=double(k)/101.0;
                for(const auto& vertex:t.vertices) {
                    const auto p=evaluate(vertex,time);
                    require(p.x>=e.complete.min.x && p.x<=e.complete.max.x &&
                            p.y>=e.complete.min.y && p.y<=e.complete.max.y &&
                            p.z>=e.complete.min.z && p.z<=e.complete.max.z,
                            "sample escaped control-point hull");
                }
            }
        }
        bool caught=false;
        try {
            auto bad=moving;bad.thickness=-0.1;(void)envelope(bad);
        }catch(const std::invalid_argument&){caught=true;}
        require(caught,"negative thickness accepted");
        caught=false;
        try {
            auto bad=moving;
            bad.vertices[0].control[1].x=std::numeric_limits<double>::quiet_NaN();
            (void)envelope(bad);
        }catch(const std::invalid_argument&){caught=true;}
        require(caught,"nonfinite control accepted");
        caught=false;
        try {(void)evaluate(moving.vertices[0],1.1);}
        catch(const std::invalid_argument&){caught=true;}
        require(caught,"time outside interval accepted");
        std::cout<<"PASS: 350 random cubic triangle sets x 102 times x 3 vertices; "
                    "analytical midpoint collision; exact candidate parity\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}

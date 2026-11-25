// Pass 31A: analytic relative-hull cases and independent dense-time
// regression. Tests do not convert unknown into proven collision.
#include "kyoto/rounded_relative_triangle_certificate.hpp"
#include "kyoto/moving_triangle_pair_certificate.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Triangle3 tri(V3 p){return {p,p+V3{.2,0,0},p+V3{0,.2,0}};}
static LinearTriangleMotion motion(V3 p,V3 q){return {tri(p),tri(q)};}
int main(){
    try{
        constexpr double skin=.0005;
        const auto a=motion({0,0,0},{0,0,10});
        const auto b=motion({0,0,.02},{0,0,10.02});
        // Both world-space swept hulls overlap; relative hull is separated.
        need(maybe_near(swept_bounding(a),swept_bounding(b),skin),
             "co-motion fixture lacks world-space overlap");
        const auto co=certify_rounded_relative_triangle_pair(a,b,skin,0);
        need(co.entirely_separated&&co.nodes==1&&co.lower_clearance>.019,
             "synchronized co-motion failed root certificate");
        const auto cross=motion({0,0,.02},{0,0,-.02});
        const auto fixed=motion({0,0,0},{0,0,0});
        need(!certify_rounded_relative_triangle_pair(cross,fixed,skin).entirely_separated,
             "midstep crossing incorrectly certified");
        need(!certify_rounded_relative_triangle_pair(fixed,cross,skin).entirely_separated,
             "swapped crossing incorrectly certified");
        need(!certify_rounded_relative_triangle_pair(fixed,fixed,0.).entirely_separated,
             "coincident triangles incorrectly certified");
        const auto exact_skin=motion({0,0,skin},{0,0,skin});
        need(!certify_rounded_relative_triangle_pair(exact_skin,fixed,skin).entirely_separated,
             "strict skin boundary incorrectly certified");
        // Root hull contains zero: one endpoint vertex is (1,-1,0)
        // and another at the other endpoint is (-1,1,0). At every
        // simultaneous time the triangle nevertheless stays away from
        // the small static triangle around the origin. Dyadic slabs
        // recover a witness without accepting the entire root hull.
        const LinearTriangleMotion turning{
            {V3{1,-2,0},V3{1,-1,0},V3{1,-1.5,.1}},
            {V3{-1,1,0},V3{-2,1,0},V3{-1.5,1,.1}}};
        const Triangle3 small{V3{0,0,0},V3{.001,0,0},V3{0,.001,0}};
        const LinearTriangleMotion still{small,small};
        need(!certify_rounded_relative_triangle_pair(turning,still,skin,0).entirely_separated,
             "root hull fixture did not exercise unresolved support");
        const auto refined=certify_rounded_relative_triangle_pair(turning,still,skin,12);
        need(refined.entirely_separated&&refined.max_depth>0,
             "safe changing witness failed dyadic refinement");
        std::mt19937_64 rng(31012026);
        std::uniform_real_distribution<double> d(-.7,.7);
        auto v=[&](){return V3{d(rng),d(rng),d(rng)};};
        std::size_t certified=0,unknown=0,observed=0;
        double smallest=1e10;
        for(int k=0;k<300;++k){
            LinearTriangleMotion x,y;
            for(int j=0;j<3;++j){
                x.start[j]=v();x.end[j]=x.start[j]+v()*.4;
                y.start[j]=v();y.end[j]=y.start[j]+v()*.4;
                if(k%3==0){y.start[j].z+=2.;y.end[j].z+=2.;}
            }
            const auto cert=certify_rounded_relative_triangle_pair(x,y,skin,10);
            double distance=1e10;
            for(int s=0;s<=128;++s){
                const double t=double(s)/128.;
                distance=std::min(distance,triangle_triangle_closest(
                    lerp_vertices(x.start,x.end,t),lerp_vertices(y.start,y.end,t)).distance);
            }
            if(distance<=skin){
                ++observed;need(!cert.entirely_separated,"sampled skin violation certified");
            }
            if(cert.entirely_separated){
                ++certified;smallest=std::min(smallest,cert.lower_clearance);
                need(cert.lower_clearance>skin&&cert.lower_clearance<=distance+1e-10,
                     "outward certificate exceeds sampled closest distance");
            }else ++unknown;
        }
        need(certified>0&&unknown>0&&observed>0,"randomized coverage insufficient");
        std::cout<<"PASS rounded relative hull: certified="<<certified<<" unknown="<<unknown
                 <<" sampled_skin_violations="<<observed<<" smallest_lower_clearance="<<smallest
                 <<" co_motion_lower="<<co.lower_clearance<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

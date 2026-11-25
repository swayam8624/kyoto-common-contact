// Pass 30A: analytic endpoint-safe/in-step-inversion counterexamples,
// independently sampled long-double determinant bounds, and randomized
// affine paths. Sampling is a regression, not the certificate proof.
#include "kyoto/tet_orientation_certificate.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace kyoto::solid;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static TetPositions unit(){return {V3{},V3{1,0,0},V3{0,1,0},V3{0,0,1}};}
static TetPositions transform(const TetPositions& a,const M3& f,V3 shift={}){
    auto b=a;for(auto& p:b)p=f*p+shift;return b;
}
static long double sampled_j(const TetPositions& a,const TetPositions& b,
                             const M3& inv,long double t){
    long double f[3][3]{};
    auto component=[](V3 p,int k)->long double{return k==0?p.x:k==1?p.y:p.z;};
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)
        for(int k=0;k<3;++k){
            const auto e0=component(a[k+1],i)-component(a[0],i);
            const auto e1=component(b[k+1],i)-component(b[0],i);
            f[i][j]+=((1-t)*e0+t*e1)*static_cast<long double>(inv(k,j));
        }
    return f[0][0]*(f[1][1]*f[2][2]-f[1][2]*f[2][1])
          -f[0][1]*(f[1][0]*f[2][2]-f[1][2]*f[2][0])
          +f[0][2]*(f[1][0]*f[2][1]-f[1][1]*f[2][0]);
}
int main(){
    try{
        const auto a=unit();const auto inv=identity();
        auto flip=identity();flip(0,0)=-2;flip(1,1)=-.5;
        const auto b=transform(a,flip);
        need(sampled_j(a,b,inv,0)>0&&sampled_j(a,b,inv,1)>0&&
             sampled_j(a,b,inv,.5L)<0,"invalid analytic inversion fixture");
        need(!certify_linear_tet_orientation(a,b,inv).certified,
             "positive endpoints hid a negative interior determinant");
        auto touch=identity();touch(0,0)=-1;touch(1,1)=-1;
        need(!certify_linear_tet_orientation(a,transform(a,touch),inv,0.).certified,
             "interior singularity falsely certified");
        auto turn=identity();turn(0,0)=-.99;turn(1,1)=-.99;
        turn(0,1)=-.1;turn(1,0)=.1;
        const auto rot=transform(a,turn);
        need(!certify_linear_tet_orientation(a,rot,inv,1e-6,0).certified,
             "depth-zero fixture did not exercise unresolved coefficient");
        const auto safe=certify_linear_tet_orientation(a,rot,inv);
        need(safe.certified&&safe.max_depth>0&&safe.lower_j>1e-6,
             "safe curved determinant required but failed dyadic refinement");
        need(certify_linear_tet_orientation(a,transform(a,inv,{2,-3,4}),inv).certified,
             "rigid translation failed orientation");
        std::mt19937_64 rng(30112026);
        std::uniform_real_distribution<double> u(-1.5,1.5);
        std::size_t certified=0,unknown=0,sampled_negative=0;
        for(int i=0;i<800;++i){
            M3 m=identity(),n=identity();
            for(int j=0;j<3;++j)for(int k=0;k<3;++k){
                m(j,k)+=u(rng)*(i%2?.1:1.);
                n(j,k)+=u(rng)*(i%2?.1:1.);
            }
            const auto p=transform(a,m),q=transform(a,n);
            const auto c=certify_linear_tet_orientation(p,q,inv);
            long double min_j=1e100L;
            for(int k=0;k<=256;++k)
                min_j=std::min(min_j,sampled_j(p,q,inv,static_cast<long double>(k)/256));
            if(min_j<=1e-6L){
                ++sampled_negative;need(!c.certified,"sampled inversion falsely certified");
            }
            if(c.certified){
                ++certified;
                need(c.lower_j>1e-6&&static_cast<long double>(c.lower_j)<=min_j+1e-13L,
                     "reported continuous lower bound exceeds sampled determinant");
            }else ++unknown;
        }
        need(certified>0&&unknown>0&&sampled_negative>0,"random gate lacked coverage");
        std::cout<<"PASS continuous tetrahedral orientation: certified="<<certified
                 <<" unknown="<<unknown<<" sampled_nonpositive="<<sampled_negative
                 <<" analytic double inversion and singularity rejected\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

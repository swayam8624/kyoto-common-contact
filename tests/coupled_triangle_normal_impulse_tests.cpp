// Independent total-momentum and energy audit of the local two-body law.
#include "kyoto/coupled_triangle_normal_impulse.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace kyoto::solid;
static void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
struct Ledger { V3 p{},l{};double energy{}; };
static Ledger measure(const ContactTriangleState& a,const ContactTriangleState& b){
    Ledger r;
    for(const auto* t:{&a,&b})for(int i=0;i<3;++i){
        const auto p=t->v[i]*t->mass[i];
        r.p+=p;r.l+=cross(t->x[i],p);
        r.energy+=.5*t->mass[i]*dot(t->v[i],t->v[i]);
    }
    return r;
}
int main(){
    try{
        std::mt19937_64 rng(29012026);
        std::uniform_real_distribution<double> d(-2.,2.),m(.1,5.),q(.1,1.);
        auto vec=[&](){return V3{d(rng),d(rng),d(rng)};};
        double max_p=0,max_l=0,max_e=0;int closing=0,separating=0;
        for(int n=0;n<1600;++n){
            ContactTriangleState a,b;
            std::array<double,3> wa{},wb{};
            for(int j=0;j<3;++j){
                a.x[j]=vec();b.x[j]=vec();
                a.v[j]=vec();b.v[j]=vec();
                a.mass[j]=m(rng);b.mass[j]=m(rng);
                wa[j]=q(rng);wb[j]=q(rng);
            }
            const double sa=wa[0]+wa[1]+wa[2],sb=wb[0]+wb[1]+wb[2];
            for(int j=0;j<3;++j){wa[j]/=sa;wb[j]/=sb;}
            V3 pa{},pb{};
            for(int j=0;j<3;++j){pa+=a.x[j]*wa[j];pb+=b.x[j]*wb[j];}
            const auto normal=(pa-pb)/norm(pa-pb);
            // Construct both closing and separating cases, not just
            // rely on random sign selection.
            const double shift=n%4==0?20.:-20.;
            for(auto& v:a.v)v+=normal*shift;
            const double e=double(n%11)/10.;
            const auto before=measure(a,b);
            const auto receipt=apply_two_triangle_normal_impulse(a,b,wa,wb,e);
            const auto after=measure(a,b);
            const double dp=norm(after.p-before.p),dl=norm(after.l-before.l);
            const double de=std::abs(after.energy-before.energy-receipt.predicted_kinetic_change);
            max_p=std::max(max_p,dp);max_l=std::max(max_l,dl);max_e=std::max(max_e,de);
            require(dp<1e-9&&dl<1e-9,"total momentum balance failed");
            require(de<1e-9*std::max(1.,before.energy),"kinetic work identity failed");
            if(n%4==0){
                ++separating;
                require(receipt.before_normal_velocity>0&&receipt.normal_impulse==0&&
                        before.energy==after.energy,"separating pair changed");
            }else{
                ++closing;
                require(receipt.before_normal_velocity<0&&receipt.normal_impulse>0&&
                    std::abs(receipt.after_normal_velocity+e*receipt.before_normal_velocity)<1e-10,
                    "Newton restitution identity failed");
            }
        }
        // Actual closest finite-triangle witnesses; both finite masses.
        ContactTriangleState a,b;
        a.x={V3{0,0,.01},V3{1,0,.01},V3{0,1,.01}};
        b.x={V3{0,0,0},V3{1,0,0},V3{0,1,0}};
        a.mass={1,2,3};b.mass={2,3,4};
        for(auto& v:a.v)v={0,0,-1};
        const auto near=triangle_triangle_closest(a.x,b.x);
        const auto r=apply_two_triangle_normal_impulse(a,b,near.deformable_weights,near.rigid_weights,1.);
        require(r.normal_impulse>0&&std::abs(r.after_normal_velocity-1.)<1e-12,
                "finite closest-witness elastic contact failed");
        bool rejected=false;
        const auto previous=measure(a,b);
        try{(void)apply_two_triangle_normal_impulse(a,b,{1,1,1},{1,0,0},0.);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&measure(a,b).energy==previous.energy,"invalid weights mutated state");
        std::cout<<"PASS local coupled normal impulse: closing="<<closing<<" separating="<<separating
                 <<" max_linear_residual="<<max_p<<" max_angular_residual="<<max_l
                 <<" max_kinetic_residual="<<max_e<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

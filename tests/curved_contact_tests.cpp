#include "kyoto/curved_contact.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
using namespace kyoto::solid;
static void must(bool v,const std::string& desc){
    if(!v)throw std::runtime_error(desc);
}
static bool close(double a,double b,double tol=1e-11){
    return std::abs(a-b)<tol;
}
int main(){
 try {
    const RigidSphere unit{{0,0,0},1.,.15,.25};
    {
        const auto hit=swept_vertex_sphere({-2,0,0},{2,0,0},unit);
        must(hit.hit,"outside-to-outside fast crossing missed");
        must(close(hit.time_fraction,.25),"wrong exact sphere crossing time");
        must(close(hit.surface_point.x,-1),"wrong inward hit position");
        must(close(hit.normal.x,-1),"wrong outward normal");
    }
    {
        const auto hit=swept_vertex_sphere({-2,1.1,0},{2,1.1,0},unit);
        must(!hit.hit,"separated segment reported as sphere impact");
        const auto tangent=swept_vertex_sphere({-2,1,0},{2,1,0},unit);
        must(tangent.hit&&close(tangent.time_fraction,.5),
             "grazing tangency not retained");
    }
    {
        const auto toward=swept_vertex_sphere({1,0,0},{0,0,0},unit);
        const auto away=swept_vertex_sphere({1,0,0},{2,0,0},unit);
        must(toward.hit&&close(toward.time_fraction,0),
             "inward boundary step missed");
        must(!away.hit,"outward boundary step spuriously detected");
    }
    {
        bool threw=false;
        try{(void)swept_vertex_sphere({0,0,0},{2,0,0},unit);}
        catch(const std::invalid_argument&){threw=true;}
        must(threw,"initially penetrating vertex not rejected");
    }
    {
        bool threw=false;
        auto invalid=unit;invalid.radius=-.1;
        try{(void)swept_vertex_sphere({-2,0,0},{2,0,0},invalid);}
        catch(const std::invalid_argument&){threw=true;}
        must(threw,"invalid sphere radius not rejected");
    }
    {
        // Independent high-precision polynomial sign check along the
        // accepted prefix of the original chord. This is a NUMERIC
        // regression, not a machine proof for arbitrary FP platforms.
        std::mt19937_64 gen(9233321);
        std::uniform_real_distribution<double> pos(-2.,2.);
        for(int k=0;k<3000;k++){
            V3 a{pos(gen),pos(gen),pos(gen)};
            V3 b{pos(gen),pos(gen),pos(gen)};
            if(norm(a)<1.05)continue;
            const auto hit=swept_vertex_sphere(a,b,unit);
            const double tau=hit.hit?
                std::max(0.,hit.time_fraction-1e-9):1.;
            const V3 step=(b-a)*tau;
            for(int j=0;j<=48;j++){
                const double t=double(j)/48.;
                const V3 p=a+step*t;
                must(norm(p)>=1-2e-9,
                     "accepted swept chord enters sphere");
            }
        }
    }
    {
        // True 3D continuum mechanics, no prescribed motion.
        const auto sphere=RigidSphere{};
        auto body=toroidal_solid();
        const auto before=body.diagnostics(-100);
        std::size_t contacts=0;
        double greatest_impulse=0;
        double minimum_j=std::numeric_limits<double>::infinity();
        double peak_stress=0;
        for(int k=0;k<900;k++){
            const auto result=step_rigid_sphere(body,sphere,.00075);
            contacts+=result.normal_impulses;
            greatest_impulse=std::max(greatest_impulse,
                                     result.normal_impulse_ns);
            const auto d=body.diagnostics(-100);
            must(std::isfinite(d.elastic_joule)&&
                 std::isfinite(d.kinetic_joule)&&d.min_jacobian>1e-6,
                 "nonfinite/inverted FEM under curved contact");
            must(result.minimum_clearance_m>=-2e-9,
                 "accepted vertex inside curved rigid obstacle");
            minimum_j=std::min(minimum_j,d.min_jacobian);
            peak_stress=std::max(peak_stress,d.max_von_mises_pa);
        }
        must(contacts>0 && greatest_impulse>0,
             "physically integrated torus never touches curved obstacle");
        must(peak_stress>2.,"no hyperelastic stress during impact");
        must(minimum_j<.999,"no physical deformation during curved collision");
        const auto after=body.diagnostics(-100);
        must(close(before.total_mass_kg,after.total_mass_kg,1e-8),
             "total 3D mass changed during curved contact");
        must(close(after.time_seconds,.675,1e-10),
             "physical integration clock incorrect");
        std::cout<<"CURVED_FEM_PASS 900 steps, contacts="<<contacts
                 <<" peak_stress_pa="<<peak_stress
                 <<" min_J="<<minimum_j
                 <<" max_normal_impulse_ns="<<greatest_impulse<<'\n';
    }
    std::cout<<"PASS: analytic swept sphere CCD, 3000 seeded random "
                "chords, tangent/near-miss/tunnelling, true 900-step "
                "free deforming 3D neo-Hookean torus impact\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

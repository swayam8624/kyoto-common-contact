#include "kyoto/solid_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
using namespace kyoto::solid;
static void req(bool ok,const std::string& why) {
    if(!ok)throw std::runtime_error(why);
}
static bool near(double a,double b,double atol,double rtol=0) {
    return std::abs(a-b)<=atol+rtol*std::max(std::abs(a),std::abs(b));
}
static double potential(const Body& b){return b.evaluate().elastic_joule;}
int main() {
    try {
        Body body=toroidal_solid();
        req(body.nodes.size()==180,"wrong 3D torus node count");
        req(body.tets.size()==480,"wrong 3D solid tetra count");
        req(body.surface_faces.size()==320,"wrong closed boundary face count");
        const auto start=body.diagnostics();
        req(start.total_mass_kg>100&&start.total_mass_kg<400,
            "physically wrong body mass");
        req(near(start.total_mass_kg,
                 body.material.density*body.rest_volume,1e-9),
            "mass does not conserve integrated reference density");
        req(std::abs(start.elastic_joule)<1e-7,
            "undeformed neo-Hookean solid has nonzero energy");
        req(near(start.min_jacobian,1.,1e-9),
            "reference J must equal one");
        {
            auto f=body.evaluate();
            V3 net{};
            for(const auto& x:f.elastic_force)net+=x;
            req(norm(net)<1e-7,
                "unbalanced internal force violates linear momentum");
            for(const auto& x:f.elastic_force)
                req(norm(x)<1e-7,
                    "rest configuration generates spurious elastic force");
        }
        {
            auto translated=body;
            for(auto& n:translated.nodes)n.x+=V3{1.5,-2.1,3.4};
            req(near(potential(translated),start.elastic_joule,1e-7),
                "neo-Hookean energy not translation invariant");
            const auto f=translated.evaluate();
            V3 net{};
            for(const auto& x:f.elastic_force)net+=x;
            req(norm(net)<1e-7,
                "translation breaks net elastic internal force");
        }
        {
            auto rotated=body;
            const double angle=.73,c=std::cos(angle),s=std::sin(angle);
            for(auto& n:rotated.nodes) {
                const auto x=n.x;
                n.x={c*x.x-s*x.y,s*x.x+c*x.y,x.z};
            }
            const auto f=rotated.evaluate();
            req(f.elastic_joule<1e-7,"rigid rotation changes strain energy");
            req(std::abs(f.min_jacobian-1.)<1e-8,
                "rotation J not one");
            for(const auto& x:f.elastic_force)
                req(norm(x)<1e-7,"rotation creates spurious force");
        }
        {
            auto disturbed=body;
            const std::size_t node=20;
            disturbed.nodes[node].x.x+=.012;
            disturbed.nodes[node].x.y-=.007;
            const auto field=disturbed.evaluate();
            const double analytical=-field.elastic_force[node].x;
            const double h=1e-6;
            disturbed.nodes[node].x.x+=h;
            const double plus=potential(disturbed);
            disturbed.nodes[node].x.x-=2*h;
            const double minus=potential(disturbed);
            const double numerical=(plus-minus)/(2*h);
            req(near(analytical,numerical,5e-3,4e-4),
                "neo-Hookean analytical force fails finite-difference energy gradient");
        }
        {
            auto invalid=body;
            invalid.nodes[0].x={0,0,std::numeric_limits<double>::quiet_NaN()};
            bool thrown=false;
            try{(void)invalid.evaluate();}
            catch(const std::exception&){thrown=true;}
            req(thrown,"NaN deformation silently accepted");
        }
        {
            // Ballistic center-of-mass check far above the floor, with
            // no internal deformation, elasticity = 0, damping = 0.
            auto gravity=body;
            gravity.material.young=0;
            gravity.material.damping=0;
            const auto z0=gravity.diagnostics(-100).center_of_mass.z;
            for(int i=0;i<100;i++)gravity.step(1e-3,-100);
            const auto z1=gravity.diagnostics(-100).center_of_mass.z;
            const double T=.1,dt=.001;
            const double symplectic_drop=
                .5*gravity.material.gravity*(T*T+T*dt);
            req(near(z0-z1,symplectic_drop,3e-8),
                "gravity-only ballistic trajectory invalid");
            req(gravity.contact_count_step==0,
                "artificial floor contact in free-fall test");
        }
        {
            // Actual hyperelastic, fully unconstrained volumetric toroidal
            // body falls and receives impulses from an infinite rigid
            // plane. No 3D vertex may remain below the floor.
            auto falling=body;
            const double dt=.00075;
            std::size_t collisions=0;
            double peak_stress=0,min_j=std::numeric_limits<double>::infinity();
            for(int i=0;i<900;i++){
                falling.step(dt,0);
                const auto d=falling.diagnostics(0);
                req(std::isfinite(d.elastic_joule)&&
                    std::isfinite(d.kinetic_joule)&&
                    std::isfinite(d.gravity_joule)&&
                    std::isfinite(d.min_jacobian),
                    "energy or deformation not finite");
                req(d.min_height>=-1e-12,"floor penetration after contact");
                req(d.min_jacobian>1e-6,"inverted tetra after physical integration");
                peak_stress=std::max(peak_stress,d.max_von_mises_pa);
                min_j=std::min(min_j,d.min_jacobian);
                collisions+=d.impact_count;
            }
            req(collisions>0,
                "real integrated elastic solid never reached ground");
            req(peak_stress>1.,
                "deformable body produced no meaningful material stress");
            req(min_j<.999,
                "deformation never affected volumetric Jacobian");
            const auto final=falling.diagnostics(0);
            req(final.max_displacement>.1,
                "solver did not integrate actual motion");
            req(near(final.total_mass_kg,start.total_mass_kg,1e-9),
                "mass changed during simulation");
            std::cout<<"FEM_PASS vertices="<<falling.nodes.size()
                     <<" tets="<<falling.tets.size()
                     <<" surface_faces="<<falling.surface_faces.size()
                     <<" kg="<<final.total_mass_kg
                     <<" impacts="<<collisions
                     <<" peak_von_mises_Pa="<<peak_stress
                     <<" min_J="<<min_j
                     <<" end_time_s="<<final.time_seconds<<'\n';
        }
        std::cout<<"PASS: true 3D hyperelastic FEM: rest/rotation/translation "
                    "invariance, finite-difference force, ballistic gravity, "
                    "unilateral contact, 900 physical steps, positive J\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
    }
}

#pragma once
// Kyoto Pass 5: small physically integrated *volumetric* deformable-body
// reference solver. 3D tetrahedral compressible neo-Hookean hyperelasticity,
// lumped masses, semi-implicit Euler, analytic rigid-plane contact impulses.
// NO kinematic mesh scripting. This is a reference, NOT production IPC FEM.
//
// E = sum_tets V0*[mu/2*(trace(F^T F)-3)-mu*ln J+lambda/2*(ln J)^2]
// F=Ds*inv(Dm), J=det(F)>0
// P = mu*(F-F^-T)+lambda*ln(J)*F^-T
// f_i = -V0*P*(grad_X N_i), sum_i f_i = 0 per element.
// Contact plane z >= floor_z: analytic one-sided crossing detection,
// position projection, lumped mass impulse, Coulomb-like tangential impulse.
// This floor-specific contact is NOT a substitute for general triangle CCD.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::solid {

struct V3 { double x{},y{},z{}; };
inline V3 operator+(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V3 operator-(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V3 operator-(V3 a){return {-a.x,-a.y,-a.z};}
inline V3 operator*(V3 a,double k){return {a.x*k,a.y*k,a.z*k};}
inline V3 operator*(double k,V3 a){return a*k;}
inline V3 operator/(V3 a,double k){return a*(1.0/k);}
inline V3& operator+=(V3& a,V3 b){a=a+b;return a;}
inline double dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V3 cross(V3 a,V3 b){
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline double norm(V3 a){return std::sqrt(dot(a,a));}
inline bool finite(V3 a){
    return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);
}

struct M3 {
    double a[3][3]{};
    double& operator()(int i,int j){return a[i][j];}
    double operator()(int i,int j)const{return a[i][j];}
};
inline M3 identity(){M3 a;for(int i=0;i<3;i++)a(i,i)=1;return a;}
inline M3 from_cols(V3 x,V3 y,V3 z){
    M3 a;const V3 cols[3]={x,y,z};
    for(int j=0;j<3;j++){
        a(0,j)=cols[j].x;a(1,j)=cols[j].y;a(2,j)=cols[j].z;
    }return a;
}
inline V3 column(const M3& m,int j){
    return {m(0,j),m(1,j),m(2,j)};
}
inline M3 transpose(const M3& m){
    M3 a;for(int i=0;i<3;i++)for(int j=0;j<3;j++)a(i,j)=m(j,i);
    return a;
}
inline M3 operator+(const M3& a,const M3& b){
    M3 c;for(int i=0;i<3;i++)for(int j=0;j<3;j++)c(i,j)=a(i,j)+b(i,j);
    return c;
}
inline M3 operator-(const M3& a,const M3& b){
    M3 c;for(int i=0;i<3;i++)for(int j=0;j<3;j++)c(i,j)=a(i,j)-b(i,j);
    return c;
}
inline M3 operator*(const M3& a,double k){
    M3 c;for(int i=0;i<3;i++)for(int j=0;j<3;j++)c(i,j)=a(i,j)*k;
    return c;
}
inline M3 operator*(const M3& a,const M3& b){
    M3 c;
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)
        for(int k=0;k<3;k++)c(i,j)+=a(i,k)*b(k,j);
    return c;
}
inline V3 operator*(const M3& a,V3 b){
    return {a(0,0)*b.x+a(0,1)*b.y+a(0,2)*b.z,
            a(1,0)*b.x+a(1,1)*b.y+a(1,2)*b.z,
            a(2,0)*b.x+a(2,1)*b.y+a(2,2)*b.z};
}
inline double determinant(const M3& m){
    return dot(column(m,0),cross(column(m,1),column(m,2)));
}
inline M3 inverse(const M3& m){
    const V3 a=column(m,0),b=column(m,1),c=column(m,2);
    const double j=dot(a,cross(b,c));
    if(!(std::isfinite(j)&&std::abs(j)>1e-13))
        throw std::runtime_error("singular tetrahedron deformation");
    const V3 rows[3]={cross(b,c)/j,cross(c,a)/j,cross(a,b)/j};
    M3 result;
    for(int i=0;i<3;i++){
        result(i,0)=rows[i].x;
        result(i,1)=rows[i].y;
        result(i,2)=rows[i].z;
    }
    return result;
}
inline double trace(const M3& a){return a(0,0)+a(1,1)+a(2,2);}
inline double frob2(const M3& a){
    double r=0;for(int i=0;i<3;i++)for(int j=0;j<3;j++)r+=a(i,j)*a(i,j);
    return r;
}
struct Material{
    double density{360.0};       // kg/m^3
    double young{22000.0};       // Pa
    double poisson{0.32};        // 0 < nu < 0.5
    double gravity{9.81};        // m/s^2
    double damping{0.45};        // 1/s (nonconservative)
    double restitution{0.12};    // lumped impact restitution
    double friction{0.28};       // Coulomb-like tangential impulse
};
inline void validate(const Material& m){
    if(!(m.density>0&&std::isfinite(m.density)&&m.young>=0&&
         std::isfinite(m.young)&&m.poisson>0&&m.poisson<.5&&
         std::isfinite(m.gravity)&&m.gravity>=0&&
         m.damping>=0&&std::isfinite(m.damping)&&
         m.restitution>=0&&m.restitution<=1&&
         m.friction>=0&&std::isfinite(m.friction)))
        throw std::invalid_argument("invalid physical material parameters");
}
struct Node {
    V3 rest{},x{},v{};
    double mass{};
    double last_normal_impulse{};
};
struct Tet {
    std::array<std::size_t,4> n{};
    M3 inverse_rest{};
    double rest_volume{};
};
struct Fields {
    std::vector<V3> elastic_force;
    std::vector<double> nodal_energy_density;
    std::vector<double> nodal_von_mises_pa;
    double elastic_joule{};
    double min_jacobian{std::numeric_limits<double>::infinity()};
    double max_von_mises_pa{};
};
struct Diagnostics {
    double kinetic_joule{},gravity_joule{},elastic_joule{};
    double min_jacobian{},max_von_mises_pa{},max_displacement{};
    double min_height{std::numeric_limits<double>::infinity()};
    V3 center_of_mass{};
    std::size_t impact_count{};
    double normal_impulse_newton_second{};
    double total_mass_kg{},time_seconds{};
};
struct Body {
    Material material{};
    std::vector<Node> nodes;
    std::vector<Tet> tets;
    std::vector<std::array<std::size_t,3>> surface_faces;
    std::vector<std::array<std::size_t,2>> surface_edges;
    double time_seconds{};
    std::size_t contact_count_step{};
    double normal_impulse_step{};
    double rest_volume{};

    double mu() const {
        return material.young/(2*(1+material.poisson));
    }
    double lambda()const{
        return material.young*material.poisson/
            ((1+material.poisson)*(1-2*material.poisson));
    }
    void add_tet(std::array<std::size_t,4> ids){
        for(auto k:ids)if(k>=nodes.size())
            throw std::invalid_argument("tet refers to invalid node");
        auto dm=[&](){
            const auto p=nodes[ids[0]].rest;
            return from_cols(nodes[ids[1]].rest-p,
                             nodes[ids[2]].rest-p,
                             nodes[ids[3]].rest-p);
        };
        M3 A=dm();double d=determinant(A);
        if(d<0){
            std::swap(ids[2],ids[3]);
            A=dm();d=determinant(A);
        }
        if(!(std::isfinite(d)&&d>1e-12))
            throw std::invalid_argument("degenerate reference tetrahedron");
        const double vol=d/6.0;
        const double mass=vol*material.density/4.0;
        for(const auto i:ids)nodes[i].mass+=mass;
        rest_volume+=vol;
        tets.push_back({ids,inverse(A),vol});
    }
    Fields evaluate()const {
        Fields out;
        out.elastic_force.resize(nodes.size());
        out.nodal_energy_density.resize(nodes.size());
        out.nodal_von_mises_pa.resize(nodes.size());
        std::vector<double> nodal_volume(nodes.size(),0);
        const double shear=mu(),bulk=lambda();
        for(const auto& cell:tets){
            const auto& a=nodes[cell.n[0]].x;
            const M3 ds=from_cols(
                nodes[cell.n[1]].x-a,nodes[cell.n[2]].x-a,
                nodes[cell.n[3]].x-a);
            const M3 F=ds*cell.inverse_rest;
            const double j=determinant(F);
            if(!(std::isfinite(j)&&j>1e-6))
                throw std::runtime_error("invalid/inverted FEM tetrahedron");
            out.min_jacobian=std::min(out.min_jacobian,j);
            const double lnJ=std::log(j);
            const M3 invFt=transpose(inverse(F));
            const M3 first_piola=(F-invFt)*shear+invFt*(bulk*lnJ);
            const double W=.5*shear*(frob2(F)-3)-
                           shear*lnJ+.5*bulk*lnJ*lnJ;
            if(!(std::isfinite(W)&&W>=-1e-7))
                throw std::runtime_error("nonfinite/negative neo-Hookean energy");
            out.elastic_joule+=cell.rest_volume*std::max(0.,W);
            const M3 force_directions=
                first_piola*transpose(cell.inverse_rest);
            const V3 f1=column(force_directions,0)*(-cell.rest_volume);
            const V3 f2=column(force_directions,1)*(-cell.rest_volume);
            const V3 f3=column(force_directions,2)*(-cell.rest_volume);
            const V3 f0=-(f1+f2+f3);
            const V3 f[4]={f0,f1,f2,f3};
            const M3 sigma=(first_piola*transpose(F))*(1.0/j);
            const double a0=sigma(0,0),b0=sigma(1,1),c0=sigma(2,2);
            const double vm=std::sqrt(std::max(0.,.5*(
                (a0-b0)*(a0-b0)+(b0-c0)*(b0-c0)+
                (c0-a0)*(c0-a0))+
                3*(sigma(0,1)*sigma(0,1)+sigma(1,2)*sigma(1,2)+
                   sigma(0,2)*sigma(0,2))));
            out.max_von_mises_pa=std::max(out.max_von_mises_pa,vm);
            for(std::size_t k=0;k<4;k++){
                const auto i=cell.n[k];
                out.elastic_force[i]+=f[k];
                nodal_volume[i]+=cell.rest_volume/4.0;
                out.nodal_energy_density[i]+=W*cell.rest_volume/4.0;
                out.nodal_von_mises_pa[i]+=vm*cell.rest_volume/4.0;
            }
        }
        for(std::size_t i=0;i<nodes.size();i++){
            if(!(nodal_volume[i]>0&&nodes[i].mass>0))
                throw std::runtime_error("orphan/zero-mass FEM vertex");
            out.nodal_energy_density[i]/=nodal_volume[i];
            out.nodal_von_mises_pa[i]/=nodal_volume[i];
            if(!finite(out.elastic_force[i]))
                throw std::runtime_error("nonfinite internal elastic force");
        }
        return out;
    }
    Diagnostics diagnostics(double plane_z=0)const{
        const auto f=evaluate();
        Diagnostics d;
        d.elastic_joule=f.elastic_joule;
        d.min_jacobian=f.min_jacobian;
        d.max_von_mises_pa=f.max_von_mises_pa;
        d.impact_count=contact_count_step;
        d.normal_impulse_newton_second=normal_impulse_step;
        d.time_seconds=time_seconds;
        for(const auto& v:nodes){
            d.total_mass_kg+=v.mass;
            d.center_of_mass+=v.x*v.mass;
            d.kinetic_joule+=.5*v.mass*dot(v.v,v.v);
            d.gravity_joule+=v.mass*material.gravity*(v.x.z-plane_z);
            d.min_height=std::min(d.min_height,v.x.z);
            d.max_displacement=std::max(d.max_displacement,norm(v.x-v.rest));
        }
        d.center_of_mass=d.center_of_mass/d.total_mass_kg;
        return d;
    }
    // Explicit dynamics with analytic plane CCD/projection, not general CCD.
    // The unilateral floor is infinite and frictional; the mesh itself
    // is a fully 3D deformable hyperelastic volume with no pinned nodes.
    void step(double dt,double floor_z=0.0){
        if(!(dt>0&&std::isfinite(dt)&&std::isfinite(floor_z)))
            throw std::invalid_argument("invalid time step or plane");
        const auto forces=evaluate();
        const double damp=std::exp(-material.damping*dt);
        std::vector<V3> velocity(nodes.size());
        std::vector<V3> position(nodes.size());
        contact_count_step=0;normal_impulse_step=0;
        std::vector<double> impulse(nodes.size(),0);
        for(std::size_t i=0;i<nodes.size();i++){
            const auto& n=nodes[i];
            const V3 acceleration=forces.elastic_force[i]/n.mass+
                                  V3{0,0,-material.gravity};
            velocity[i]=(n.v+acceleration*dt)*damp;
            position[i]=n.x+velocity[i]*dt;
            if(position[i].z<floor_z){
                // Exact analytic unilateral z(t) crossing of this
                // linear trial path. Clamp the *final* state to admissible.
                const double predicted_penetration=floor_z-position[i].z;
                if(!(n.x.z>=floor_z-1e-11&&predicted_penetration>0))
                    throw std::runtime_error("invalid floor precondition");
                position[i].z=floor_z;
                if(velocity[i].z<0) {
                    const double j=-(1+material.restitution)*n.mass*velocity[i].z;
                    velocity[i].z=-material.restitution*velocity[i].z;
                    const V3 vt{velocity[i].x,velocity[i].y,0};
                    const double tangential=norm(vt);
                    if(tangential>0){
                        const double reduction=std::max(0.,
                            1-material.friction*j/(n.mass*tangential));
                        velocity[i].x*=reduction;
                        velocity[i].y*=reduction;
                    }
                    impulse[i]=j;
                    normal_impulse_step+=j;
                    contact_count_step++;
                }else{
                    velocity[i].z=std::max(0.,velocity[i].z);
                }
            }
            if(!finite(position[i])||!finite(velocity[i]))
                throw std::runtime_error("nonfinite dynamic state");
        }
        // Validate new tetra orientations BEFORE committing positions.
        // With too-large dt, fail loudly rather than fabricate
        // numerically stable or geometrically valid dynamics.
        for(const auto& t:tets){
            const auto a=position[t.n[0]];
            const auto ds=from_cols(position[t.n[1]]-a,
                        position[t.n[2]]-a,position[t.n[3]]-a);
            const auto J=determinant(ds*t.inverse_rest);
            if(!(std::isfinite(J)&&J>1e-6))
                throw std::runtime_error("trial step inverts a tetrahedron: decrease dt");
        }
        for(std::size_t i=0;i<nodes.size();i++){
            nodes[i].v=velocity[i];
            nodes[i].x=position[i];
            nodes[i].last_normal_impulse=impulse[i];
        }
        time_seconds+=dt;
    }
};

// True connected solid toroidal lattice, periodic around major circle.
// Rectangular cross-section is intentional: tetrahedralizes exactly
// without singular radial coordinates or open seam at angle 2*pi.
inline Body toroidal_solid(unsigned major=20,
                           unsigned radial=3,
                           unsigned vertical=3,
                           Material material={}) {
    validate(material);
    if(major<8||major>64||radial<2||radial>8||
       vertical<2||vertical>8)
        throw std::invalid_argument("invalid toroidal FEM resolution");
    Body body;body.material=material;
    constexpr double R=.70, half_width=.18,initial_height=.52;
    const double tau=2*std::numbers::pi;
    const auto index=[&](unsigned i,unsigned j,unsigned k)->std::size_t{
        return (std::size_t(i%major)*radial+j)*vertical+k;
    };
    for(unsigned i=0;i<major;i++){
        const double theta=tau*double(i)/major;
        for(unsigned j=0;j<radial;j++){
            const double u=-half_width+2*half_width*j/(radial-1);
            for(unsigned k=0;k<vertical;k++){
                const double v=-half_width+2*half_width*k/(vertical-1);
                const V3 p{(R+u)*std::cos(theta),
                           (R+u)*std::sin(theta),
                           initial_height+v};
                body.nodes.push_back({p,p,{0,0,0},0,0});
            }
        }
    }
    for(unsigned i=0;i<major;i++)for(unsigned j=0;j+1<radial;j++)
        for(unsigned k=0;k+1<vertical;k++){
            const auto a=index(i,j,k),b=index(i+1,j,k);
            const auto c=index(i,j+1,k),d=index(i+1,j+1,k);
            const auto e=index(i,j,k+1),f=index(i+1,j,k+1);
            const auto g=index(i,j+1,k+1),h=index(i+1,j+1,k+1);
            body.add_tet({a,b,d,h});
            body.add_tet({a,d,c,h});
            body.add_tet({a,c,g,h});
            body.add_tet({a,g,e,h});
            body.add_tet({a,e,f,h});
            body.add_tet({a,f,b,h});
        }
    const auto quad=[&](std::size_t a,std::size_t b,std::size_t c,
                        std::size_t d,V3 outward){
        const auto n=cross(body.nodes[b].rest-body.nodes[a].rest,
                           body.nodes[d].rest-body.nodes[a].rest);
        if(dot(n,outward)>0){
            body.surface_faces.push_back({a,b,c});
            body.surface_faces.push_back({a,c,d});
        }else{
            body.surface_faces.push_back({a,d,c});
            body.surface_faces.push_back({a,c,b});
        }
    };
    for(unsigned i=0;i<major;i++){
        const double theta=tau*(i+.5)/major;
        const V3 radial_out{std::cos(theta),std::sin(theta),0};
        for(unsigned k=0;k+1<vertical;k++){
            for(unsigned j:{0u,radial-1}){
                const double sign=j==0?-1:1;
                quad(index(i,j,k),index(i+1,j,k),
                     index(i+1,j,k+1),index(i,j,k+1),
                     radial_out*sign);
            }
        }
        for(unsigned j=0;j+1<radial;j++){
            for(unsigned k:{0u,vertical-1}){
                const double sign=k==0?-1:1;
                quad(index(i,j,k),index(i+1,j,k),
                     index(i+1,j+1,k),index(i,j+1,k),{0,0,sign});
            }
        }
    }
    std::set<std::array<std::size_t,2>> edges;
    for(const auto& tri:body.surface_faces)
        for(int k=0;k<3;k++){
            const auto a=tri[k],b=tri[(k+1)%3];
            edges.insert({std::min(a,b),std::max(a,b)});
        }
    body.surface_edges.assign(edges.begin(),edges.end());
    if(!(body.rest_volume>0))
        throw std::runtime_error("FEM toroidal mesh has no volume");
    return body;
}
} // namespace kyoto::solid

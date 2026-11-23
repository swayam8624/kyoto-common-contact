// Pass23 isolated original 3D coupled neo-Hookean contact tests.
// Two deformable FEM bodies receive equal-and-opposite finite-face
// contact impulses and only accepted physical CCD-safe timesteps commit.
// This is NOT general IPC contact, closed-body topology or self-contact.
#include "kyoto/coupled_deformable_fem_contact.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace s=kyoto::solid;

static void require(bool ok,const char* reason){
    if(!ok)throw std::runtime_error(reason);
}
static s::Body placed(double x){
    auto result=s::toroidal_solid(8);
    for(auto& n:result.nodes){
        n.x.x+=x;
        n.rest.x+=x;
    }
    return result;
}
static double kinetic(const s::Body& a,const s::Body& b,
                      const std::vector<s::V3>& va,
                      const std::vector<s::V3>& vb){
    double total=0;
    for(std::size_t i=0;i<va.size();++i)
        total+=.5*a.nodes[i].mass*s::dot(va[i],va[i]);
    for(std::size_t i=0;i<vb.size();++i)
        total+=.5*b.nodes[i].mass*s::dot(vb[i],vb[i]);
    return total;
}
int main(){
 try{
    // Isolated impulse kernel: no gravity, no damping and no elastic
    // integration. This precisely tests pairwise conservation of
    // linear momentum and noncreation of kinetic contact energy.
    const auto mass_a=placed(-1.3),mass_b=placed(1.3);
    std::vector<s::V3> va(mass_a.nodes.size(),{0,0,-1.0});
    std::vector<s::V3> vb(mass_b.nodes.size(),{0,0, 1.0});
    const auto p_before=s::coupled_total_linear_momentum(
        mass_a,mass_b,va,vb);
    const double e_before=kinetic(mass_a,mass_b,va,vb);
    s::RigidFaceContact near;
    near.on_deformable={0,0,.01};
    near.on_rigid={0,0,0};
    near.distance=.01;
    near.deformable_weights={1,0,0};
    near.rigid_weights={1,0,0};
    s::CoupledDeformableSettings cfg;
    cfg.friction=0.;
    cfg.restitution=.1;
    s::CoupledFEMReceipt impulse;
    require(s::apply_coupled_face_impulse(
        mass_a,mass_b,mass_a.surface_faces.front(),
        mass_b.surface_faces.front(),near,va,vb,cfg,impulse),
        "symmetric finite-mass contact did not change both velocities");
    const auto p_after=s::coupled_total_linear_momentum(
        mass_a,mass_b,va,vb);
    const double e_after=kinetic(mass_a,mass_b,va,vb);
    const double tolerance=1e-10*std::max(1.,s::norm(p_before));
    require(s::norm(p_after-p_before)<tolerance,
        "equal/opposite coupled impulse violated total linear momentum");
    require(e_after<=e_before+1e-10*std::max(1.,e_before),
        "finite-mass barycentric contact impulse created kinetic energy");
    require(s::norm(impulse.impulse_on_a+impulse.impulse_on_b)<1e-12,
        "contact impulse lost equal-and-opposite reaction");
    require(impulse.impulse_updates==1&&impulse.normal_impulse_ns>0,
        "isolated finite-mass impulse was not recorded");

    // A genuine volumetric, freely falling, non-contact physical step:
    // both independent solids receive elastic forces and gravity,
    // but advance transactionally under the SAME shared 3D clock.
    auto far_a=placed(-1.3),far_b=placed(1.3);
    std::vector<s::CoupledFEMAcceptedLeaf> accepted;
    const auto far=s::step_time_consistent_coupled_deformables(
        far_a,far_b,.00075,{},&accepted);
    require(far.accepted_leaves==1&&accepted.size()==1,
        "original pair did not accept exactly one physical leaf");
    require(std::abs(far.integrated_dt_s-.00075)<1e-12&&
            std::abs(far_a.time_seconds-.00075)<1e-12&&
            std::abs(far_b.time_seconds-.00075)<1e-12,
        "coupled pair integrated the wrong physical elapsed time");
    require(far.min_accepted_face_gap_m>.00025&&
            far.min_accepted_tet_j>0.,
        "physical coupled FEM state failed geometry or volume gate");
    require(accepted[0].a_before.nodes[0].x.z>
            accepted[0].a_after.nodes[0].x.z,
        "falling original FEM body did not move under gravity");
    require(accepted[0].b_before.nodes[0].x.z>
            accepted[0].b_after.nodes[0].x.z,
        "second original FEM body did not move under gravity");

    // Start two actual deformable surfaces close enough to activate
    // face-face impulses but strictly nonpenetrating. Both move toward
    // one another, so the second body MUST react as finite-mass.
    auto approaching_a=placed(-.91),approaching_b=placed(.91);
    for(auto& n:approaching_a.nodes)n.v={+.5,0,0};
    for(auto& n:approaching_b.nodes)n.v={-.5,0,0};
    s::CoupledDeformableSettings contact_cfg;
    contact_cfg.activation_width_m=.09;
    contact_cfg.friction=0.;
    contact_cfg.impulse_iterations=1;
    const auto contact=s::try_coupled_deformable_fem_step(
        approaching_a,approaching_b,.0001,contact_cfg);
    require(contact.accepted,
            "genuinely approaching finite-mass bodies failed safe physical leaf");
    require(contact.impulse_updates>0,
            "two genuinely approaching deformables did not exchange impulses");
    require(s::norm(contact.impulse_on_a+contact.impulse_on_b)<1e-11,
            "accepted coupled step did not have balanced contact reactions");
    require(contact.min_face_gap_m>contact_cfg.skin_m*.5&&
            contact.min_tet_j>1e-6,
            "accepted physically coupled contact penetrated or inverted tets");
    require(std::abs(approaching_a.time_seconds-.0001)<1e-12&&
            std::abs(approaching_b.time_seconds-.0001)<1e-12,
            "mutual-impulse solver lost the accepted physical clock");

    // Explicitly bad settings must fail BEFORE changing either body.
    const auto old_pos_a=approaching_a.nodes.front().x;
    const auto old_pos_b=approaching_b.nodes.front().x;
    const double old_time=approaching_a.time_seconds;
    auto invalid=contact_cfg;
    invalid.interval_depth=1;
    bool rejected=false;
    try{
        (void)s::step_time_consistent_coupled_deformables(
            approaching_a,approaching_b,.00075,invalid);
    }catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"invalid continuous coupling parameters accepted");
    require(std::abs(approaching_a.time_seconds-old_time)<1e-12&&
            std::abs(approaching_b.time_seconds-old_time)<1e-12&&
            s::norm(approaching_a.nodes.front().x-old_pos_a)<1e-12&&
            s::norm(approaching_b.nodes.front().x-old_pos_b)<1e-12,
            "failed pair trial changed the publicly accepted physical bodies");

    std::cout<<"PASS23 original two-body volumetric neo-Hookean physics, "
             <<"finite-mass equal/opposite face contact, kinetic impulse "
             <<"noncreation, true accepted physical clock, 3D surface "
             <<"separation, noninverted tets and rollback after invalid input. "
             <<"No general IPC solver/CCD proof claimed.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS23 physically coupled FEM FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

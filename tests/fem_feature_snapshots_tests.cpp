// Independent controls for the *shared* physically integrated source
// adapter used for native maintained-tree AND official IPC measurements.
// This is a feature-box/metadata test, NOT a general moving mesh CCD proof.
#include "kyoto/fem_feature_snapshots.hpp"
#include "kyoto/refit_feature_hierarchy.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace f=kyoto::contact::features;
namespace s=kyoto::solid;
using kyoto::contact::evaluate;

static void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
int main() {
 try{
    auto before=s::toroidal_solid();
    const auto rigid=s::corrugated_contact_patch();
    auto after=before;
    const auto step=s::step_time_consistent_rigid_triangle_mesh(
        after,rigid,.00075);
    require(step.accepted_leaves>0,"physical snapshot source was not accepted");
    auto frame=kyoto::contact::fem::feature_snapshot(before,after,rigid);
    require(frame.vertices.size()==before.nodes.size()+rigid.vertices.size(),
        "shared source FEM vertex IDs changed");
    require(frame.faces.size()==before.surface_faces.size()+rigid.faces.size(),
        "shared source FEM original face connectivity changed");
    require(frame.topology_epoch==1,"shared feature topology epoch changed");
    for(std::size_t i=0;i<before.nodes.size();++i){
        const auto& motion=frame.vertices[i].path;
        const auto half=evaluate(motion,.5);
        const auto a=kyoto::contact::fem::cast(before.nodes[i].x);
        const auto b=kyoto::contact::fem::cast(after.nodes[i].x);
        const auto midpoint=kyoto::contact::affine(a,b,.5);
        require(std::abs(midpoint.x-half.x)<1e-12 &&
                std::abs(midpoint.y-half.y)<1e-12 &&
                std::abs(midpoint.z-half.z)<1e-12,
                "source adapter broke linearized physical endpoints");
    }
    kyoto::contact::atlas::RefitHierarchy hierarchy;
    const auto oracle=f::exhaustive(frame).pairs;
    require(hierarchy.query(frame).pairs==oracle,
        "shared source adapter not represented by maintained hierarchy");
    auto unchanged=frame;
    unchanged.thickness*=2;
    require(hierarchy.query(unchanged).pairs==f::exhaustive(unchanged).pairs,
        "changing contact skin changed retained hierarchy parity");
    require(hierarchy.rebuilds()==1 && hierarchy.refits()==1,
        "changing only finite contact thickness should refit");
    std::cout<<"PASS15 shared original FEM physical feature contract; "
        "macro-step linearization, stable native IDs and paid BVH refit\n";
    return 0;
 }catch(const std::exception& e) {
    std::cerr<<"PASS15 source adapter FAIL: "<<e.what()<<'\n';
    return 1;
 }
}

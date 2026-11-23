// Pass 20: exact original rigid-face box enumeration parity.
// Tests real triangle patch plus 9000 deterministically sampled geometric
// queries against independently exhaustive `maybe_near` enumeration.
// This is a broadphase test, NOT a physics-solver speedup result.
#include "kyoto/static_rigid_mesh_bvh.hpp"
#include "kyoto/rigid_mesh_fem_contact.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using namespace kyoto::solid;
static void ensure(bool yes,const char* reason){
    if(!yes)throw std::runtime_error(reason);
}
int main(){
 try{
    const auto rigid=corrugated_contact_patch();
    std::vector<TriangleBox> boxes;
    for(std::size_t f=0;f<rigid.faces.size();++f)
        boxes.push_back(bounding(face_positions(rigid,f)));
    StaticRigidMeshBVH tree(boxes);
    ensure(tree.face_count()==rigid.faces.size(),
           "lost original rigid patch topology");
    std::mt19937_64 gen(20261124);
    std::uniform_real_distribution<double> pos(-1.2,1.2);
    std::uniform_real_distribution<double> size(.00001,.3);
    for(int k=0;k<9000;++k){
        const double x=pos(gen),y=pos(gen),z=pos(gen);
        const TriangleBox query{{x,y,z},{x+size(gen),y+size(gen),z+size(gen)}};
        const double skin=k%4==0?0:k%4==1?.0005:k%4==2?.004:.06;
        std::vector<std::size_t> baseline;
        for(std::size_t j=0;j<boxes.size();++j)
            if(maybe_near(query,boxes[j],skin))baseline.push_back(j);
        std::size_t tree_nodes=0;
        const auto found=tree.query(query,skin,&tree_nodes);
        ensure(found==baseline,"source rigid BVH missed or added finite-face box candidate");
        ensure(std::is_sorted(found.begin(),found.end()),
               "contact impulse face order changed by BVH traversal");
        ensure(tree_nodes>0,"nonempty real rigid tree traversed no nodes");
    }
    // Also compare actual source FEM surface boxes, not only randomized
    // geometric inputs.
    auto body=toroidal_solid();
    for(const auto& face:body.surface_faces){
        const Triangle3 moving={body.nodes[face[0]].x,
                               body.nodes[face[1]].x,
                               body.nodes[face[2]].x};
        const auto bound=bounding(moving);
        std::vector<std::size_t> oracle;
        for(std::size_t j=0;j<boxes.size();++j)
            if(maybe_near(bound,boxes[j],.0035))oracle.push_back(j);
        ensure(tree.query(bound,.0035)==oracle,
               "actual native FEM face box candidate omission");
    }
    std::cout<<"PASS20 9000 independent AABB queries plus original 3D "
             <<"FEM/rigid-face candidate coverage and original face order\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL20 "<<e.what()<<'\n';
    return 1;
 }
}

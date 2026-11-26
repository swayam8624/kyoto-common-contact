// Check the actual native Body factories, rather than the independent
// standalone reconstruction in volume_boundary_fixture_tests.cpp.
#include "kyoto/connected_self_contact_fixture.hpp"
#include <iostream>
using namespace kyoto::solid;
static void check(const Body& b,std::size_t faces){
    std::vector<kyoto::geometry::CellRow> cells;
    for(const auto& t:b.tets)cells.push_back(t.n);
    const auto r=kyoto::geometry::validate_volume_boundary_topology(b.nodes.size(),cells,b.surface_faces);
    if(r.boundary_faces!=faces)throw std::runtime_error("native factory boundary receipt mismatch");
}
int main(){
    for(unsigned n:{8u,20u,64u})for(unsigned r:{2u,3u,8u})for(unsigned z:{2u,3u,8u})
        check(toroidal_solid(n,r,z),4*n*(r+z-2));
    for(unsigned w:{1u,4u,16u,32u,64u})
        check(fixtures::u_solid(w),24*w+20);
    std::cout<<"PASS actual native volume factory topology: 32 configurations\n";
}

#include "kyoto/coupled_triangle_normal_impulse.hpp"
#include <iostream>
using namespace kyoto::solid;
int main(){
    // Captured from the actual rotated 240-tet coupled trajectory.
    const V3 p{0.22456146359831272,-0.72149759611370756,0.83650081761390094},a{-0.049835498782815055,-0.56323767043082729,0.66541866495366864},b{0.22648146359831281,-0.72005759611370768,0.8347008176139008},c{0.20311939163440362,-0.49422423379591796,0.99044796403996282};
    auto r=independent_triangle_distance(p,a,b,c);
    double sum=0;for(auto w:r.barycentric){if(!(w>=0&&w<=1))throw std::runtime_error("invalid boundary weight");sum+=w;}
    if(std::abs(sum-1)>1e-14||norm(r.point-b)>1e-14||std::abs(r.distance-norm(p-b))>1e-14)throw std::runtime_error("boundary witness changed beyond roundoff");
    ContactTriangleState x{{p,p,p},{{V3{},V3{},V3{}}},{1,1,1}},y{{a,b,c},{{V3{},V3{},V3{}}},{1,1,1}};
    (void)apply_two_triangle_normal_impulse(x,y,{1,0,0},r.barycentric,0.);
    std::cout<<"PASS captured negative barycentric roundoff regression\n";
}

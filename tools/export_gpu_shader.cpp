#include "kyoto/gpu_packed_mask.hpp"
#include <iostream>
int main(int argc,char**){std::cout<<(argc==2?kyoto::contact::gpu_mask::metal_source():kyoto::contact::gpu_mask::opencl_source());}

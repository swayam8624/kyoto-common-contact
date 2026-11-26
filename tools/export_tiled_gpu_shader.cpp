#include "kyoto/tiled_gpu_mask.hpp"
#include <iostream>
int main(int argc,char**){std::cout<<(argc>1?kyoto::contact::gpu_tiles::metal_source():kyoto::contact::gpu_tiles::opencl_source());}

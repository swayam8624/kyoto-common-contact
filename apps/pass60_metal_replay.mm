// Real Metal execution only; no fallback, missing device is a hard failure.
#include "pass60_mask_replay.hpp"
#include "kyoto/metal_tiled_dispatch.hpp"
int main(int argc,char** argv){@autoreleasepool{try{
    need(argc==3,"usage: pass60_metal inputdir device.txt");
    id<MTLDevice> d=MTLCreateSystemDefaultDevice();need(d!=nil,"real Metal device required");
    std::ofstream meta(argv[2]);need(meta.good(),"cannot write device receipt");
    meta<<"backend=Metal\n"<<"name="<<d.name.UTF8String<<"\nregistry_id="<<d.registryID<<"\nmax_buffer="<<d.maxBufferLength<<'\n';
    kyoto::contact::metal_dispatch::MetalDispatch gpu(d);
    campaign(gpu,argv[1],std::cout);meta<<"completed=1\n";need(meta.good(),"device receipt incomplete");
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}}

// Scalar execution of exactly the shader body; NOT hardware validation.
#include "pass60_mask_replay.hpp"
int main(int argc,char** argv){try{need(argc==2,"usage: pass60_host inputdir");ScalarMask d;campaign(d,argv[1],std::cout);return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

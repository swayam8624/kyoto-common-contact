#include "tree.hpp"
static Pairs tree_pairs(const std::vector<Box>&a,const std::vector<Box>&b){Tree t(b);Pairs out;if(!b.empty())for(unsigned i=0;i<a.size();++i)t.visit(0,a[i],i,out);std::sort(out.begin(),out.end());return out;}
struct Policy{std::vector<Box> ea,eb;Pairs cached;bool ready=false;unsigned path=0;bool hit=false;
Pairs query(const std::vector<Box>&a,const std::vector<Box>&b,double skin){hit=false;auto p=uint64_t(a.size())*b.size();if(p<=100000){path=0;return brute(a,b);}uint64_t state=0xa17f129b;unsigned count=0;for(unsigned k=0;k<256;++k){state=state*6364136223846793005ULL+1;auto i=(state>>32)%a.size();state=state*6364136223846793005ULL+1;auto j=(state>>32)%b.size();count+=overlap(a[i],b[j]);}if(count>=16){path=0;ready=false;cached.clear();return brute(a,b);}bool reuse=ready&&ea.size()==a.size()&&eb.size()==b.size();for(size_t i=0;i<a.size()&&reuse;++i)reuse=contains(ea[i],a[i]);for(size_t i=0;i<b.size()&&reuse;++i)reuse=contains(eb[i],b[i]);if(!reuse){ea=expand(a,skin*.5);eb=expand(b,skin*.5);cached=tree_pairs(ea,eb);ready=cached.size()<=p/8;}path=reuse?3:2;hit=reuse;Pairs out;for(auto ij:cached)if(overlap(a[ij[0]],b[ij[1]]))out.push_back(ij);if(!ready)cached.clear();return out;}};
#include "external.hpp"
#ifdef KYOTO_BULLET
#include "bullet_adapter.hpp"
extern "C" unsigned bullet_scalar_bytes(){return sizeof(btScalar);}
#else
extern "C" unsigned bullet_scalar_bytes(){return 0;}
#endif
struct Context{Cache old;Policy policy;BoundedPolicy bounded;Pairs out;
#ifdef KYOTO_BULLET
BulletIndex bullet;
#endif
};
extern "C" void* make(){try{return new Context;}catch(...){return nullptr;}}
extern "C" void drop(void*c){delete static_cast<Context*>(c);}
extern "C" int run(void*c,const double*a,size_t na,const double*b,size_t nb,int mode,double skin,const unsigned**out,size_t*n,unsigned*path,unsigned*hit){try{auto&ctx=*static_cast<Context*>(c);std::vector<Box>A(na),B(nb);for(size_t i=0;i<na;i++)for(int k=0;k<3;k++){A[i].lo[k]=a[6*i+k];A[i].hi[k]=a[6*i+k+3];}for(size_t i=0;i<nb;i++)for(int k=0;k<3;k++){B[i].lo[k]=b[6*i+k];B[i].hi[k]=b[6*i+k+3];}validate(A);validate(B);if(!std::isfinite(skin)||skin<0)return -1;Stats s;*hit=0;*path=mode;if(mode==0)ctx.out=brute(A,B);else if(mode==2)ctx.out=tree_pairs(A,B);else if(mode==3){if(uint64_t(na)*nb<=100000)ctx.out=brute(A,B);else ctx.out=ctx.old.query(A,B,0,skin,s);*hit=s.hit;}else if(mode==4){ctx.out=ctx.policy.query(A,B,skin);*path=ctx.policy.path;*hit=ctx.policy.hit;}else if(mode==5)ctx.out=rtree_pairs(A,B);else if(mode==6){ctx.out=ctx.bounded.query(A,B,skin);*path=ctx.bounded.path;*hit=ctx.bounded.hit;}
#ifdef KYOTO_BULLET
else if(mode==7)ctx.out=ctx.bullet.query(A,B);
#endif
else return -1;*n=ctx.out.size();*out=reinterpret_cast<const unsigned*>(ctx.out.data());return 0;}catch(...){return -1;}}

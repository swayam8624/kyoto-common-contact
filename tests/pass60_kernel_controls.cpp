// Directly test the production shared CPU/Metal arithmetic on the host.
// Does not execute Metal. Randomized and boundary controls with independent
// interval-overlap oracle; compare conservative mask bits before postfilter.
#include "kyoto/tiled_gpu_mask.hpp"
#include <array>
#include <bit>
#include <random>
#include <iostream>
namespace g=kyoto::contact::gpu_mask;
namespace t=kyoto::contact::gpu_tiles;
using g::Box;using g::Params;using g::bound;using g::packed_word;using t::Job;
static void need(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct DBox {std::array<double,3> lo,hi;};
static bool exact(const DBox& a,const DBox& b){for(int i=0;i<3;++i)if(std::max(a.lo[i],b.lo[i])>std::min(a.hi[i],b.hi[i]))return false;return true;}
static bool rounded(const Box& a,const Box& b){for(int i=0;i<3;++i)if(std::max(a.lo[i],b.lo[i])>std::min(a.hi[i],b.hi[i]))return false;return true;}
int main(){try{
    std::mt19937_64 rng(0x4B594F544F60ULL);std::uniform_real_distribution<double> u(-2.,2.);
    std::uint64_t enclosures=0,rejected=0,packed_words=0,tile_words=0,exact_hits=0,conservative_extras=0;
    auto check=[&](double x){for(bool up:{false,true}){try{const float y=bound(x,up);need(std::isfinite(x)&&std::isfinite(y)&&(up?double(y)>=x:double(y)<=x),"inward rounding");need(y==0||std::abs(y)>=std::numeric_limits<float>::min(),"subnormal output");++enclosures;}catch(const std::overflow_error&){need(!std::isfinite(x)||std::abs(x)>double(std::numeric_limits<float>::max()),"unexpected overflow");++rejected;}}};
    for(double x:{0.,-0.,std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),1e-40,-1e-40,16777216.25,-16777216.25,double(std::numeric_limits<float>::max()),-double(std::numeric_limits<float>::max()),std::numeric_limits<double>::max(),-std::numeric_limits<double>::max(),INFINITY+0.,-INFINITY+0.,NAN+0.})check(x);
    for(int i=0;i<100000;++i)check(std::bit_cast<double>(rng()));
    for(double scale:{1.,1e-40,1e-20,1e20,1e30,16777216.})for(unsigned n:{1u,2u,31u,32u,33u,63u,64u,65u,97u}){
        std::vector<DBox> d(n);std::vector<Box> b(n);
        for(unsigned i=0;i<n;++i)for(int k=0;k<3;++k){double x=u(rng)*scale,y=x+std::abs(u(rng))*scale*.3;d[i].lo[k]=x;d[i].hi[k]=y;b[i].lo[k]=bound(x,false);b[i].hi[k]=bound(y,true);}
        if(n>1){d[1]=d[0];d[1].lo[0]=d[0].hi[0];d[1].hi[0]=d[0].hi[0];b[1]=b[0];b[1].lo[0]=bound(d[1].lo[0],false);b[1].hi[0]=bound(d[1].hi[0],true);}
        for(unsigned self:{0u,1u})for(unsigned chunk:{1u,31u,32u,33u,257u,262144u})for(unsigned start=0;start<n*n;start+=chunk){Params p{start,std::min(chunk,n*n-start),n,self};unsigned words=(p.count+31)/32;
            for(unsigned w=0;w<=words;++w){std::uint32_t expected=0;for(unsigned lane=0;lane<32;++lane){unsigned off=w*32+lane;if(off>=p.count)continue;unsigned k=p.first+off,i=k/n,j=k%n;if(self&&i>=j)continue;const bool hit=rounded(b[i],b[j]),truth=exact(d[i],d[j]);need(!truth||hit,"packed inward miss");if(hit)expected|=1u<<lane;exact_hits+=truth;conservative_extras+=hit&&!truth;}
                need(packed_word(b.data(),b.data(),p,w)==expected,"packed mask or padding mismatch");++packed_words;}
        }
        std::vector<Job> jobs;for(unsigned i=0;i<n;i+=32)for(unsigned j=i;j<n;j+=32)jobs.push_back({i,j,std::min(32u,n-i),std::min(32u,n-j),unsigned(i==j)});
        for(unsigned w=0;w<=jobs.size()*32;++w){std::uint32_t expected=0;unsigned k=w/32,row=w%32;if(k<jobs.size()&&row<jobs[k].nleft){auto j=jobs[k];for(unsigned col=0;col<j.nright;++col){if(j.diagonal&&row>=col)continue;const bool hit=rounded(b[j.left+row],b[j.right+col]),truth=exact(d[j.left+row],d[j.right+col]);need(!truth||hit,"tiled inward miss");if(hit)expected|=1u<<col;}}need(t::word(b.data(),b.data(),jobs.data(),std::uint32_t(jobs.size()),w)==expected,"tile mask, padding or diagonal mismatch");++tile_words;}
    }
    need(enclosures&&rejected&&packed_words&&tile_words&&exact_hits&&conservative_extras,"vacuous controls");
    std::cout<<"{\"enclosures\":"<<enclosures<<",\"range_rejections\":"<<rejected<<",\"packed_words\":"<<packed_words<<",\"tiled_words\":"<<tile_words<<",\"exact_hits\":"<<exact_hits<<",\"conservative_extras\":"<<conservative_extras<<",\"mismatches\":0}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

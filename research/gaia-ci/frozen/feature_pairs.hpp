#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>
namespace kyoto {
using V=std::array<double,3>;
struct Box { V lo,hi; };
using Pair=std::array<std::uint32_t,2>;
using Pairs=std::vector<Pair>;
inline bool overlap(const Box&a,const Box&b){for(int k=0;k<3;++k)if(a.hi[k]<b.lo[k]||b.hi[k]<a.lo[k])return false;return true;}
inline bool contains(const Box&a,const Box&b){for(int k=0;k<3;++k)if(a.lo[k]>b.lo[k]||a.hi[k]<b.hi[k])return false;return true;}
inline void validate(const std::vector<Box>&v){if(v.size()>std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("too many boxes");for(auto&b:v)for(int k=0;k<3;++k)if(!std::isfinite(b.lo[k])||!std::isfinite(b.hi[k])||b.lo[k]>b.hi[k])throw std::invalid_argument("invalid box");}
inline std::vector<Box> expand(std::vector<Box> v,double r){if(!std::isfinite(r)||r<0)throw std::invalid_argument("invalid radius");for(auto&b:v)for(int k=0;k<3;++k){b.lo[k]-=r;b.hi[k]+=r;}validate(v);return v;}
inline Pairs brute(const std::vector<Box>&a,const std::vector<Box>&b){Pairs out;for(std::uint32_t i=0;i<a.size();++i)for(std::uint32_t j=0;j<b.size();++j)if(overlap(a[i],b[j]))out.push_back({i,j});return out;}
struct Event{double x;std::uint32_t i;bool side,end;};
inline std::vector<Event> events(const std::vector<Box>&a,const std::vector<Box>&b,int axis){std::vector<Event> e;e.reserve(2*(a.size()+b.size()));for(int side=0;side<2;++side){auto&v=side?b:a;for(std::uint32_t i=0;i<v.size();++i){e.push_back({v[i].lo[axis],i,bool(side),false});e.push_back({v[i].hi[axis],i,bool(side),true});}}// Starts precede ends at equal coordinates, preserving closed contact.
std::sort(e.begin(),e.end(),[](auto&p,auto&q){if(p.x!=q.x)return p.x<q.x;if(p.end!=q.end)return p.end<q.end;if(p.side!=q.side)return p.side<q.side;return p.i<q.i;});return e;}
inline std::uint64_t projected(const std::vector<Event>&e){std::uint64_t n[2]{},c=0;for(auto&v:e){if(v.end)--n[v.side];else{c+=n[!v.side];++n[v.side];}}return c;}
struct Stats{std::uint64_t projected=0, retained=0;int axis=-1;bool hit=false;};
inline Pairs sweep(const std::vector<Box>&a,const std::vector<Box>&b,Stats&stats){auto e=events(a,b,0);auto best=projected(e);int axis=0;// Avoid two extra sorts if projected work already linear.
if(best>2ull*(a.size()+b.size()))for(int k=1;k<3;++k){auto f=events(a,b,k);auto c=projected(f);if(c<best){best=c;axis=k;e=std::move(f);}}
std::set<std::uint32_t> active[2];Pairs out;for(auto&v:e){auto&own=active[v.side];if(v.end){own.erase(v.i);continue;}for(auto j:active[!v.side]){auto i0=v.side?j:v.i,i1=v.side?v.i:j;if(overlap(a[i0],b[i1]))out.push_back({i0,i1});}own.insert(v.i);}std::sort(out.begin(),out.end());stats={best,out.size(),axis,false};return out;}
class Cache{std::vector<Box> ea_,eb_;Pairs pairs_;bool ready_=false;public:
void reset(){ready_=false;ea_.clear();eb_.clear();pairs_.clear();}
Pairs query(const std::vector<Box>&a,const std::vector<Box>&b,double radius,double skin,Stats&stats,bool force=false){validate(a);validate(b);if(!std::isfinite(skin)||skin<0)throw std::invalid_argument("invalid skin");auto aa=expand(a,radius);bool reuse=ready_&&!force&&ea_.size()==aa.size()&&eb_.size()==b.size();if(reuse){for(std::size_t i=0;i<aa.size()&&reuse;++i)reuse=contains(ea_[i],aa[i]);for(std::size_t i=0;i<b.size()&&reuse;++i)reuse=contains(eb_[i],b[i]);}
// Reuse only if the CURRENT represented boxes lie within their stored envelopes.
// This avoids relying on a rounded displacement estimate for completeness.
if(!reuse){auto ea=expand(aa,skin*.5),eb=expand(b,skin*.5);auto pairs=sweep(ea,eb,stats);ea_=std::move(ea);eb_=std::move(eb);pairs_=std::move(pairs);ready_=true;}else stats={0,0,-1,true};Pairs out;for(auto p:pairs_)if(overlap(aa[p[0]],b[p[1]]))out.push_back(p);stats.retained=pairs_.size();return out;}
};
}

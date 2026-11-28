#pragma once
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
namespace bg=boost::geometry;
namespace bgi=boost::geometry::index;
using Point=bg::model::point<double,3,bg::cs::cartesian>;
using RBox=bg::model::box<Point>;
using Entry=std::pair<RBox,unsigned>;
static RBox rbox(const Box& b){return {Point(b.lo[0],b.lo[1],b.lo[2]),Point(b.hi[0],b.hi[1],b.hi[2])};}
static Pairs rtree_pairs(const std::vector<Box>&a,const std::vector<Box>&b){
 std::vector<Entry> entries; entries.reserve(b.size());
 for(unsigned j=0;j<b.size();++j)entries.emplace_back(rbox(b[j]),j);
 bgi::rtree<Entry,bgi::rstar<16>> tree(entries.begin(),entries.end());
 Pairs out;
 for(unsigned i=0;i<a.size();++i){for(auto it=tree.qbegin(bgi::intersects(rbox(a[i])));it!=tree.qend();++it){if(overlap(a[i],b[it->second]))out.push_back({i,it->second});}}
 std::sort(out.begin(),out.end());return out;
}
// Limit envelope candidate materialization before it grows beyond the budget.
static bool bounded_visit(const Tree&t,int n,const Box&a,unsigned i,Pairs&out,size_t cap){
 const auto& node=t.nodes[n];if(!overlap(node.box,a))return true;
 if(node.left<0){for(size_t k=node.lo;k<node.hi;++k){auto j=t.ids[k];if(overlap(a,t.b[j])){if(out.size()==cap)return false;out.push_back({i,j});}}return true;}
 return bounded_visit(t,node.left,a,i,out,cap)&&bounded_visit(t,node.right,a,i,out,cap);
}
struct BoundedPolicy{
 std::vector<Box> ea,eb;Pairs cached;bool ready=false;unsigned path=0;bool hit=false;
 Pairs query(const std::vector<Box>&a,const std::vector<Box>&b,double skin){
  hit=false;auto p=uint64_t(a.size())*b.size();if(p<=100000){path=0;return brute(a,b);}
  uint64_t state=0xa17f129b;unsigned count=0;for(unsigned k=0;k<256;++k){state=state*6364136223846793005ULL+1;auto i=(state>>32)%a.size();state=state*6364136223846793005ULL+1;auto j=(state>>32)%b.size();count+=overlap(a[i],b[j]);}
  if(count>=16){path=0;ready=false;Pairs{}.swap(cached);return brute(a,b);}
  bool reuse=ready&&ea.size()==a.size()&&eb.size()==b.size();for(size_t i=0;i<a.size()&&reuse;++i)reuse=contains(ea[i],a[i]);for(size_t i=0;i<b.size()&&reuse;++i)reuse=contains(eb[i],b[i]);
  if(!reuse){ready=false;Pairs{}.swap(cached);try{ea=expand(a,skin*.5);eb=expand(b,skin*.5);}catch(const std::invalid_argument&){path=5;return tree_pairs(a,b);}
   const size_t cap=static_cast<size_t>(std::min<uint64_t>(p/8,1048576));Tree t(eb);bool ok=true;for(unsigned i=0;i<ea.size()&&ok;++i)ok=bounded_visit(t,0,ea[i],i,cached,cap);
   if(!ok){Pairs{}.swap(cached);path=5;return tree_pairs(a,b);}std::sort(cached.begin(),cached.end());ready=true;
  }
  hit=reuse;path=reuse?3:2;Pairs out;for(auto ij:cached)if(overlap(a[ij[0]],b[ij[1]]))out.push_back(ij);return out;
 }
};

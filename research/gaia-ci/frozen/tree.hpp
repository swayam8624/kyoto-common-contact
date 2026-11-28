// Independent median-split AABB tree baseline; rebuilt each query.
#include "feature_pairs.hpp"
#include <numeric>
using namespace kyoto;
struct Node{Box box; int left=-1,right=-1; size_t lo,hi;};
struct Tree{
 const std::vector<Box>& b; std::vector<unsigned> ids; std::vector<Node> nodes;
 Tree(const std::vector<Box>& boxes):b(boxes),ids(b.size()){std::iota(ids.begin(),ids.end(),0);if(!ids.empty())build(0,ids.size());}
 int build(size_t l,size_t r){Box e=b[ids[l]];for(size_t i=l+1;i<r;++i)for(int k=0;k<3;++k){e.lo[k]=std::min(e.lo[k],b[ids[i]].lo[k]);e.hi[k]=std::max(e.hi[k],b[ids[i]].hi[k]);}int n=nodes.size();nodes.push_back({e,-1,-1,l,r});if(r-l>8){int axis=0;for(int k=1;k<3;++k)if(e.hi[k]-e.lo[k]>e.hi[axis]-e.lo[axis])axis=k;size_t m=(l+r)/2;std::nth_element(ids.begin()+l,ids.begin()+m,ids.begin()+r,[&](unsigned i,unsigned j){double a=std::midpoint(b[i].lo[axis],b[i].hi[axis]),c=std::midpoint(b[j].lo[axis],b[j].hi[axis]);return a==c?i<j:a<c;});int x=build(l,m),y=build(m,r);nodes[n].left=x;nodes[n].right=y;}return n;}
 void visit(int n,const Box&a,unsigned i,Pairs&out){const auto& t=nodes[n];if(!overlap(t.box,a))return;if(t.left<0){for(size_t k=t.lo;k<t.hi;++k)if(overlap(a,b[ids[k]]))out.push_back({i,ids[k]});}else{visit(t.left,a,i,out);visit(t.right,a,i,out);}}
};

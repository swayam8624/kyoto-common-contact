#include "kyoto/exact_swept_tet_certificate.hpp"
#include <iostream>
#include <random>
using namespace kyoto::geometry;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static Tetrahedron move(Tetrahedron t,Point d){for(auto& p:t)for(int i=0;i<3;++i)p[i]+=d[i];return t;}
int main(){
    const Tetrahedron t{{{0,0,0},{1,0,0},{0,1,0},{0,0,1}}};
    const ExactTetIds a_ids{0,1,2,3}, b_ids{4,5,6,7};
    const ExactLinearTetMotion a{t,t,a_ids};
    auto check=[&](const ExactLinearTetMotion& b,bool expected){
        const auto r=certify_exact_swept_tet_pair(a,b,3);
        need(r.certified==expected,"analytic swept pair mismatch");
        need(certify_exact_swept_tet_pair(b,a,3).certified==expected,"swapped swept pair mismatch");
    };
    check({move(t,{2,0,0}),move(t,{3,0,0}),b_ids},true);
    check({move(t,{2,0,0}),move(t,{-2,0,0}),b_ids},false);
    check({move(t,{.5,.5,.5}),move(t,{.75,.5,.5}),b_ids},true);
    const Tetrahedron face{{{0,0,0},{1,0,0},{0,1,0},{0,0,-1}}};
    auto face_end=face;face_end[3]={.25,.25,-.5};
    check({face,face_end,{0,1,2,4}},true);
    face_end[3]={.25,.25,.5};check({face,face_end,{0,1,2,4}},false);
    const Tetrahedron edge{{{0,0,0},{1,0,0},{0,-1,0},{0,0,-1}}};
    auto edge_end=edge;edge_end[2]={0,-.5,-.25};
    check({edge,edge_end,{0,1,4,5}},true);
    const Tetrahedron vertex{{{0,0,0},{-1,0,0},{0,-1,0},{0,0,-1}}};
    auto vertex_end=vertex;vertex_end[1]={-1,-.5,0};
    check({vertex,vertex_end,{0,4,5,6}},true);
    // Moving common face, with a time-varying normal; entire mesh shears.
    auto shear=[](Tetrahedron s){for(auto& p:s)p[2]+=.75*p[0]+.5*p[1];return s;};
    need(certify_exact_swept_tet_pair({t,shear(t),a_ids},{face,shear(face),{0,1,2,4}}).certified,"moving face plane failed");
    // Exact dyadic translated simplex: collision iff x range intersects
    // [-1+negative_yz,1-positive_yz] and both yz sums are <=1.
    std::mt19937_64 rng(3802028);std::uniform_int_distribution<int> d(-32,32);
    std::size_t separated=0,contact=0,unknown=0,certified=0;
    for(int i=0;i<1000;++i){
        const int x0=d(rng),x1=d(rng),y=d(rng),z=d(rng);
        const int pos=std::max(y,0)+std::max(z,0),neg=std::max(-y,0)+std::max(-z,0);
        const bool hit=pos<=16&&neg<=16&&std::max(x0,x1)>=-16+neg&&std::min(x0,x1)<=16-pos;
        const ExactLinearTetMotion b{move(t,{x0/16.,y/16.,z/16.}),move(t,{x1/16.,y/16.,z/16.}),b_ids};
        const auto r=certify_exact_swept_tet_pair(a,b,3);
        need(!(hit&&r.certified),"false continuous safety certification");
        if(hit)++contact;else{++separated;unknown+=!r.certified;certified+=r.certified;}
    }
    need(contact>0&&certified>0,"vacuous campaign");
    // Co-moving exact scale transformation preserves shared geometry.
    for(int exp:{-500,-20,0,20,500}){
        auto x=t,y=face;
        for(auto* q:{&x,&y})for(auto& p:*q)for(auto& v:p)v=std::ldexp(v,exp);
        need(certify_exact_swept_tet_pair({x,x,a_ids},{y,y,{0,1,2,4}}).certified,"scaled shared face failed");
    }
    std::cout<<"PASS exact swept tetrahedra: separated="<<separated<<" intersecting="<<contact
             <<" certified="<<certified<<" conservative_unknown="<<unknown<<" false_clear=0\n";
}

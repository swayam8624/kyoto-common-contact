#pragma once
// Conditional-completeness exact continuous separation reference for linear
// tetrahedral motion. Integer Bernstein signs prove a moving supporting plane
// on each closed time slab. Unknown is never a collision or safety verdict.
// The caller separately checks continuous nondegeneracy/orientation of cells.
#include "kyoto/exact_tet_intersection.hpp"
#include <optional>

namespace kyoto::geometry {
struct ExactLinearTetMotion { Tetrahedron start,end; ExactTetIds ids; };
struct ExactSweptTetCertificate {
    bool certified{};
    std::size_t nodes{},axes{},polynomials{};
    unsigned max_depth{};
};
namespace swept_tet_detail {
using namespace exact_tet_detail;
struct Line { Q a,b; };
using Motion=std::array<Line,4>;
using Coeff=std::array<Z,4>;
inline Line difference(const Line& x,const Line& y){return {sub(x.a,y.a),sub(x.b,y.b)};}
inline Q sum(const Q& x,const Q& y){return {x[0]+y[0],x[1]+y[1],x[2]+y[2]};}
inline Line plus(const Line& x,const Line& y){return {sum(x.a,y.a),sum(x.b,y.b)};}
inline Coeff det_coeff(const Line& x,const Line& y,const Line& z){
    auto d=[](const Q& a,const Q& b,const Q& c){return dot(a,cross(b,c));};
    return {3*d(x.a,y.a,z.a),
        d(x.b,y.a,z.a)+d(x.a,y.b,z.a)+d(x.a,y.a,z.b),
        d(x.a,y.b,z.b)+d(x.b,y.a,z.b)+d(x.b,y.b,z.a),
        3*d(x.b,y.b,z.b)};
}
// Quadratic Bernstein normal, with the middle coefficient doubled.
// Reuse these cross-product coefficients for each exact projection.
struct AxisCoefficients {
    Q n0,n1,n2;
    AxisCoefficients(const Line& e,const Line& f):
        n0(cross(e.a,f.a)),n1(sum(cross(e.b,f.a),cross(e.a,f.b))),n2(cross(e.b,f.b)){}
    Coeff project(const Line& z)const{
        return {3*dot(n0,z.a),dot(n0,z.b)+dot(n1,z.a),
                dot(n1,z.b)+dot(n2,z.a),3*dot(n2,z.b)};
    }
};
inline bool signed_coeff(const Coeff& c,int sign,bool strict){
    for(const auto& v:c){
        if(sign>0?(strict?v<=0:v<0):(strict?v>=0:v>0))return false;
    }
    return true;
}
inline bool zero(const Coeff& c){return std::all_of(c.begin(),c.end(),[](const Z& x){return x==0;});}
inline Q integer_point(Point p,int exponent){
    Q out;
    for(int i=0;i<3;++i){
        const auto d=decode(p[i]);Z v=0;
        if(d.mantissa){
            const auto shift=unsigned(d.exponent-exponent);
            for(unsigned bit=0;bit<53;++bit)
                if((d.mantissa>>bit)&1u)boost::multiprecision::bit_set(v,shift+bit);
        }
        out[i]=d.negative?-v:v;
    }
    return out;
}
inline Motion scaled_motion(const ExactLinearTetMotion& m,int exp){
    Motion x;for(int i=0;i<4;++i)x[i]={integer_point(m.start[i],exp),integer_point(m.end[i],exp)};
    return x;
}
inline Motion half(const Motion& m,bool right){
    Motion out;
    // Both halves use a grid with half the spacing: 2*endpoint and sum of
    // endpoints denote exactly the original endpoint and midpoint.
    for(int i=0;i<4;++i){
        const Q mid=sum(m[i].a,m[i].b);
        out[i]=right?Line{mid,sum(m[i].b,m[i].b)}:Line{sum(m[i].a,m[i].a),mid};
    }
    return out;
}
} // namespace swept_tet_detail

inline ExactSweptTetCertificate certify_exact_swept_tet_pair(
    const ExactLinearTetMotion& a,const ExactLinearTetMotion& b,unsigned depth_limit=6,bool factored=false){
    using namespace swept_tet_detail;
    if(depth_limit>12)throw std::invalid_argument("exact swept tet depth must be <=12");
    unique_ids(a.ids);unique_ids(b.ids);
    std::array<bool,4> shared_a{},shared_b{};int anchor=-1,common=0;
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)if(a.ids[i]==b.ids[j]){
        if(a.start[i]!=b.start[j]||a.end[i]!=b.end[j])
            throw std::invalid_argument("inconsistent shared trajectory");
        shared_a[i]=shared_b[j]=true;anchor=i;++common;
    }
    if(common==4)throw std::invalid_argument("duplicate swept tet");
    int exp=0;
    for(const auto* m:{&a,&b})for(const auto* t:{&m->start,&m->end})for(auto p:*t)for(double v:p){
        const auto d=decode(v);if(d.mantissa)exp=std::min(exp,d.exponent);
    }
    const auto x=scaled_motion(a,exp),y=scaled_motion(b,exp);
    ExactSweptTetCertificate receipt;
    auto recurse=[&](auto&& self,const Motion& u,const Motion& v,unsigned depth)->bool{
        ++receipt.nodes;receipt.max_depth=std::max(receipt.max_depth,depth);
        auto try_axis=[&](const Line& e,const Line& f){
            ++receipt.axes;
            std::optional<AxisCoefficients> normal;
            if(factored)normal.emplace(e,f);
            auto project=[&](const Line& z){return factored?normal->project(z):det_coeff(e,f,z);};
            if(!common){
                bool pos=true,neg=true;
                for(const auto& p:u)for(const auto& q:v){
                    ++receipt.polynomials;const auto c=project(difference(p,q));
                    pos=pos&&signed_coeff(c,1,true);neg=neg&&signed_coeff(c,-1,true);
                    if(!pos&&!neg)return false;
                }
                return pos||neg;
            }
            std::array<Coeff,4> ca,cb;
            for(int i=0;i<4;++i){
                ca[i]=project(difference(u[i],u[anchor]));
                cb[i]=project(difference(v[i],u[anchor]));
                receipt.polynomials+=2;
                if((shared_a[i]&&!zero(ca[i]))||(shared_b[i]&&!zero(cb[i])))return false;
            }
            for(int sign:{1,-1}){
                bool weak=true,strict_a=true,strict_b=true;
                for(int i=0;i<4;++i){
                    weak=weak&&signed_coeff(ca[i],sign,false)&&signed_coeff(cb[i],-sign,false);
                    if(!shared_a[i])strict_a=strict_a&&signed_coeff(ca[i],sign,true);
                    if(!shared_b[i])strict_b=strict_b&&signed_coeff(cb[i],-sign,true);
                }
                // Any intersection lies on the plane. If every nonshared
                // vertex on either side is strictly off it, the intersection
                // belongs to that side's shared simplex, and vice versa.
                if(weak&&(strict_a||strict_b))return true;
            }
            return false;
        };
        const Q ex{1,0,0},ey{0,1,0},ez{0,0,1};
        if(try_axis({ey,ey},{ez,ez})||try_axis({ez,ez},{ex,ex})||try_axis({ex,ex},{ey,ey}))return true;
        if(common){
            std::vector<Line> shared,other;
            for(int i=0;i<4;++i){
                if(shared_a[i]&&i!=anchor)shared.push_back(difference(u[i],u[anchor]));
                if(!shared_a[i])other.push_back(difference(u[i],u[anchor]));
                if(!shared_b[i])other.push_back(difference(v[i],u[anchor]));
            }
            if(common==3){if(try_axis(shared[0],shared[1]))return true;}
            else if(common==2){
                for(const auto& d:other)if(try_axis(shared[0],d))return true;
                for(std::size_t i=0;i<other.size();++i)for(std::size_t j=i+1;j<other.size();++j)
                    if(try_axis(shared[0],plus(other[i],other[j])))return true;
            }else{
                // Diagonal constant directions can strictly separate
                // opposite octants where a face normal leaves extra
                // vertices on both sides' support planes.
                for(int i:{-1,0,1})for(int j:{-1,0,1})for(int k:{-1,0,1}){
                    if(!i&&!j)continue;
                    const Q n{i,j,k},e{j,-i,0},f=cross(n,e);
                    if(try_axis({e,e},{f,f}))return true;
                }
                for(std::size_t i=0;i<other.size();++i)for(std::size_t j=i+1;j<other.size();++j)
                    if(try_axis(other[i],other[j]))return true;
                // At the exact midpoint, orient all cone rays towards
                // the same halfspace. Sum feasible extreme normal rays
                // to propose a constant interior direction. Exact
                // Bernstein checks still validate the entire slab.
                std::vector<Q> rays;
                for(int i=0;i<4;++i){
                    if(!shared_a[i]){const auto d=difference(u[i],u[anchor]);rays.push_back(sum(d.a,d.b));}
                    if(!shared_b[i]){const auto d=difference(u[anchor],v[i]);rays.push_back(sum(d.a,d.b));}
                }
                Q normal{0,0,0};
                for(std::size_t i=0;i<rays.size();++i)for(std::size_t j=i+1;j<rays.size();++j){
                    auto n=cross(rays[i],rays[j]);
                    bool pos=true,neg=true;
                    for(const auto& r:rays){const auto p=dot(n,r);pos&=p>=0;neg&=p<=0;}
                    if(pos&&!neg)normal=sum(normal,n);
                    else if(neg&&!pos)normal=sub(normal,n);
                }
                const Q e=normal[0]!=0||normal[1]!=0?Q{normal[1],-normal[0],0}:Q{1,0,0};
                const Q f=cross(normal,e);
                if(try_axis({e,e},{f,f}))return true;
            }
        }else{
            constexpr int faces[4][3]={{0,1,2},{0,1,3},{0,2,3},{1,2,3}};
            for(const auto* m:{&u,&v})for(const auto& f:faces)
                if(try_axis(difference((*m)[f[1]],(*m)[f[0]]),difference((*m)[f[2]],(*m)[f[0]])))return true;
            for(int i=0;i<4;++i)for(int j=i+1;j<4;++j)for(int k=0;k<4;++k)for(int l=k+1;l<4;++l)
                if(try_axis(difference(u[j],u[i]),difference(v[l],v[k])))return true;
        }
        if(depth==depth_limit)return false;
        return self(self,half(u,false),half(v,false),depth+1)&&self(self,half(u,true),half(v,true),depth+1);
    };
    receipt.certified=recurse(recurse,x,y,0);return receipt;
}
} // namespace kyoto::geometry

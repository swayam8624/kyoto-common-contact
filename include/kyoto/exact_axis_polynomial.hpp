#pragma once
// Exact quadratic normal and scaled cubic projection. The optional
// constant-normal route is an algebraic identity, not an approximation.
#include <array>
namespace kyoto::geometry::exact_axis {
template<class Z,class Line> struct AxisPolynomial {
    using Q=std::array<Z,3>;
    using Coeff=std::array<Z,4>;
    Q n0{},n1{},n2{};
    bool constant{};
    static Q cross(const Q& a,const Q& b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
    static Q sum(const Q& a,const Q& b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
    static Z dot(const Q& a,const Q& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
    AxisPolynomial(const Line& e,const Line& f,bool fast=false):n0(cross(e.a,f.a)){
        // Constant input directions need only one cross product.
        if(fast&&e.a==e.b&&f.a==f.b){
            n1=sum(n0,n0);n2=n0;constant=true;return;
        }
        n1=sum(cross(e.b,f.a),cross(e.a,f.b));n2=cross(e.b,f.b);
        // A moving input pair can nevertheless have a constant normal.
        constant=fast&&n0==n2&&n1==sum(n0,n0);
    }
    Coeff project(const Line& z)const{
        if(constant){
            const Z a=dot(n0,z.a),b=dot(n0,z.b);
            return {3*a,2*a+b,a+2*b,3*b};
        }
        return {3*dot(n0,z.a),dot(n0,z.b)+dot(n1,z.a),
                dot(n1,z.b)+dot(n2,z.a),3*dot(n2,z.b)};
    }
};
} // namespace kyoto::geometry::exact_axis

#pragma once
// Headless-testable 3D picking geometry, shared by the macOS GLFW viewer.
// Cursor positions are GLFW WINDOW coordinates (top-left origin); do not mix
// them with retina framebuffer pixels when constructing a camera ray.
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>
#include <stdexcept>
namespace ccp::interaction {
using Vec3=ccp::live::Vec3;
inline Vec3 plus(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 minus(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 times(Vec3 a,double k){return {a.x*k,a.y*k,a.z*k};}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){
 return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline Vec3 unit(Vec3 a){
 const double n=std::sqrt(dot(a,a));
 if(!(n>1e-12)||!std::isfinite(n))throw std::invalid_argument("degenerate picking direction");
 return times(a,1./n);
}
struct Ray {Vec3 origin{},direction{};};
struct Hit {
 int triangle{-1};
 double distance{std::numeric_limits<double>::infinity()};
 Vec3 position{};
 std::array<double,3> barycentric{};
};
// Consistent with the native viewer's perspective(.70f,...), look(eye,target,z).
inline Ray cameraRay(double pixelX,double pixelY,double viewportWidth,double viewportHeight,
                     Vec3 eye,Vec3 target,double verticalFovRadians=.70) {
 if(!(std::isfinite(pixelX)&&std::isfinite(pixelY)&&std::isfinite(viewportWidth)&&
      std::isfinite(viewportHeight)&&std::isfinite(verticalFovRadians)&&
      viewportWidth>0&&viewportHeight>0&&verticalFovRadians>0&&verticalFovRadians<3.0&&
      std::isfinite(eye.x)&&std::isfinite(eye.y)&&std::isfinite(eye.z)&&
      std::isfinite(target.x)&&std::isfinite(target.y)&&std::isfinite(target.z)))
  throw std::invalid_argument("invalid perspective viewport");
 const Vec3 forward=unit(minus(target,eye));
 const Vec3 worldUp=std::abs(dot(forward,{0,0,1}))>.999
   ? Vec3{0,1,0}:Vec3{0,0,1};
 const Vec3 right=unit(cross(forward,worldUp));
 const Vec3 up=cross(right,forward);
 const double half=std::tan(verticalFovRadians*.5);
 const double nx=(2.*pixelX/viewportWidth-1.)*half*viewportWidth/viewportHeight;
 const double ny=(1.-2.*pixelY/viewportHeight)*half;
 return {eye,unit(plus(forward,plus(times(right,nx),times(up,ny))))};
}
// Moller-Trumbore, two-sided. Front and back cloth faces are selectable.
// The nearest positive intersection wins so a back sheet cannot be grabbed
// accidentally through an overlapping front sheet.
inline std::optional<Hit> pickTriangleRay(const Ray& ray,
  const std::vector<Vec3>& positions,const std::vector<std::array<int,3>>& faces){
 std::optional<Hit> nearest;
 for(std::size_t i=0;i<faces.size();++i){
  const auto face=faces[i];
  const Vec3 a=positions.at(face[0]),b=positions.at(face[1]),c=positions.at(face[2]);
  const Vec3 ab=minus(b,a),ac=minus(c,a),p=cross(ray.direction,ac);
  const double determinant=dot(ab,p);
  if(std::abs(determinant)<1e-11)continue;
  const double inv=1./determinant;
  const Vec3 t=minus(ray.origin,a);
  const double u=dot(t,p)*inv;
  if(u<0||u>1)continue;
  const Vec3 q=cross(t,ab);
  const double v=dot(ray.direction,q)*inv;
  if(v<0||u+v>1)continue;
  const double distance=dot(ac,q)*inv;
  if(distance<=.001||(nearest&&distance>=nearest->distance))continue;
  nearest=Hit{static_cast<int>(i),distance,
              plus(ray.origin,times(ray.direction,distance)),{1.-u-v,u,v}};
 }
 return nearest;
}
// Keep the grab in a camera-facing plane through the original picked point,
// rather than projecting onto the floor (which makes vertical folds impossible).
inline std::optional<Vec3> intersectDragPlane(Ray ray,Vec3 point,Vec3 normal){
 if(!(std::isfinite(ray.origin.x)&&std::isfinite(ray.origin.y)&&std::isfinite(ray.origin.z)&&
      std::isfinite(ray.direction.x)&&std::isfinite(ray.direction.y)&&std::isfinite(ray.direction.z)&&
      std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z)&&
      std::isfinite(normal.x)&&std::isfinite(normal.y)&&std::isfinite(normal.z)))
  return std::nullopt;
 const double d=dot(ray.direction,normal);
 if(std::abs(d)<1e-8)return std::nullopt;
 const double distance=dot(minus(point,ray.origin),normal)/d;
 if(distance<=0||!std::isfinite(distance))return std::nullopt;
 return plus(ray.origin,times(ray.direction,distance));
}
}

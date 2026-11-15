#pragma once
// Certified broad-phase policy selection: small PyTorch-trained MLP inference
// with NO Python/PyTorch dependency inside the simulation loop.
//
// This model predicts the cost of four COMPLETELY SAFE broad-phase algorithms;
// it NEVER predicts which collisions may be omitted. Every selected algorithm
// remains subject to the existing exhaustive/SAP pair oracle.
// File format is versioned plain-text KYOTO_MLP_V1; loading fails closed.
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ccp::learned {
using Bounds=ccp::live::Bounds;
using Pair=ccp::live::Pair;
constexpr int kInputs=12;
constexpr int kActions=4;
// Fixed label order MUST match scripts/train_online_controller.py.
constexpr std::array<const char*,kActions> kActionNames={
 "sap_rebuild","global_guarded_exact",
 "selective_guard_exact","region_hierarchical"
};
using Features=std::array<double,kInputs>;

class Extractor {
 std::vector<Bounds> previous_;
 double previousPairsPerObject_{},priorPairsPerObject_{};
public:
 void reset(){previous_.clear();previousPairsPerObject_=priorPairsPerObject_=0;}
 Features describe(const std::vector<Bounds>& b)const{
  if(b.empty())throw std::invalid_argument("ML controller requires non-empty scene");
  const auto n=b.size();
  std::array<double,3> minimum={1e30,1e30,1e30},maximum={-1e30,-1e30,-1e30};
  std::array<double,3> extents{},centerMean{},centerSquare{};
  double sumVolume=0,displacementSum=0,displacementMax=0,numberMoving=0;
  for(const auto& a:b){
   const double lo[3]={a.min.x,a.min.y,a.min.z};
   const double hi[3]={a.max.x,a.max.y,a.max.z};
   for(int k=0;k<3;++k){
    if(!std::isfinite(lo[k])||!std::isfinite(hi[k])||lo[k]>hi[k])
     throw std::invalid_argument("ML controller received invalid AABB");
    minimum[k]=std::min(minimum[k],lo[k]);maximum[k]=std::max(maximum[k],hi[k]);
    extents[k]+=hi[k]-lo[k];
    const double midpoint=(lo[k]+hi[k])*.5;
    centerMean[k]+=midpoint;centerSquare[k]+=midpoint*midpoint;
   }
   sumVolume+=std::max(0.,hi[0]-lo[0])*
              std::max(0.,hi[1]-lo[1])*
              std::max(0.,hi[2]-lo[2]);
  }
  double ranges[3]{};
  for(int k=0;k<3;++k)ranges[k]=std::max(1e-7,maximum[k]-minimum[k]);
  const double diag=std::sqrt(ranges[0]*ranges[0]+ranges[1]*ranges[1]+ranges[2]*ranges[2]);
  const double invN=1./double(n);
  double variance=0,meanExtent=0;
  for(int k=0;k<3;++k){
   const double avg=centerMean[k]*invN;
   variance+=std::max(0.,centerSquare[k]*invN-avg*avg)/(ranges[k]*ranges[k]);
   meanExtent+=extents[k]*invN/diag;
  }
  if(previous_.size()==n){
   for(size_t i=0;i<n;++i){
    if(previous_[i].id!=b[i].id)
     throw std::invalid_argument("ML controller requires stable ordered primitive IDs");
    const auto center=[](const Bounds& a){
     return std::array<double,3>{(a.min.x+a.max.x)*.5,(a.min.y+a.max.y)*.5,
                                  (a.min.z+a.max.z)*.5};};
    const auto a=center(b[i]),p=center(previous_[i]);
    const double movement=std::sqrt((a[0]-p[0])*(a[0]-p[0])+
       (a[1]-p[1])*(a[1]-p[1])+(a[2]-p[2])*(a[2]-p[2]))/diag;
    displacementSum+=movement;
    displacementMax=std::max(displacementMax,movement);
    numberMoving+=(movement>0.002)?1.:0.;
   }
  }
  Features f={
   std::log1p(double(n)),
   meanExtent,
   ranges[0]/diag,ranges[1]/diag,ranges[2]/diag,
   std::log1p(std::min(1e12,sumVolume/std::max(1e-12,ranges[0]*ranges[1]*ranges[2]))),
   variance,
   displacementSum*invN,
   displacementMax,
   numberMoving*invN,
   std::log1p(previousPairsPerObject_),
   std::log1p(std::abs(previousPairsPerObject_-priorPairsPerObject_))
  };
  for(const auto value:f)if(!std::isfinite(value))
   throw std::runtime_error("non-finite ML feature");
  return f;
 }
 void observe(const std::vector<Bounds>& b,size_t pairCount){
  previous_=b;
  priorPairsPerObject_=previousPairsPerObject_;
  previousPairsPerObject_=double(pairCount)/double(std::max(size_t{1},b.size()));
 }
};

class Network {
 int width_{},depth_{};
 Features mean_{},scale_{};
 // PyTorch Linear weight row-major: [output][input], then biases.
 std::vector<std::vector<double>> weights_,biases_;
 static std::vector<double> read(std::istream& stream,size_t count){
  if(count>32768)throw std::invalid_argument("invalid ML tensor size");
  std::vector<double> out(count);
  for(double& value:out)
   if(!(stream>>value)||!std::isfinite(value))
    throw std::runtime_error("truncated/non-finite ML weights");
  return out;
 }
public:
 static Network load(const std::string& path){
  std::ifstream stream(path);
  if(!stream)throw std::runtime_error("cannot open ML model: "+path);
  std::string magic;stream>>magic;
  if(magic!="KYOTO_MLP_V1")throw std::invalid_argument("unsupported ML format/version");
  int inputs=0,outputs=0;
  Network net;
  if(!(stream>>inputs>>net.width_>>net.depth_>>outputs)||
      inputs!=kInputs||outputs!=kActions||net.depth_<0||net.depth_>2||
      net.width_<1||net.width_>64)
   throw std::invalid_argument("invalid ML architecture");
  const auto means=read(stream,kInputs),scales=read(stream,kInputs);
  for(int k=0;k<kInputs;++k){
   net.mean_[k]=means[k];net.scale_[k]=scales[k];
   if(scales[k]<1e-8||scales[k]>1e8)
    throw std::invalid_argument("ML feature standard deviation invalid");
  }
  int previous=kInputs;
  for(int layer=0;layer<=net.depth_;++layer){
   const int next=layer==net.depth_?kActions:net.width_;
   net.weights_.push_back(read(stream,size_t(previous)*size_t(next)));
   net.biases_.push_back(read(stream,size_t(next)));
   previous=next;
  }
  std::string trailing;
  if(stream>>trailing)throw std::invalid_argument("unexpected trailing ML model data");
  return net;
 }
 std::array<double,kActions> predict(const Features& f)const{
  std::vector<double> values(kInputs);
  for(int k=0;k<kInputs;++k)
   values[k]=std::clamp((f[k]-mean_[k])/scale_[k],-8.,8.);
  for(size_t layer=0;layer<weights_.size();++layer){
   const size_t rows=biases_[layer].size();
   std::vector<double> next(rows);
   for(size_t i=0;i<rows;++i){
    double result=biases_[layer][i];
    for(size_t j=0;j<values.size();++j)
     result+=weights_[layer][i*values.size()+j]*values[j];
    if(layer+1<weights_.size())result=std::max(0.,result);
    next[i]=result;
   }
   values=std::move(next);
  }
  std::array<double,kActions> milliseconds{};
  for(int i=0;i<kActions;++i){
   if(!std::isfinite(values[i]))throw std::runtime_error("invalid ML prediction");
   // Model predicts natural logarithm of observed broad-phase query ms.
   milliseconds[i]=std::exp(std::clamp(values[i],-16.,10.));
  }
  return milliseconds;
 }
};
} // namespace ccp::learned

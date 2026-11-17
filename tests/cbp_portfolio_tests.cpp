// Phase-4 conservative portfolio: exact-pair proof across safe V4 fallback,
// cold regional trials, rejected/accepted probes and changing geometry.
#include "ccp/cbp_portfolio.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::CbpPortfolio;
using ccp::cbp::CrossMode;
using ccp::cbp::PortfolioMode;
void require(bool c,const char* reason){if(!c)throw std::runtime_error(reason);}
Pairs oracle(const std::vector<Bounds>& v){
 Pairs p;
 for(std::size_t a=0;a<v.size();++a)
  for(std::size_t b=a+1;b<v.size();++b)
   if(ccp::cbp::compatible(v[a],v[b])&&ccp::cbp::overlap(v[a],v[b]))
    p.push_back({v[a].id,v[b].id});
 return p;
}
void make(std::vector<Bounds>& v,std::vector<int>& labels,int frame){
 const int stage=frame/40;
 for(std::size_t i=0;i<v.size();++i){
  double x=stage==0?double(i)*3.0:
   (stage==1?double(i%12)*.45:double(i%12)*.002);
  double y=stage==0?0:
   (stage==1?double(i/12)*.45:double(i/12)*.002);
  if(stage==1)x+=double(frame%9)*.005;
  if(stage==0)x+=double((i%5))*frame*.0006;
  const double extent=stage==2?1.5:.55;
  v[i]={static_cast<int>(i),{x,y,0},{x+extent,y+extent,.6},
        i%11==0?2u:1u,i%19==0?0u:3u,true};
  labels[i]=static_cast<int>(i/12);
 }
 // Explicit metadata and region-label changes break stale graph assumptions.
 if(frame==58)v[10].mask=0;
 if(frame==62)for(std::size_t i=0;i<labels.size();++i)
  labels[i]=static_cast<int>(i%7);
 if(frame==76)v[22].max.z=v[22].min.z;
}
void test(CrossMode candidate){
 constexpr int N=144;
 CbpPortfolio p;
 p.setTrialKernel(candidate);
 p.setSapThreshold(48);
 p.setCachePadding(.045);
 std::vector<Bounds> v(N);
 std::vector<int> labels(N);
 std::size_t v4=0,regional=0,switches=0,trials=0;
 for(int t=0;t<120;++t){
  make(v,labels,t);
  require(p.query(v,labels)==oracle(v),"Portfolio candidate mismatch");
  const auto d=p.decision();
  require(d.totalMs>=d.backendMs,"Portfolio total did not include backend");
  require(d.decisionMs>=0. && d.coldSwitchMs>=0.,"Invalid charged cost");
  require(d.switches>=switches,"Portfolio switch count regressed");
  require(d.trials>=trials,"Portfolio trial count regressed");
  switches=d.switches;trials=d.trials;
  if(d.mode==PortfolioMode::FrozenV4){
   ++v4;
   require(std::string(d.v4Action)=="global_guarded_exact",
           "Unexpected V4 backend below its 384-object trial threshold");
  }else ++regional;
 }
 require(v4>0 && regional>0,"Portfolio did not execute both V4 and regional trials");
 require(trials>0 && switches>0,"Portfolio failed to charge executed trials/switches");
 const auto last=p.decision();
 require(last.accepted+last.rejected<=last.trials,"Invalid portfolio verdicts");

 bool rejected=false;
 make(v,labels,0);
 v[3].min.x=std::numeric_limits<double>::quiet_NaN();
 try{(void)p.query(v,labels);}catch(const std::invalid_argument&){rejected=true;}
 require(rejected,"Portfolio accepted nonfinite input");
 make(v,labels,0);
 require(p.query(v,labels)==oracle(v),"Invalid input corrupted portfolio state");

 // Primitive-count changes also reset dormant Guard and regional structures.
 v.resize(N+16);labels.resize(N+16);
 make(v,labels,4);
 require(p.query(v,labels)==oracle(v),"Portfolio failed on primitive count migration");
 require(p.decision().frame==1,"Portfolio did not reset frame clock after topology change");
 std::cout<<"PASS portfolio candidate="<<CbpPortfolio::name(
   candidate==CrossMode::Exhaustive?PortfolioMode::RegionalExhaustive:
   candidate==CrossMode::SharedBVH?PortfolioMode::RegionalSharedBVH:
   PortfolioMode::RegionalCertifiedCache)
   <<" frames=120, probes="<<trials<<", switches="<<switches<<"\n";
}
}
int main(){
 test(CrossMode::Exhaustive);
 test(CrossMode::SharedBVH);
 test(CrossMode::CertifiedCache);
}

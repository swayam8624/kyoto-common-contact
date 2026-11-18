// Phase 10: exact pair coverage for all four causal on-policy portfolio arms.
// Does not assume trial promotions: a regression may legitimately reject all
// experimental backends if the random workload favors the frozen policy.
#include "ccp/cbp_portfolio_v2.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::CbpPortfolioV2;
using ccp::cbp::PortfolioV2Arm;
void require(bool value,const char* message){
 if(!value)throw std::runtime_error(message);
}
Pairs direct(const std::vector<Bounds>& boxes){
 Pairs pairs;
 for(std::size_t i=0;i<boxes.size();++i)
  for(std::size_t j=i+1;j<boxes.size();++j)
   if(ccp::cbp::compatible(boxes[i],boxes[j])&&
      ccp::cbp::overlap(boxes[i],boxes[j]))
    pairs.push_back({int(i),int(j)});
 return pairs;
}
void construct(std::vector<Bounds>& boxes,std::vector<int>& owners,int frame){
 for(std::size_t i=0;i<boxes.size();++i){
  const double group=double(i/8);
  const double motion=.08*std::sin(.13*frame+double(i%11));
  const double local=frame<45?2.5:frame<95?.11:.5;
  const double x=double(int(group)%7)*5.+
                  double(int(i)%2)*local+motion;
  const double y=double(int(group)%7)*3.+
                  ((i/2)%2)*local+.04*std::cos(.08*frame+i);
  const double z=double(int(group)/7)*4.+((i/4)%2)*local;
  boxes[i]={int(i),{x,y,z},{x+.6,y+.6,z+.6},
             i%13==0?2u:1u,i%19==0?0u:3u,true};
  owners[i]=int(i/8);
 }
}
void check(){
 constexpr int n=192;
 CbpPortfolioV2 p;
 p.setSapThreshold(16);
 std::vector<Bounds> bounds(n);
 std::vector<int> labels(n);
 std::array<std::size_t,4> calls{};
 std::size_t probes=0,switches=0,rejects=0,accepted=0;
 bool sawReturn=false;
 for(int frame=0;frame<150;++frame){
  construct(bounds,labels,frame);
  if(frame>=106){
   // Exactness of local ownership migration: noncontiguous labels remain legal.
   for(std::size_t i=0;i<labels.size();++i)labels[i]=int(i%17);
  }
  auto actual=p.query(bounds,labels);
  require(actual==direct(bounds),"Four-arm portfolio lost an exact candidate");
  const auto& d=p.decision();
  require(d.totalMs>=d.backendMs,"Controller undercharged its actual backend");
  require(d.trials>=probes&&d.switches>=switches,
          "Four-arm trajectory counters regressed without topology reset");
  if(frame==106){
   require(d.frame==1,"Topology ownership changes must reset policy statistics");
   probes=0;switches=0;
  }
  probes=d.trials;switches=d.switches;
  if(d.probe){
   ++calls[static_cast<std::size_t>(d.mode)];
  }
  if(d.returning)sawReturn=true;
  if(d.rejected)++rejects;
  if(d.promoted)++accepted;
  require(d.promotions+d.rejections<=d.trials,
          "Four-arm trial completed without a corresponding counted execution");
  require(std::isfinite(d.totalMs)&&std::isfinite(d.championEwmaMs),
          "Invalid cost telemetry in on-policy portfolio");
 }
 require(probes>0,"New portfolio never conducted an actual trial");
 require(calls[static_cast<std::size_t>(PortfolioV2Arm::SharedBVH)]>0,
         "New portfolio could not run SharedBVH, reproducing Phase 9 error");
 // Promotion/rejection counts are machine-load-dependent, so NOT asserted.
 require(sawReturn||accepted>0||rejects==0,
         "Manager reported rejected probe without reverting to champion");

 construct(bounds,labels,10);
 bounds[5].min.x=std::numeric_limits<double>::quiet_NaN();
 bool invalid=false;
 try{(void)p.query(bounds,labels);}catch(const std::invalid_argument&){invalid=true;}
 require(invalid,"Invalid input was not rejected");
 construct(bounds,labels,11);
 require(p.query(bounds,labels)==direct(bounds),
         "Failed invalid-input recovery");

 std::cout<<"PASS exact four-arm causal portfolio; tested SharedBVH; "
          <<"completed trials="<<probes<<" switches="<<switches
          <<" accepted="<<accepted<<" rejected="<<rejects<<"\n";
}
}
int main(){check();}

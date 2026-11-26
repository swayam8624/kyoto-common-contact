// Pass 39B closeout: all three production pair modes, held-out dyadic
// trajectories and an independent exact static sampled falsifier.
#include "kyoto/exact_swept_tet_certificate.hpp"
#include <chrono>
#include <iostream>
#include <random>
using namespace kyoto::geometry;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static bool same(const ExactSweptTetCertificate& a,const ExactSweptTetCertificate& b){
    return a.certified==b.certified&&a.nodes==b.nodes&&a.axes==b.axes&&a.polynomials==b.polynomials&&a.max_depth==b.max_depth;
}
static Tetrahedron at(const ExactLinearTetMotion& m,int step){
    Tetrahedron t;
    for(int i=0;i<4;++i)for(int k=0;k<3;++k)t[i][k]=((8-step)*m.start[i][k]+step*m.end[i][k])/8.;
    return t;
}
struct Case{ExactLinearTetMotion a,b;};
int main(int argc,char** argv){
    std::mt19937_64 rng(3917129);std::uniform_int_distribution<int> d(-8,8);
    const Tetrahedron t{{{0,0,0},{1,0,0},{0,1,0},{0,0,1}}};
    std::array<std::vector<Case>,4> cases;
    std::array<std::size_t,4> clear{},unknown{},sample_hits{},oracle_checks{},degenerate{};
    for(unsigned shared=0;shared<4;++shared)for(int trial=0;trial<120;++trial){
        Case c{{t,t,{0,1,2,3}},{t,t,{4,5,6,7}}};
        if(shared==0)for(auto* p:{&c.b.start,&c.b.end})for(auto& v:*p)v[0]+=2.;
        if(shared==1)for(auto* p:{&c.b.start,&c.b.end})for(auto& v:*p)for(auto& x:v)x=-x;
        if(shared==2)for(auto* p:{&c.b.start,&c.b.end})for(auto& v:*p){v[1]=-v[1];v[2]=-v[2];}
        if(shared==3)for(auto* p:{&c.b.start,&c.b.end})for(auto& v:*p)v[2]=-v[2];
        for(unsigned i=0;i<shared;++i)c.b.ids[i]=c.a.ids[i];
        // Even: common shear/translation. Odd: independent perturbations.
        if(trial%2==0){
            const double sh=d(rng)/16.,tr=d(rng)/8.;
            for(auto* p:{&c.a.end,&c.b.end})for(auto& v:*p){v[0]+=sh*v[1]+tr;v[1]+=sh*v[2];}
        }else for(auto* p:{&c.a.end,&c.b.end})for(auto& v:*p)for(auto& x:v)x+=d(rng)/8.;
        for(unsigned i=0;i<shared;++i){c.b.start[i]=c.a.start[i];c.b.end[i]=c.a.end[i];}
        const int exp=trial%3==0?-500:trial%3==1?0:500;
        for(auto* m:{&c.a,&c.b})for(auto* p:{&m->start,&m->end})for(auto& v:*p)for(auto& x:v)x=std::ldexp(x,exp);
        for(auto* m:{&c.a,&c.b})for(int i=3;i>0;--i){const int j=int(rng()%unsigned(i+1));std::swap(m->start[i],m->start[j]);std::swap(m->end[i],m->end[j]);std::swap(m->ids[i],m->ids[j]);}
        const auto r=certify_exact_swept_tet_pair(c.a,c.b,2,false,false);
        const auto f=certify_exact_swept_tet_pair(c.a,c.b,2,true,false);
        const auto s=certify_exact_swept_tet_pair(c.a,c.b,2,true,true);
        need(same(r,f)&&same(r,s),"held-out complete receipt mismatch");
        const auto rev=certify_exact_swept_tet_pair(c.b,c.a,2,false,false);
        need(same(rev,certify_exact_swept_tet_pair(c.b,c.a,2,true,true)),"swapped receipt mismatch");
        clear[shared]+=r.certified;unknown[shared]+=!r.certified;
        bool hit=false;
        for(int step=0;step<=8;++step){
            try{const auto q=exact_static_tet_intersection(at(c.a,step),c.a.ids,at(c.b,step),c.b.ids);
                ++oracle_checks[shared];hit|=!q.matches_shared_simplex;
                need(!r.certified||q.matches_shared_simplex,"continuous certificate contradicts static exact oracle");
                need(!rev.certified||q.matches_shared_simplex,"swapped certificate contradicts static oracle");
            }catch(const std::invalid_argument&){++degenerate[shared];}
        }
        sample_hits[shared]+=hit;cases[shared].push_back(std::move(c));
    }
    for(unsigned k=0;k<4;++k){need(clear[k]>0&&oracle_checks[k]>0,"vacuous held-out stratum");
        std::cerr<<"shared="<<k<<" paths="<<cases[k].size()<<" certified="<<clear[k]<<" unknown="<<unknown[k]<<" sampled_hit_paths="<<sample_hits[k]<<" static_checks="<<oracle_checks[k]<<" degenerate="<<degenerate[k]<<'\n';}
    std::cerr<<"PASS held-out full pair parity and sampled exact falsifier\n";
    if(argc!=2||std::string(argv[1])!="--timing")return 0;
    std::cout<<"shared,block,slot,arm,paths,elapsed_ns,certified,axes,polynomials\n";
    using Clock=std::chrono::steady_clock;
    for(unsigned k=0;k<4;++k)for(int block=-1;block<3;++block)for(int slot=0;slot<6;++slot){
        const int arm=((slot<3?slot:5-slot)+block+3)%3;
        std::size_t certified=0,axes=0,polynomials=0;
        const auto begin=Clock::now();
        for(const auto& c:cases[k]){const auto r=certify_exact_swept_tet_pair(c.a,c.b,2,arm!=0,arm==2);
            certified+=r.certified;axes+=r.axes;polynomials+=r.polynomials;}
        const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();
        need(certified==clear[k]&&ns>0,"timed audit mismatch");
        if(block>=0)std::cout<<k<<','<<block<<','<<slot<<','<<(arm==0?"reference":arm==1?"factored":"constant_fast")<<','<<cases[k].size()<<','<<ns<<','<<certified<<','<<axes<<','<<polynomials<<'\n';
    }
}

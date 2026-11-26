// Frozen production arms. New seeded synthetic cubic trajectories are
// adversarial broad-phase inputs, not physically integrated trajectories.
#define main pass41_original_main
#include "pass41_common_campaign.cpp"
#undef main
#include <random>
static double u(std::mt19937_64& rng){return double(rng()>>11)*0x1p-53;}
static f::Mesh generate(std::uint64_t seed,int mode,int frame){
    std::mt19937_64 rng(seed+0x9e3779b97f4a7c15ULL*std::uint64_t(frame+1));
    f::Mesh m;const unsigned n=mode==2?unsigned(12+4*(frame%3)):16u;
    const double scale=mode==4?1e-40:1.;
    const double offset=mode==3?16777216.:0.;
    m.thickness=frame%2?scale*.001:0.;
    m.topology_epoch=mode==2?std::uint64_t(frame+1):1;
    for(unsigned i=0;i<n;++i){
        const double pitch=mode==0?3.:.15;
        const double cx=pitch*double(i%4)+.08*u(rng),cy=pitch*double((i/4)%4)+.08*u(rng),cz=.08*u(rng);
        const std::uint64_t base=m.vertices.size();
        for(unsigned j=0;j<3;++j){
            const double x=cx+(j==1?.25:0.),y=cy+(j==2?.25:0.),z=cz;
            kyoto::contact::CubicVertex path;
            for(int k=0;k<4;++k){
                const double amp=mode==0?.01:mode==2?1.5:.12;
                const double dx=k?amp*(2*u(rng)-1):0.,dy=k?amp*(2*u(rng)-1):0.,dz=k?amp*(2*u(rng)-1):0.;
                const double sign=(mode==2&&frame%2)?-1.:1.;
                path.control[k]={offset+scale*sign*(x+dx),scale*(y+dy),scale*(z+dz)};
            }
            f::Vertex v{base+j,path,i};
            if(mode==1){v.category=1ULL<<(i%3);v.mask=(frame%2)?7ULL:3ULL;}
            m.vertices.push_back(v);
        }
        f::Face face{m.faces.size(),{base,base+1,base+2}};
        if(mode==1){face.category=1ULL<<(i%3);face.mask=(frame%2)?7ULL:5ULL;}
        m.faces.push_back(face);
        for(unsigned k=0;k<3;++k){f::Edge e{m.edges.size(),{base+k,base+(k+1)%3}};
            if(mode==1){e.category=1ULL<<(i%3);e.mask=(frame%2)?7ULL:6ULL;}
            m.edges.push_back(e);}
    }
    if(mode==2&&frame%2){std::reverse(m.faces.begin(),m.faces.end());std::reverse(m.edges.begin(),m.edges.end());
        for(std::size_t i=0;i<m.faces.size();++i)m.faces[i].id=i;
        for(std::size_t i=0;i<m.edges.size();++i)m.edges[i].id=i;}
    return m;
}
int main(){try{
    std::cout<<"scene,width,regime,block,leaf,slot,arm,ns,candidates,vertices,edges,faces\n";
    for(auto seed:{61057ULL,61069ULL,61091ULL,61121ULL})for(int mode=0;mode<5;++mode){
        std::vector<f::Mesh> motion;
        for(int frame=0;frame<6;++frame)motion.push_back(generate(seed,mode,frame));
        run("heldout_"+std::to_string(seed),1,std::array<const char*,5>{"sparse","dense_masks","topology_shear","large_offset","subnormal"}[mode],motion);
    }
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

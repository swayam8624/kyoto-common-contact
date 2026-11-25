// Audit actual accepted coupled FEM linear leaves, with one combined
// node index space so cross-body cell intersections cannot be mistaken
// for shared topology. An unresolved exact reference is a failed test,
// not proof of an actual intersection.
#include "kyoto/connected_self_contact_fixture.hpp"
#include "kyoto/exact_swept_volume_audit.hpp"
#include <iostream>
#include <numeric>
using namespace kyoto::solid;
namespace g=kyoto::geometry;
static void append(const Body& before,const Body& after,std::vector<g::Point>& x,
                   std::vector<g::Point>& y,std::vector<g::ExactTetIds>& cells){
    if(before.nodes.size()!=after.nodes.size()||before.tets.size()!=after.tets.size())
        throw std::runtime_error("native leaf topology changed");
    const auto base=x.size();
    for(std::size_t i=0;i<before.nodes.size();++i){
        const auto a=before.nodes[i].x,b=after.nodes[i].x;
        x.push_back({a.x,a.y,a.z});y.push_back({b.x,b.y,b.z});
    }
    for(std::size_t i=0;i<before.tets.size();++i){
        auto row=before.tets[i].n;
        if(row!=after.tets[i].n)throw std::runtime_error("native leaf cell order changed");
        for(auto& id:row)id+=base;
        cells.push_back(row);
    }
}
int main(){
    Body a=fixtures::u_solid(),b=fixtures::remote();
    CoupledFEMSettings cfg;cfg.typed_self_contact=cfg.hierarchy=true;
    CoupledFEMContext context;
    std::size_t leaves=0,impulses=0,unknown=0,exact_pairs=0;
    std::cout<<"macrostep,leaf,dt,cells,pairs,overlap_tests,orientation_unknown,pair_unknown,certified\n";
    for(int step=0;step<8;++step){
        std::vector<CoupledFEMLeaf> trace;
        const auto r=step_coupled_fem(a,b,.00025,cfg,&trace,&context);
        if(trace.size()!=r.accepted_leaves||trace.empty())
            throw std::runtime_error("missing actual native leaf");
        impulses+=r.self_a.impulse_updates;
        for(std::size_t i=0;i<trace.size();++i){
            const auto& l=trace[i];
            std::vector<g::Point> x,y;std::vector<g::ExactTetIds> cells;
            append(l.before_a,l.after_a,x,y,cells);
            append(l.before_b,l.after_b,x,y,cells);
            const auto p=g::audit_exact_swept_volume(x,y,cells,6);
            const auto q=g::audit_exact_swept_volume(x,y,cells,6,true);
            if(p.certified!=q.certified||p.unknown!=q.unknown||p.tested!=q.tested||
               p.orientation_unknown!=q.orientation_unknown||p.first_unknown!=q.first_unknown||
               p.polynomials!=q.polynomials||p.axes!=q.axes||p.pair_nodes!=q.pair_nodes)
                throw std::runtime_error("native exact optimized/exhaustive audit disagreement");
            const auto u=std::accumulate(p.unknown.begin(),p.unknown.end(),std::size_t{});
            unknown+=!p.certified;exact_pairs+=std::accumulate(p.tested.begin(),p.tested.end(),std::size_t{});
            ++leaves;
            std::cout<<step<<','<<i<<','<<l.dt<<','<<p.cells<<','<<p.pair_tests<<','<<p.overlap_tests<<','
                     <<p.orientation_unknown<<','<<u<<','<<p.certified<<'\n';
            if(!p.certified)std::cerr<<"UNRESOLVED native leaf "<<step<<':'<<i
                                     <<" first pair "<<p.first_unknown[0]<<','<<p.first_unknown[1]<<'\n';
        }
    }
    if(!impulses||!exact_pairs||leaves<8)throw std::runtime_error("vacuous native continuous audit");
    std::cerr<<"native continuous audit leaves="<<leaves<<" unknown="<<unknown
             <<" exact_pairs="<<exact_pairs<<" self_impulses="<<impulses<<'\n';
    return unknown?1:0;
}

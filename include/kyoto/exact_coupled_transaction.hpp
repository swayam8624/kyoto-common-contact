#pragma once
// Exact-volume gate for the native coupled solver. This is deliberately an
// opt-in reference mode: the native time integrator proposes and certifies
// surface motion, then this independent integer-polynomial oracle checks
// the initial complex and every provisional accepted linear leaf. A failure
// (including an unresolved certificate) publishes neither body nor trace.
// The warm context is reset on failure; it never owns committed geometry.
#include "kyoto/coupled_fem_contact.hpp"
#include "kyoto/exact_swept_volume_audit.hpp"
#include <numeric>
#include <type_traits>

namespace kyoto::solid::exact_transaction {
namespace g=kyoto::geometry;

struct Receipt {
    CoupledFEMStats physics;
    std::size_t audited_leaves{}, exact_pairs{}, possible_pairs{}, overlap_tests{};
    g::ExactSweptVolumeReceipt initial;
    std::vector<g::ExactSweptVolumeReceipt> leaves;
};

// Assign disjoint node IDs to the two bodies. Matching coordinates cannot
// create artificial shared simplices between distinct physical bodies.
inline void append(const Body& start,const Body& end,std::vector<g::Point>& x,
                   std::vector<g::Point>& y,std::vector<g::ExactTetIds>& cells){
    if(start.nodes.size()!=end.nodes.size()||start.tets.size()!=end.tets.size())
        throw std::invalid_argument("exact transaction topology changed");
    const auto offset=x.size();
    for(std::size_t i=0;i<start.nodes.size();++i){
        const auto a=start.nodes[i].x,b=end.nodes[i].x;
        x.push_back({a.x,a.y,a.z});y.push_back({b.x,b.y,b.z});
    }
    for(std::size_t i=0;i<start.tets.size();++i){
        auto row=start.tets[i].n;
        if(row!=end.tets[i].n)throw std::invalid_argument("exact transaction cell changed");
        for(auto& id:row){if(id>=start.nodes.size())throw std::invalid_argument("invalid exact transaction index");id+=offset;}
        cells.push_back(row);
    }
}

inline g::ExactSweptVolumeReceipt audit(const Body& a,const Body& b,
                                      const Body& aa,const Body& bb,unsigned depth){
    std::vector<g::Point> x,y;std::vector<g::ExactTetIds> cells;
    append(a,aa,x,y,cells);append(b,bb,x,y,cells);
    return g::audit_exact_swept_volume(x,y,cells,depth,false,true,true);
}

// Return the same native physical receipt with additional exact evidence.
// No retry at this outer boundary: if native accepted a path that the exact
// audit cannot certify, reject the entire macrostep. A caller may choose a
// smaller dt, but this function cannot silently change elapsed time.
inline Receipt step(Body& a,Body& b,double dt,const CoupledFEMSettings& settings={},
                    std::vector<CoupledFEMLeaf>* trace=nullptr,
                    CoupledFEMContext* context=nullptr,unsigned depth=6){
    static_assert(std::is_nothrow_move_assignable_v<Body>);
    static_assert(std::is_nothrow_move_constructible_v<Receipt>);
    CoupledFEMContext local;auto& warm=context?*context:local;
    struct Reset {CoupledFEMContext& c;bool committed{};~Reset(){if(!committed)c.reset();}} guard{warm};
    if(&a==&b||depth>12||!std::isfinite(dt)||dt<=0.)
        throw std::invalid_argument("invalid exact transaction settings");
    Receipt r;
    r.initial=audit(a,b,a,b,depth);
    if(!r.initial.certified)throw std::runtime_error("exact initial volume uncertified");
    auto aa=a,bb=b;std::vector<CoupledFEMLeaf> provisional;
    r.physics=step_coupled_fem(aa,bb,dt,settings,&provisional,&warm);
    if(provisional.empty()||provisional.size()!=r.physics.accepted_leaves)
        throw std::runtime_error("exact transaction missing native leaves");
    r.leaves.reserve(provisional.size());
    for(const auto& leaf:provisional){
        auto q=audit(leaf.before_a,leaf.before_b,leaf.after_a,leaf.after_b,depth);
        if(!q.certified)throw std::runtime_error("exact accepted leaf volume uncertified");
        r.exact_pairs+=std::accumulate(q.tested.begin(),q.tested.end(),std::size_t{});
        r.possible_pairs+=q.pair_tests;r.overlap_tests+=q.overlap_tests;
        r.leaves.push_back(q);++r.audited_leaves;
    }
    // Only no-throw operations after publication begins. Caller trace is
    // unchanged on every prior error, including allocation failure.
    a=std::move(aa);b=std::move(bb);if(trace)trace->swap(provisional);
    guard.committed=true;return r;
}
} // namespace kyoto::solid::exact_transaction

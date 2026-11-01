#include "ccp/live_cloth.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace ccp::live {
namespace {
using Clock = std::chrono::steady_clock;
constexpr int kSide = 25;
constexpr double kClothSide = 3.2;
constexpr double kDt = 1.0 / 120.0;
constexpr double kGravity = -9.81;
constexpr double kThickness = 0.02;
constexpr int kSolverIterations = 8;

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 mul(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
double length(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 closestOnSegment(Vec3 p, Vec3 a, Vec3 b) {
    const Vec3 ab=sub(b,a);
    const double den=dot(ab,ab);
    if(den<=1e-30) return a;
    return add(a,mul(ab,std::clamp(dot(sub(p,a),ab)/den,0.0,1.0)));
}
Vec3 normalized(Vec3 a) {
    const double l = length(a);
    return l > 1e-12 ? mul(a, 1.0/l) : Vec3{0.0, 0.0, 1.0};
}
bool overlaps(const Bounds& a, const Bounds& b) {
    return a.min.x <= b.max.x && b.min.x <= a.max.x &&
           a.min.y <= b.max.y && b.min.y <= a.max.y &&
           a.min.z <= b.max.z && b.min.z <= a.max.z;
}
bool eligible(const Bounds& a, const Bounds& b) {
    return (a.mask & b.category)!=0 && (b.mask & a.category)!=0;
}
Bounds expanded(Bounds b, double d) {
    b.min = sub(b.min, {d, d, d});
    b.max = add(b.max, {d, d, d});
    return b;
}
std::vector<Bounds> queryBoundsFor(const std::vector<Vec3>& positions,
                                   const std::vector<std::array<int,3>>& triangles,
                                   Vec3 sphere, double radius) {
    std::vector<Bounds> result;
    result.reserve(triangles.size()+1);
    result.push_back({static_cast<int>(triangles.size()),
        {sphere.x-radius,sphere.y-radius,sphere.z-radius},
        {sphere.x+radius,sphere.y+radius,sphere.z+radius},2u,1u,false});
    for(std::size_t ti=0;ti<triangles.size();++ti) {
        const auto tri=triangles[ti]; Vec3 lo=positions[tri[0]],hi=lo;
        for(int k=1;k<3;++k) { const auto p=positions[tri[k]];
            lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
            hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
        }
        result.push_back({static_cast<int>(ti),lo,hi,1u,2u,true});
    }
    return result;
}
bool containsBounds(const Bounds& envelope, const Bounds& current) {
    return envelope.id==current.id && envelope.category==current.category &&
        envelope.mask==current.mask && envelope.movable==current.movable &&
        envelope.min.x<=current.min.x && envelope.min.y<=current.min.y && envelope.min.z<=current.min.z &&
        envelope.max.x>=current.max.x && envelope.max.y>=current.max.y && envelope.max.z>=current.max.z;
}
void validateBounds(const std::vector<Bounds>& bounds) {
    std::vector<int> ids; ids.reserve(bounds.size());
    for(const auto& b:bounds) {
        const double values[]={b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z};
        if(!std::all_of(std::begin(values),std::end(values),[](double v){return std::isfinite(v);}))
            throw std::invalid_argument("AABB coordinates must be finite");
        if(b.min.x>b.max.x || b.min.y>b.max.y || b.min.z>b.max.z)
            throw std::invalid_argument("AABB minimum must not exceed maximum");
        ids.push_back(b.id);
    }
    std::sort(ids.begin(),ids.end());
    if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())
        throw std::invalid_argument("AABB IDs must be unique");
}
bool sameRequest(const QueryRequest& a,const QueryRequest& b) {
    return a.margin==b.margin && a.displacementBudget==b.displacementBudget && a.updatePolicy==b.updatePolicy;
}
void validateGenericRequest(const QueryRequest& request) {
    if(!std::isfinite(request.margin) || request.margin<0.0)
        throw std::invalid_argument("query margin must be finite and nonnegative");
    if(!std::isfinite(request.displacementBudget) || request.displacementBudget<0.0)
        throw std::invalid_argument("displacement budget must be finite and nonnegative");
    if(request.updatePolicy!=UpdatePolicy::RebuildEveryIteration && request.updatePolicy!=UpdatePolicy::GuardedReuse)
        throw std::invalid_argument("unknown broad-phase update policy");
}
Vec3 closestPoint(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
    const Vec3 ab = sub(b,a), ac = sub(c,a), ap = sub(p,a);
    const Vec3 bc=sub(c,b);
    const Vec3 normal{ab.y*ac.z-ab.z*ac.y,ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
    const double scale=std::max({dot(ab,ab),dot(ac,ac),dot(bc,bc),1e-30});
    if(dot(normal,normal)<=1e-24*scale*scale) {
        const Vec3 qab=closestOnSegment(p,a,b), qbc=closestOnSegment(p,b,c), qca=closestOnSegment(p,c,a);
        const double dab=dot(sub(p,qab),sub(p,qab));
        const double dbc=dot(sub(p,qbc),sub(p,qbc));
        const double dca=dot(sub(p,qca),sub(p,qca));
        return dab<=dbc && dab<=dca ? qab : (dbc<=dca ? qbc : qca);
    }
    const double d1=dot(ab,ap), d2=dot(ac,ap);
    if(d1<=0.0 && d2<=0.0) return a;
    const Vec3 bp=sub(p,b); const double d3=dot(ab,bp), d4=dot(ac,bp);
    if(d3>=0.0 && d4<=d3) return b;
    const double vc=d1*d4-d3*d2;
    if(vc<=0.0 && d1>=0.0 && d3<=0.0) return add(a,mul(ab,d1/(d1-d3)));
    const Vec3 cp=sub(p,c); const double d5=dot(ab,cp), d6=dot(ac,cp);
    if(d6>=0.0 && d5<=d6) return c;
    const double vb=d5*d2-d1*d6;
    if(vb<=0.0 && d2>=0.0 && d6<=0.0) return add(a,mul(ac,d2/(d2-d6)));
    const double va=d3*d6-d5*d4;
    if(va<=0.0 && (d4-d3)>=0.0 && (d5-d6)>=0.0)
        return add(b,mul(sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
    const double denom=1.0/(va+vb+vc);
    return add(add(a,mul(ab,vb*denom)),mul(ac,vc*denom));
}
std::array<double,3> barycentric(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
    const Vec3 v0=sub(b,a),v1=sub(c,a),v2=sub(p,a);
    const double d00=dot(v0,v0),d01=dot(v0,v1),d11=dot(v1,v1);
    const double d20=dot(v2,v0),d21=dot(v2,v1);
    const double den=d00*d11-d01*d01;
    if(std::abs(den)<1e-18) return {1.0/3.0,1.0/3.0,1.0/3.0};
    const double v=(d11*d20-d01*d21)/den, w=(d00*d21-d01*d20)/den;
    return {1.0-v-w,v,w};
}
}

std::vector<Pair> candidates(const std::vector<Bounds>& bounds, bool useSAP, std::size_t* pairTests) {
    std::vector<int> order(bounds.size());
    std::iota(order.begin(), order.end(), 0);
    validateBounds(bounds);
    std::size_t tests=0;
    std::vector<Pair> out;
    if(useSAP) {
        std::sort(order.begin(),order.end(),[&](int a,int b){
            if(bounds[a].min.x!=bounds[b].min.x) return bounds[a].min.x<bounds[b].min.x;
            return bounds[a].id<bounds[b].id;
        });
        for(std::size_t i=0;i<order.size();++i) {
            const auto ai=order[i];
            for(std::size_t j=i+1;j<order.size();++j) {
                const auto bi=order[j];
                if(bounds[bi].min.x>bounds[ai].max.x) break;
                if(!eligible(bounds[ai],bounds[bi])) continue;
                ++tests;
                if(overlaps(bounds[ai],bounds[bi]))
                    out.push_back({std::min(bounds[ai].id,bounds[bi].id),
                                   std::max(bounds[ai].id,bounds[bi].id)});
            }
        }
    } else {
        for(std::size_t i=0;i<bounds.size();++i)
            for(std::size_t j=i+1;j<bounds.size();++j) {
                if(!eligible(bounds[i],bounds[j])) continue;
                ++tests;
                if(overlaps(bounds[i],bounds[j]))
                    out.push_back({std::min(bounds[i].id,bounds[j].id),
                                   std::max(bounds[i].id,bounds[j].id)});
            }
    }
    std::sort(out.begin(),out.end());
    out.erase(std::unique(out.begin(),out.end()),out.end());
    if(pairTests) *pairTests=tests;
    return out;
}

const std::vector<Pair>& BroadPhaseService::query(const std::vector<Bounds>& rawGeometryBounds,
                                                   QueryRequest request,
                                                   bool useSAP,
                                                   QueryMetrics& metrics) {
    const auto start=Clock::now();
    validateGenericRequest(request);
    validateBounds(rawGeometryBounds); // Always validate, including a potential cache reuse.
    std::vector<Bounds> current; current.reserve(rawGeometryBounds.size());
    for(const auto& raw:rawGeometryBounds) current.push_back(expanded(raw,request.margin*0.5));
    validateBounds(current);

    // A cache *decision* is scientific evidence: a rebuild due to an
    // escaping triangle has a different meaning than explicit policy
    // changes, topology changes, or a first-time empty cache.
    bool canReuse=true;
    if(request.updatePolicy!=UpdatePolicy::GuardedReuse) {
        canReuse=false; metrics.lastDecision="POLICY_REBUILD";
    } else if(!cacheValid_) {
        canReuse=false; metrics.lastDecision="CACHE_EMPTY";
    } else if(!sameRequest(cachedRequest_,request)) {
        canReuse=false; metrics.lastDecision="REQUEST_CHANGED";
    } else if(cachedSAP_!=useSAP) {
        canReuse=false; metrics.lastDecision="BACKEND_CHANGED";
    } else if(envelopes_.size()!=current.size()) {
        canReuse=false; metrics.lastDecision="PRIMITIVE_COUNT_CHANGED";
    }
    if(canReuse) for(std::size_t i=0;i<current.size();++i) {
        if(!containsBounds(envelopes_[i],current[i])) {
            canReuse=false;
            metrics.lastDecision="ENVELOPE_ESCAPE_OR_METADATA_CHANGE";
            break;
        }
    }
    if(canReuse) {
        ++metrics.reuses;
        metrics.lastDecision="CERTIFIED_REUSE";
        metrics.broadMs+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        return pairs_;
    }

    ++metrics.builds;
    envelopes_=std::move(current);
    if(request.updatePolicy==UpdatePolicy::GuardedReuse)
        for(auto& bound:envelopes_) if(bound.movable) bound=expanded(bound,request.displacementBudget);
    validateBounds(envelopes_);
    std::size_t tests=0;
    pairs_=candidates(envelopes_,useSAP,&tests);
    metrics.pairTests+=tests;
    metrics.broadMs+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    cachedRequest_=request; cachedSAP_=useSAP; cacheValid_=true;
    return pairs_;
}

const std::vector<Bounds>& BroadPhaseService::envelopes() const { return envelopes_; }
void BroadPhaseService::invalidate() {
    cacheValid_=false;
    envelopes_.clear(); pairs_.clear();
}

ClothSimulation::ClothSimulation() { reset(); }

void ClothSimulation::reset() {
    const bool sap=stats_.sap, collisions=stats_.collisions;
    positions_.clear(); previousPositions_.clear(); triangles_.clear(); constraints_.clear();
    stats_={};
    stats_.sap=sap; stats_.collisions=collisions;
    stats_.oracleEnabled=oracleEnabled_;
    stats_.guardedReuse=queryRequest_.updatePolicy==UpdatePolicy::GuardedReuse;
    broadPhase_.invalidate();
    positions_.reserve(kSide*kSide);
    const double spacing=kClothSide/(kSide-1);
    for(int y=0;y<kSide;++y) for(int x=0;x<kSide;++x)
        positions_.push_back({-kClothSide/2+x*spacing,-kClothSide/2+y*spacing,2.4});
    previousPositions_=positions_;
    triangles_.reserve((kSide-1)*(kSide-1)*2);
    for(int y=0;y<kSide-1;++y) for(int x=0;x<kSide-1;++x) {
        const int a=y*kSide+x, b=a+1, c=a+kSide, d=c+1;
        triangles_.push_back({a,b,d});
        triangles_.push_back({a,d,c});
    }
    auto addConstraint=[&](int a,int b,double compliance) {
        constraints_.push_back({a,b,length(sub(positions_[a],positions_[b])),compliance,0.0});
    };
    for(int y=0;y<kSide;++y) for(int x=0;x<kSide;++x) {
        const int i=y*kSide+x;
        if(x+1<kSide) addConstraint(i,i+1,1e-7);
        if(y+1<kSide) addConstraint(i,i+kSide,1e-7);
        if(x+1<kSide && y+1<kSide) {
            addConstraint(i,i+kSide+1,1e-6);
            addConstraint(i+1,i+kSide,1e-6);
        }
        if(x+2<kSide) addConstraint(i,i+2,2e-4);
        if(y+2<kSide) addConstraint(i,i+2*kSide,2e-4);
    }
    stats_.minGap=std::numeric_limits<double>::infinity();
}

void ClothSimulation::step() {
    constexpr Vec3 sphere{0.0,0.0,0.9};
    constexpr double radius=0.8;
    constexpr double invMass=1.0;
    constexpr double alphaScale=1.0/(kDt*kDt);
    const auto frameStart=Clock::now();
    std::vector<Vec3> old=positions_;
    for(std::size_t i=0;i<positions_.size();++i) {
        Vec3 v=mul(sub(positions_[i],previousPositions_[i]),0.995);
        v.z+=kGravity*kDt*kDt;
        positions_[i]=add(positions_[i],v);
    }
    for(auto& c:constraints_) c.lambda=0.0;
    double narrowTotal=0.0;
    QueryMetrics queryMetrics;
    stats_.guardedReuse=queryRequest_.updatePolicy==UpdatePolicy::GuardedReuse;
    stats_.oracleEnabled=oracleEnabled_;
    stats_.oracleChecked=false;
    stats_.oracleMisses=0;
    stats_.oracleMs=0.0;
    stats_.diagnosticMs=0.0;
    stats_.preciseTests=0;
    stats_.oracleTests=0;
    stats_.diagnosticTests=0;
    stats_.candidateTriangles.clear(); stats_.contactTriangles.clear();
    for(int iteration=0;iteration<kSolverIterations;++iteration) {
        const auto solverStart=Clock::now();
        for(auto& c:constraints_) {
            const Vec3 d=sub(positions_[c.a],positions_[c.b]);
            const double len=length(d);
            if(len<1e-12) continue;
            const double C=len-c.rest, alpha=c.compliance*alphaScale;
            const double dl=(-C-alpha*c.lambda)/(2.0*invMass+alpha);
            c.lambda+=dl;
            const Vec3 corr=mul(d,dl/len);
            positions_[c.a]=add(positions_[c.a],corr);
            positions_[c.b]=sub(positions_[c.b],corr);
        }
        if(stats_.collisions) for(auto& p:positions_) if(p.z<kThickness) p.z=kThickness;
        if(stats_.collisions) {
            const auto current=queryBoundsFor(positions_,triangles_,sphere,radius);
            const auto& pairs=broadPhase_.query(current,queryRequest_,stats_.sap,queryMetrics);
            const auto narrowStart=Clock::now();
            const double oracleBefore=stats_.oracleMs;
            // Actual production path: precise checks ONLY for broad-phase
            // candidates. Sort IDs so a conservative cached superset cannot
            // perturb sequential constraint application order.
            std::vector<int> sphereCandidates;
            sphereCandidates.reserve(pairs.size());
            for(const auto pair:pairs) {
                if(pair.a==static_cast<int>(triangles_.size())) sphereCandidates.push_back(pair.b);
                else if(pair.b==static_cast<int>(triangles_.size())) sphereCandidates.push_back(pair.a);
            }
            std::sort(sphereCandidates.begin(),sphereCandidates.end());
            sphereCandidates.erase(std::unique(sphereCandidates.begin(),sphereCandidates.end()),sphereCandidates.end());
            std::vector<int> activeContacts;
            activeContacts.reserve(sphereCandidates.size());
            for(const int ti:sphereCandidates) {
                const auto tri=triangles_[static_cast<std::size_t>(ti)];
                const double d=length(sub(closestPoint(sphere,positions_[tri[0]],positions_[tri[1]],positions_[tri[2]]),sphere));
                ++stats_.preciseTests;
                if(d<radius+kThickness) activeContacts.push_back(ti);
            }
            // The exhaustive oracle is *not* used to drive response. It runs
            // independently for correctness evidence and has its own clock.
            if(oracleEnabled_) {
                const auto oracleStart=Clock::now();
                stats_.oracleChecked=true;
                for(std::size_t ti=0;ti<triangles_.size();++ti) {
                    const auto tri=triangles_[ti];
                    const double d=length(sub(closestPoint(sphere,positions_[tri[0]],positions_[tri[1]],positions_[tri[2]]),sphere));
                    ++stats_.oracleTests;
                    if(d<=radius+queryRequest_.margin &&
                       !std::binary_search(sphereCandidates.begin(),sphereCandidates.end(),static_cast<int>(ti)))
                        ++stats_.oracleMisses;
                }
                stats_.oracleMs+=std::chrono::duration<double,std::milli>(Clock::now()-oracleStart).count();
            }
            for(const int ti:activeContacts) {
                const auto tri=triangles_[static_cast<std::size_t>(ti)];
                const Vec3 q=closestPoint(sphere,positions_[tri[0]],positions_[tri[1]],positions_[tri[2]]);
                const Vec3 delta=sub(q,sphere); const double distance=length(delta);
                if(distance>=radius+kThickness) continue;
                const Vec3 n=normalized(delta);
                const auto bary=barycentric(q,positions_[tri[0]],positions_[tri[1]],positions_[tri[2]]);
                const double denom=bary[0]*bary[0]+bary[1]*bary[1]+bary[2]*bary[2];
                const double penetration=radius+kThickness-distance;
                if(denom>1e-14) for(int k=0;k<3;++k)
                    positions_[tri[k]]=add(positions_[tri[k]],mul(n,penetration*bary[k]/denom));
                stats_.contactTriangles.push_back(ti);
            }
            const double elapsedNarrow=std::chrono::duration<double,std::milli>(Clock::now()-narrowStart).count();
            narrowTotal+=std::max(0.0,elapsedNarrow-(stats_.oracleMs-oracleBefore));
        }
        stats_.solverMs+=std::chrono::duration<double,std::milli>(Clock::now()-solverStart).count();
    }
    previousPositions_=std::move(old);
    // Reconstruct damped velocities from the final constrained positions.
    for(std::size_t i=0;i<positions_.size();++i) {
        const Vec3 v=mul(sub(positions_[i],previousPositions_[i]),0.995);
        previousPositions_[i]=sub(positions_[i],v);
    }
    stats_.narrowMs=narrowTotal; stats_.iterations=kSolverIterations;
    stats_.frame++;
    stats_.contactTriangles.erase(std::unique(stats_.contactTriangles.begin(),stats_.contactTriangles.end()),stats_.contactTriangles.end());

    const auto finalBounds=queryBoundsFor(positions_,triangles_,sphere,radius);
    const auto& finalPairs=broadPhase_.query(finalBounds,queryRequest_,stats_.sap,queryMetrics);
    stats_.queryBounds=broadPhase_.envelopes();
    stats_.queryBuilds=queryMetrics.builds; stats_.queryReuses=queryMetrics.reuses;
    stats_.pairTests=queryMetrics.pairTests; stats_.broadMs=queryMetrics.broadMs;
    stats_.cacheDecision=queryMetrics.lastDecision;
    stats_.candidateTriangles.clear(); stats_.contactTriangles.clear();
    stats_.minGap=std::numeric_limits<double>::infinity(); stats_.proximityCount=0;
    for(const auto pair:finalPairs) {
        if(pair.a!=static_cast<int>(triangles_.size()) && pair.b!=static_cast<int>(triangles_.size())) continue;
        const int ti=pair.a==static_cast<int>(triangles_.size())?pair.b:pair.a;
        stats_.candidateTriangles.push_back(ti);
    }
    // Global final-frame scan is for scientific visualization and min-gap
    // statistics, not for production collision response. Always count its cost.
    const auto diagnosticStart=Clock::now();
    for(std::size_t ti=0;ti<triangles_.size();++ti) {
        const auto tri=triangles_[ti];
        const double d=length(sub(closestPoint(sphere,positions_[tri[0]],positions_[tri[1]],positions_[tri[2]]),sphere));
        ++stats_.diagnosticTests;
        const double gap=d-radius;
        stats_.minGap=std::min(stats_.minGap,gap);
        if(d<=radius+queryRequest_.margin) {
            ++stats_.proximityCount; stats_.contactTriangles.push_back(static_cast<int>(ti));
            if(oracleEnabled_ &&
               !std::binary_search(stats_.candidateTriangles.begin(),stats_.candidateTriangles.end(),static_cast<int>(ti)))
                ++stats_.oracleMisses;
        }
    }
    stats_.diagnosticMs=std::chrono::duration<double,std::milli>(Clock::now()-diagnosticStart).count();
    stats_.candidateCount=stats_.candidateTriangles.size();
    double maxStrain=0.0;
    for(const auto& c:constraints_) if(c.rest>1e-12)
        maxStrain=std::max(maxStrain,std::abs(length(sub(positions_[c.a],positions_[c.b]))/c.rest-1.0));
    stats_.maxStrain=maxStrain;
    stats_.stepMs=std::chrono::duration<double,std::milli>(Clock::now()-frameStart).count();
    // Residual includes XPBD solver, integration, bound construction, and
    // bookkeeping; it must not be mislabeled as pure solver microtiming.
    stats_.solverMs=std::max(0.0,stats_.stepMs-queryMetrics.broadMs-narrowTotal
                                       -stats_.oracleMs-stats_.diagnosticMs);
}

void ClothSimulation::setBroadPhase(bool sap) {
    if(stats_.sap!=sap) broadPhase_.invalidate();
    stats_.sap=sap;
}
void ClothSimulation::setCollisions(bool enabled) { stats_.collisions=enabled; }
void ClothSimulation::setOracleEnabled(bool enabled) {
    oracleEnabled_=enabled;
    stats_.oracleEnabled=enabled;
    // The previous frame's coverage result cannot validate a new mode.
    stats_.oracleChecked=false;
    stats_.oracleMisses=0;
}
bool ClothSimulation::oracleEnabled() const { return oracleEnabled_; }
void ClothSimulation::setQueryRequest(QueryRequest request) {
    if(!std::isfinite(request.margin) || request.margin<kThickness)
        throw std::invalid_argument("query margin must be finite and at least the response thickness");
    if(!std::isfinite(request.displacementBudget) || request.displacementBudget<0.0)
        throw std::invalid_argument("displacement budget must be finite and nonnegative");
    if(request.updatePolicy!=UpdatePolicy::RebuildEveryIteration && request.updatePolicy!=UpdatePolicy::GuardedReuse)
        throw std::invalid_argument("unknown broad-phase update policy");
    if(request.margin!=queryRequest_.margin || request.displacementBudget!=queryRequest_.displacementBudget ||
       request.updatePolicy!=queryRequest_.updatePolicy) broadPhase_.invalidate();
    queryRequest_=request;
    stats_.guardedReuse=queryRequest_.updatePolicy==UpdatePolicy::GuardedReuse;
}
QueryRequest ClothSimulation::queryRequest() const { return queryRequest_; }
const std::vector<Vec3>& ClothSimulation::positions() const { return positions_; }
const std::vector<std::array<int,3>>& ClothSimulation::triangles() const { return triangles_; }
Vec3 ClothSimulation::sphereCenter() const { return {0.0,0.0,0.9}; }
double ClothSimulation::sphereRadius() const { return 0.8; }
const Stats& ClothSimulation::stats() const { return stats_; }

} // namespace ccp::live

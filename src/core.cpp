#include "ccp/core.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace ccp {
namespace {
using Clock = std::chrono::steady_clock;
constexpr double kFixedDt = 1.0 / 120.0;
constexpr double kTimeEpsilon = 1.0e-12;

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Vec3 a) { return std::sqrt(dot(a,a)); }

struct Bounds { Vec3 lo, hi; };
Bounds queryBounds(const Body& body, double interval, bool swept) {
    const Vec3 end = swept ? body.position + body.velocity * interval : body.position;
    return {{std::min(body.position.x,end.x)-body.radius,
             std::min(body.position.y,end.y)-body.radius,
             std::min(body.position.z,end.z)-body.radius},
            {std::max(body.position.x,end.x)+body.radius,
             std::max(body.position.y,end.y)+body.radius,
             std::max(body.position.z,end.z)+body.radius}};
}
bool overlap(const Bounds& a, const Bounds& b) {
    return a.lo.x<=b.hi.x && b.lo.x<=a.hi.x && a.lo.y<=b.hi.y && b.lo.y<=a.hi.y && a.lo.z<=b.hi.z && b.lo.z<=a.hi.z;
}
std::vector<Pair> generateCandidates(const std::vector<Body>& bodies, const std::string& mode,
                                     bool swept, double interval) {
    std::vector<Bounds> boxes; boxes.reserve(bodies.size());
    for (const auto& body : bodies) boxes.push_back(queryBounds(body, interval, swept));
    std::vector<Pair> pairs;
    if (mode=="brute") {
        for (std::size_t i=0;i<bodies.size();++i)
            for (std::size_t j=i+1;j<bodies.size();++j)
                if (overlap(boxes[i],boxes[j])) pairs.push_back({static_cast<int>(i),static_cast<int>(j)});
    } else {
        std::vector<int> order(bodies.size()); std::iota(order.begin(),order.end(),0);
        std::sort(order.begin(),order.end(),[&](int a,int b) {
            return boxes[a].lo.x==boxes[b].lo.x ? a<b : boxes[a].lo.x<boxes[b].lo.x;
        });
        for (std::size_t oi=0;oi<order.size();++oi) {
            const int i=order[oi];
            for (std::size_t oj=oi+1;oj<order.size();++oj) {
                const int j=order[oj];
                if (boxes[j].lo.x>boxes[i].hi.x) break;
                if (overlap(boxes[i],boxes[j])) pairs.push_back({std::min(i,j),std::max(i,j)});
            }
        }
        std::sort(pairs.begin(),pairs.end());
    }
    return pairs;
}

struct DisjointSet {
    std::vector<int> p;
    explicit DisjointSet(std::size_t n):p(n) { std::iota(p.begin(),p.end(),0); }
    int find(int x) { return p[x]==x ? x : p[x]=find(p[x]); }
    void join(int a,int b) { a=find(a); b=find(b); if(a!=b) p[b]=a; }
};
std::vector<int> contactIslands(std::size_t bodyCount, const std::vector<Contact>& contacts) {
    DisjointSet sets(bodyCount);
    for (const auto& c:contacts) sets.join(c.a,c.b);
    std::vector<int> rootToIsland(bodyCount,-1), islands(bodyCount);
    int next=0;
    for (std::size_t i=0;i<bodyCount;++i) {
        const int root=sets.find(static_cast<int>(i));
        if(rootToIsland[root]<0) rootToIsland[root]=next++;
        islands[i]=rootToIsland[root];
    }
    return islands;
}
Contact makeContact(const Body& a, const Body& b, double timeOfImpact) {
    const Vec3 delta=b.position-a.position;
    const double d=length(delta), sum=a.radius+b.radius;
    const Vec3 normal=d>1e-12 ? delta*(1.0/d) : Vec3{1.0,0.0,0.0};
    const double penetration=std::max(0.0,sum-d);
    const Vec3 point=a.position+normal*(a.radius-penetration*0.5);
    return {a.id,b.id,normal,point,d,penetration,timeOfImpact};
}
void impulse(Body& a, Body& b, Vec3 normal) {
    const double invA=1.0/a.mass, invB=1.0/b.mass;
    const double closing=dot(b.velocity-a.velocity,normal);
    if(closing<0.0) {
        constexpr double restitution=1.0;
        const double magnitude=-(1.0+restitution)*closing/(invA+invB);
        a.velocity=a.velocity-normal*(magnitude*invA);
        b.velocity=b.velocity+normal*(magnitude*invB);
    }
}
void resolveWalls(Body& body) {
    auto axis=[&](double& p,double& v,double radius) {
        const double edge=1.0-radius;
        if(p>edge){p=edge;v=-std::abs(v);} else if(p<-edge){p=-edge;v=std::abs(v);}
    };
    axis(body.position.x,body.velocity.x,body.radius);
    axis(body.position.y,body.velocity.y,body.radius);
    axis(body.position.z,body.velocity.z,body.radius);
}
void appendPathPoint(std::vector<Vec3>& path, Vec3 p) {
    if(path.empty() || length(path.back()-p)>1e-11) path.push_back(p);
}
}

std::optional<double> sphereSphereTimeOfImpact(Vec3 relativePosition, Vec3 relativeVelocity,
                                                double radiusSum, double maxTime) {
    if(radiusSum<0.0 || maxTime<0.0) throw std::invalid_argument("invalid sphere TOI query");
    const double a=dot(relativeVelocity,relativeVelocity);
    const double b=2.0*dot(relativePosition,relativeVelocity);
    const double c=dot(relativePosition,relativePosition)-radiusSum*radiusSum;
    if(c<=1e-14) return b<0.0 ? std::optional<double>{0.0} : std::nullopt;
    if(a<=1e-24) return std::nullopt;
    const double discriminant=b*b-4.0*a*c;
    const double tolerance=1e-14*(b*b+std::abs(4.0*a*c)+1.0);
    if(discriminant < -tolerance) return std::nullopt;
    const double root=(-b-std::sqrt(std::max(0.0,discriminant)))/(2.0*a);
    if(root < -kTimeEpsilon || root > maxTime+kTimeEpsilon) return std::nullopt;
    return std::clamp(root,0.0,maxTime);
}

Simulation::Simulation() { reset("elastic"); }

void Simulation::reset(const std::string& scene) {
    if (scene!="elastic" && scene!="islands" && scene!="dense" && scene!="tunneling")
        throw std::invalid_argument("unknown scene");
    scene_=scene; frame_=0; bodies_.clear(); candidates_.clear(); contacts_.clear(); islands_.clear();
    motionPaths_.clear(); freeFlightPositions_.clear();
    if (scene=="elastic") {
        bodies_={{0,{-0.72,0.0,0.0},{1.25,0.0,0.0},0.18,1.0},
                 {1,{0.72,0.0,0.0},{-1.25,0.0,0.0},0.18,1.0},
                 {2,{0.0,0.65,0.0},{0.0,0.0,0.0},0.16,1.0}};
    } else if(scene=="islands") {
        for(int i=0;i<6;++i) bodies_.push_back({i,{-0.78+0.23*i,0.28,0.0},{0.0,0.0,0.0},0.13,1.0});
        for(int i=0;i<4;++i) bodies_.push_back({6+i,{0.18+0.23*i,-0.45,0.32},{0.0,0.0,0.0},0.13,1.0});
    } else if(scene=="dense") {
        int id=0;
        for(int y=0;y<5;++y) for(int x=0;x<9;++x)
            bodies_.push_back({id++,{-0.82+0.205*x,-0.38+0.19*y,0.025*((x+y)%3)},
                               {0.0,0.0,0.0},0.105,1.0});
    } else {
        bodies_={{0,{-0.72,0.0,0.0},{110.0,0.0,0.0},0.18,1.0},
                 {1,{0.72,0.0,0.0},{-110.0,0.0,0.0},0.18,1.0}};
    }
    detectAndResolve();
}

void Simulation::setBroadPhase(const std::string& mode) {
    if(mode!="brute" && mode!="sweep") throw std::invalid_argument("broad phase must be brute or sweep");
    broadPhase_=mode; detectAndResolve(false);
}
void Simulation::setQueryMode(const std::string& mode) {
    if(mode!="discrete" && mode!="swept") throw std::invalid_argument("query mode must be discrete or swept");
    queryMode_=mode; detectAndResolve(false);
}

void Simulation::detectAndResolve(bool applyResponse) {
    const auto start=Clock::now();
    candidates_=generateCandidates(bodies_,broadPhase_,queryMode_=="swept",kFixedDt);
    const auto broadDone=Clock::now();
    contacts_.clear();
    for(const auto pair:candidates_) {
        auto& a=bodies_[pair.a]; auto& b=bodies_[pair.b];
        const double d=length(b.position-a.position);
        if(d>a.radius+b.radius) continue;
        const auto contact=makeContact(a,b,-1.0);
        contacts_.push_back(contact);
        if(applyResponse) {
            impulse(a,b,contact.normal);
            const double invA=1.0/a.mass, invB=1.0/b.mass;
            const double correction=std::max(0.0,contact.penetration-1e-7)*0.8/(invA+invB);
            a.position=a.position-contact.normal*(correction*invA);
            b.position=b.position+contact.normal*(correction*invB);
        }
    }
    islands_=contactIslands(bodies_.size(),contacts_);
    const auto end=Clock::now();
    broadMs_=std::chrono::duration<double,std::milli>(broadDone-start).count();
    narrowMs_=std::chrono::duration<double,std::milli>(end-broadDone).count();
}

void Simulation::advanceSwept() {
    const auto originalBodies=bodies_;
    const auto originalCandidates=candidates_;
    const auto originalContacts=contacts_;
    const auto originalIslands=islands_;
    const auto originalPaths=motionPaths_;
    const auto originalFreeFlight=freeFlightPositions_;
    const double originalBroadMs=broadMs_, originalNarrowMs=narrowMs_;
    motionPaths_.assign(bodies_.size(),{});
    freeFlightPositions_.resize(bodies_.size());
    for(std::size_t i=0;i<bodies_.size();++i) {
        motionPaths_[i].push_back(bodies_[i].position);
        freeFlightPositions_[i]=bodies_[i].position+bodies_[i].velocity*kFixedDt;
    }
    contacts_.clear();
    std::vector<Pair> allCandidates;
    double remaining=kFixedDt, elapsed=0.0, broadTotal=0.0, narrowTotal=0.0;
    std::size_t eventCount=0;
    const auto initialCandidates=generateCandidates(bodies_,broadPhase_,true,kFixedDt);
    allCandidates.insert(allCandidates.end(),initialCandidates.begin(),initialCandidates.end());
    for(const auto pair:initialCandidates) {
        auto& a=bodies_[pair.a]; auto& b=bodies_[pair.b];
        const double distance=length(b.position-a.position);
        if(distance>=a.radius+b.radius) continue;
        const auto contact=makeContact(a,b,0.0);
        contacts_.push_back(contact);
        impulse(a,b,contact.normal);
        const double invA=1.0/a.mass, invB=1.0/b.mass;
        const double correction=std::max(0.0,contact.penetration-1e-7)*0.8/(invA+invB);
        a.position=a.position-contact.normal*(correction*invA);
        b.position=b.position+contact.normal*(correction*invB);
        appendPathPoint(motionPaths_[pair.a],a.position);
        appendPathPoint(motionPaths_[pair.b],b.position);
    }
    while(remaining>kTimeEpsilon) {
        const auto broadStart=Clock::now();
        const auto candidates=generateCandidates(bodies_,broadPhase_,true,remaining);
        const auto broadEnd=Clock::now();
        broadTotal+=std::chrono::duration<double,std::milli>(broadEnd-broadStart).count();
        allCandidates.insert(allCandidates.end(),candidates.begin(),candidates.end());
        const auto narrowStart=Clock::now();
        double earliest=std::numeric_limits<double>::infinity();
        std::vector<std::pair<Pair,double>> pairEvents;
        for(const auto pair:candidates) {
            const auto& a=bodies_[pair.a]; const auto& b=bodies_[pair.b];
            const auto toi=sphereSphereTimeOfImpact(b.position-a.position,b.velocity-a.velocity,
                                                    a.radius+b.radius,remaining);
            if(!toi) continue;
            if(*toi<earliest-1e-10){earliest=*toi;pairEvents.clear();}
            if(std::abs(*toi-earliest)<=1e-10) pairEvents.push_back({pair,*toi});
        }
        struct WallEvent { int body{}, axis{}; double direction{}; };
        std::vector<WallEvent> wallEvents;
        for(std::size_t i=0;i<bodies_.size();++i) {
            const auto& body=bodies_[i];
            const double p[3]={body.position.x,body.position.y,body.position.z};
            const double v[3]={body.velocity.x,body.velocity.y,body.velocity.z};
            const double edge[3]={1.0-body.radius,1.0-body.radius,1.0-body.radius};
            for(int axis=0;axis<3;++axis) {
                if(std::abs(v[axis])<1e-14) continue;
                const double bound=v[axis]>0.0?edge[axis]:-edge[axis];
                const double time=(bound-p[axis])/v[axis];
                if(time < -kTimeEpsilon || time > remaining+kTimeEpsilon) continue;
                const double clamped=std::clamp(time,0.0,remaining);
                if(clamped<earliest-1e-10){earliest=clamped;pairEvents.clear();wallEvents.clear();}
                if(std::abs(clamped-earliest)<=1e-10) wallEvents.push_back({static_cast<int>(i),axis,v[axis]>0.0?1.0:-1.0});
            }
        }
        narrowTotal+=std::chrono::duration<double,std::milli>(Clock::now()-narrowStart).count();
        if(!std::isfinite(earliest)) {
            for(std::size_t i=0;i<bodies_.size();++i) {
                bodies_[i].position=bodies_[i].position+bodies_[i].velocity*remaining;
                appendPathPoint(motionPaths_[i],bodies_[i].position);
            }
            elapsed+=remaining; remaining=0.0; break;
        }
        if(++eventCount>64) {
            bodies_=originalBodies;
            candidates_=originalCandidates; contacts_=originalContacts; islands_=originalIslands;
            motionPaths_=originalPaths; freeFlightPositions_=originalFreeFlight;
            broadMs_=originalBroadMs; narrowMs_=originalNarrowMs;
            throw std::runtime_error("swept step exceeded 64 impact events; state was rolled back");
        }
        for(std::size_t i=0;i<bodies_.size();++i) {
            bodies_[i].position=bodies_[i].position+bodies_[i].velocity*earliest;
            appendPathPoint(motionPaths_[i],bodies_[i].position);
        }
        elapsed+=earliest; remaining=std::max(0.0,remaining-earliest);
        for(const auto& [pair,_]:pairEvents) {
            auto& a=bodies_[pair.a]; auto& b=bodies_[pair.b];
            auto contact=makeContact(a,b,elapsed);
            contacts_.push_back(contact);
            impulse(a,b,contact.normal);
        }
        for(const auto& wall:wallEvents) {
            auto& body=bodies_[wall.body];
            double* p[3]={&body.position.x,&body.position.y,&body.position.z};
            double* v[3]={&body.velocity.x,&body.velocity.y,&body.velocity.z};
            const double edge=1.0-body.radius;
            *p[wall.axis]=wall.direction>0.0?edge:-edge;
            *v[wall.axis]=-*v[wall.axis];
        }
    }
    for(std::size_t i=0;i<bodies_.size();++i) appendPathPoint(motionPaths_[i],bodies_[i].position);
    std::sort(allCandidates.begin(),allCandidates.end());
    allCandidates.erase(std::unique(allCandidates.begin(),allCandidates.end()),allCandidates.end());
    candidates_=std::move(allCandidates);
    std::sort(contacts_.begin(),contacts_.end(),[](const Contact& a,const Contact& b){
        if(a.timeOfImpact!=b.timeOfImpact) return a.timeOfImpact<b.timeOfImpact;
        return std::pair{a.a,a.b}<std::pair{b.a,b.b};
    });
    contacts_.erase(std::unique(contacts_.begin(),contacts_.end(),[](const Contact& a,const Contact& b){return a.a==b.a&&a.b==b.b;}),contacts_.end());
    islands_=contactIslands(bodies_.size(),contacts_);
    broadMs_=broadTotal;
    narrowMs_=narrowTotal;
}

void Simulation::step() {
    if(queryMode_=="swept") {
        advanceSwept();
    } else {
        constexpr double dt=kFixedDt;
        motionPaths_.assign(bodies_.size(),{});
        freeFlightPositions_.resize(bodies_.size());
        for(std::size_t i=0;i<bodies_.size();++i) {
            const Vec3 start=bodies_[i].position;
            freeFlightPositions_[i]=start+bodies_[i].velocity*dt;
            bodies_[i].position=freeFlightPositions_[i];
            resolveWalls(bodies_[i]);
            motionPaths_[i]={start,bodies_[i].position};
        }
        detectAndResolve();
        for(std::size_t i=0;i<bodies_.size();++i) motionPaths_[i].back()=bodies_[i].position;
    }
    ++frame_;
}

Snapshot Simulation::snapshot() const {
    return {scene_,broadPhase_,queryMode_,frame_,bodies_.size()*(bodies_.size()-1)/2,candidates_.size(),
            broadMs_,narrowMs_,bodies_,candidates_,contacts_,islands_,motionPaths_,freeFlightPositions_};
}

std::string toJson(const Snapshot& s) {
    auto v=[](Vec3 p){ return "["+std::to_string(p.x)+","+std::to_string(p.y)+","+std::to_string(p.z)+"]"; };
    std::string out="{\"protocol\":1,\"scene\":\""+s.scene+"\",\"broadPhase\":\""+s.broadPhase+"\",\"queryMode\":\""+s.queryMode+"\",\"frame\":"+std::to_string(s.frame)+",\"potentialPairs\":"+std::to_string(s.potentialPairs)+",\"candidates\":"+std::to_string(s.candidates)+",\"broadMs\":"+std::to_string(s.broadMs)+",\"narrowMs\":"+std::to_string(s.narrowMs)+",\"bodies\":[";
    for(std::size_t i=0;i<s.bodies.size();++i){const auto& b=s.bodies[i];if(i)out+=',';out+="{\"id\":"+std::to_string(b.id)+",\"p\":"+v(b.position)+",\"v\":"+v(b.velocity)+",\"r\":"+std::to_string(b.radius)+",\"island\":"+std::to_string(s.islands[i])+"}";}
    out+="],\"candidatePairs\":[";for(std::size_t i=0;i<s.candidatePairs.size();++i){const auto p=s.candidatePairs[i];if(i)out+=',';out+="["+std::to_string(p.a)+","+std::to_string(p.b)+"]";}
    out+="],\"contacts\":[";for(std::size_t i=0;i<s.contacts.size();++i){const auto& c=s.contacts[i];if(i)out+=',';out+="{\"a\":"+std::to_string(c.a)+",\"b\":"+std::to_string(c.b)+",\"n\":"+v(c.normal)+",\"point\":"+v(c.point)+",\"distance\":"+std::to_string(c.distance)+",\"penetration\":"+std::to_string(c.penetration)+",\"toi\":"+std::to_string(c.timeOfImpact)+"}";}
    out+="],\"motionPaths\":[";for(std::size_t i=0;i<s.motionPaths.size();++i){if(i)out+=',';out+='[';for(std::size_t j=0;j<s.motionPaths[i].size();++j){if(j)out+=',';out+=v(s.motionPaths[i][j]);}out+=']';}
    out+="],\"freeFlightPositions\":[";for(std::size_t i=0;i<s.freeFlightPositions.size();++i){if(i)out+=',';out+=v(s.freeFlightPositions[i]);}
    out+="]}";return out;
}
} // namespace ccp

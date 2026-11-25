// Pass 40 GPU packed mask comparison: authored Metal host, device run required.
// Pass 37B: transparent Metal all-pairs AABB mask baseline on actual
// native accepted FEM geometry. GPU float boxes are outward widened
// from binary64; CPU exact post-filter removes extra pairs and applies
// adjacency. Readback, filtering and sorting are paid. Not a GPU BVH.
#include "kyoto/physical_scaling_snapshot.hpp"
#include "kyoto/gpu_packed_mask.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace f=kyoto::contact::features;
namespace s=kyoto::solid;
using Clock=std::chrono::steady_clock;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct alignas(16) Box {float lo[4]{},hi[4]{};};
static_assert(sizeof(Box)==32);
struct Params {std::uint32_t first,count,right_count,self;};
static_assert(sizeof(Params)==16);
// Round away from the interval interior, also avoid subnormal floats
// because a GPU may flush them to zero. Fail explicitly on overflow.
static float bound(double x,bool upper){
    float y=static_cast<float>(x);
    if(!std::isfinite(y))throw std::overflow_error("Metal float box range exceeded");
    if((upper&&double(y)<x)||(!upper&&double(y)>x))
        y=std::nextafter(y,upper?INFINITY:-INFINITY);
    if(!std::isfinite(y))throw std::overflow_error("Metal expanded box range exceeded");
    const float m=std::numeric_limits<float>::min();
    if(std::abs(y)<m){
        if(upper)y=y>0?m:0.f;
        else y=y<0?-m:0.f;
    }
    need(upper?double(y)>=x:double(y)<=x,"Metal conversion is not outward");
    return y;
}
static std::vector<Box> boxes(const std::vector<f::Record>& records){
    std::vector<Box> out;
    out.reserve(records.size());
    for(const auto& r:records){
        Box b;
        b.lo[0]=bound(r.bounds.min.x,false);b.lo[1]=bound(r.bounds.min.y,false);b.lo[2]=bound(r.bounds.min.z,false);
        b.hi[0]=bound(r.bounds.max.x,true);b.hi[1]=bound(r.bounds.max.y,true);b.hi[2]=bound(r.bounds.max.z,true);
        out.push_back(b);
    }
    return out;
}
static const char* shader=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Box {float4 lo,hi;};
struct Params {uint first,count,right_count,self;};
kernel void overlap_mask(device const Box* a [[buffer(0)]],
                         device const Box* b [[buffer(1)]],
                         device uint* mask [[buffer(2)]],
                         constant Params& p [[buffer(3)]],
                         uint tid [[thread_position_in_grid]]) {
    if(tid>=p.count)return;
    uint pair=p.first+tid;
    uint i=pair/p.right_count,j=pair%p.right_count;
    if(p.self!=0 && i>=j){mask[tid]=0;return;}
    // No arithmetic on bounds. Inclusive overlap with outward boxes.
    mask[tid]=all(a[i].lo<=b[j].hi)&&all(b[j].lo<=a[i].hi)?1u:0u;
}
)MSL";
struct GpuResult{
    f::Result candidates;
    std::uint64_t rectangular_slots{},dispatches{},mask_positives{},mask_bytes{};
};
class MetalMask{
    bool packed_{};
    id<MTLDevice> device_;
    id<MTLCommandQueue> queue_;
    id<MTLComputePipelineState> pipeline_;
public:
    explicit MetalMask(id<MTLDevice> d,bool packed=false):packed_(packed),device_(d){
        NSError* error=nil;
        auto* options=[[MTLCompileOptions alloc] init];
        options.fastMathEnabled=NO;
        const std::string source=packed_?kyoto::contact::gpu_mask::metal_source():std::string(shader);
        id<MTLLibrary> library=[device_ newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()]
                                                     options:options error:&error];
        if(!library)throw std::runtime_error(error.localizedDescription.UTF8String);
        pipeline_=[device_ newComputePipelineStateWithFunction:[library newFunctionWithName:(packed_?@"overlap_packed":@"overlap_mask")] error:&error];
        if(!pipeline_)throw std::runtime_error(error.localizedDescription.UTF8String);
        queue_=[device_ newCommandQueue];
        need(queue_!=nil,"Metal command queue unavailable");
    }
    GpuResult query(const f::Mesh& mesh){
        @autoreleasepool {
            const auto built=f::build(mesh);
            GpuResult result;
            auto scan=[&](const std::vector<f::Record>& a,const std::vector<f::Record>& b,bool self){
                if(a.empty()||b.empty())return;
                const auto total=std::uint64_t(a.size())*std::uint64_t(b.size());
                need(total<=std::numeric_limits<std::uint32_t>::max(),"Metal index range exceeded");
                const auto aa=boxes(a),bb=boxes(b);
                id<MTLBuffer> ab=[device_ newBufferWithBytes:aa.data() length:aa.size()*sizeof(Box) options:MTLResourceStorageModeShared];
                id<MTLBuffer> cb=[device_ newBufferWithBytes:bb.data() length:bb.size()*sizeof(Box) options:MTLResourceStorageModeShared];
                constexpr std::uint32_t chunk=262144;
                id<MTLBuffer> mb=[device_ newBufferWithLength:(packed_?kyoto::contact::gpu_mask::words(chunk):chunk)*sizeof(std::uint32_t) options:MTLResourceStorageModeShared];
                need(ab&&cb&&mb,"Metal buffer allocation failed");
                result.rectangular_slots+=total;
                for(std::uint64_t first=0;first<total;first+=chunk){
                    const Params p{std::uint32_t(first),std::uint32_t(std::min<std::uint64_t>(chunk,total-first)),std::uint32_t(b.size()),self?1u:0u};
                    id<MTLCommandBuffer> command=[queue_ commandBuffer];
                    id<MTLComputeCommandEncoder> enc=[command computeCommandEncoder];
                    need(command&&enc,"Metal command allocation failed");
                    [enc setComputePipelineState:pipeline_];
                    [enc setBuffer:ab offset:0 atIndex:0];
                    [enc setBuffer:cb offset:0 atIndex:1];
                    [enc setBuffer:mb offset:0 atIndex:2];
                    [enc setBytes:&p length:sizeof(p) atIndex:3];
                    const auto group=std::min<NSUInteger>(256,pipeline_.maxTotalThreadsPerThreadgroup);
                    const auto work=packed_?kyoto::contact::gpu_mask::words(p.count):p.count;
                    [enc dispatchThreads:MTLSizeMake(work,1,1) threadsPerThreadgroup:MTLSizeMake(group,1,1)];
                    [enc endEncoding];[command commit];[command waitUntilCompleted];
                    need(command.status==MTLCommandBufferStatusCompleted,"Metal overlap kernel failed");
                    ++result.dispatches;result.mask_bytes+=std::uint64_t(work)*sizeof(std::uint32_t);
                    const auto* mask=static_cast<const std::uint32_t*>(mb.contents);
                    for(std::uint32_t k=0;k<p.count;++k){
                        if(packed_ ? ((mask[k/32u]>>(k%32u))&1u)==0u : mask[k]==0u)continue;
                        ++result.mask_positives;
                        const auto i=(first+k)/b.size(),j=(first+k)%b.size();
                        if(self&&i>=j)throw std::runtime_error("Metal self pair ordering failed");
                        if(!f::valid_pair(a[i],b[j],result.candidates))continue;
                        if(self)result.candidates.pairs.push_back({f::Kind::EdgeEdge,std::min(a[i].id,b[j].id),std::max(a[i].id,b[j].id)});
                        else result.candidates.pairs.push_back({f::Kind::VertexFace,a[i].id,b[j].id});
                    }
                }
            };
            scan(built.vertices,built.faces,false);scan(built.edges,built.edges,true);
            f::normalize(result.candidates);return result;
        }
    }
};
// Explicit conversion controls, including ranges where float precision
// collapses different doubles and subnormal float values may flush.
static void precision_controls(MetalMask& gpu){
    for(double scale:{1.,16777216.,1e-40}){
        f::Mesh m;
        auto add=[&](double x,double y,double z){
            const kyoto::contact::Vec3 p{x,y,z};
            m.vertices.push_back({std::uint64_t(m.vertices.size()),
                                  kyoto::contact::CubicVertex{{p,p,p,p}}});
        };
        const double delta=scale==16777216.?.25:scale*.125;
        add(scale,0,0);add(scale+delta,0,0);
        add(scale,delta,0);add(scale+delta*.5,delta*.5,0);
        add(scale,0,delta);add(scale+delta,delta,0);
        m.edges={{0,{0,2}},{1,{1,3}},{2,{4,5}}};
        m.faces={{0,{0,1,2}}};
        m.thickness=0.;
        const auto expected=f::exhaustive(m).pairs;
        const auto result=gpu.query(m);
        need(!expected.empty()&&result.candidates.pairs==expected,
             "Metal rounded precision control failed");
    }
    const double tiny=1e-45;
    need(double(bound(tiny,true))>=tiny&&double(bound(tiny,false))<=tiny&&
         double(bound(-tiny,true))>=-tiny&&double(bound(-tiny,false))<=-tiny,
         "Metal subnormal enclosure failed");
}
int main(int argc,char** argv){
 @autoreleasepool {
    try{
        if(argc<2||argc>3)throw std::invalid_argument("usage: pass37_metal OUTPUT_DIR [max_width]");
        const unsigned maximum=argc==3?unsigned(std::stoul(argv[2])):16;
        need(maximum==1||maximum==4||maximum==16||maximum==32||maximum==64,"invalid width");
        id<MTLDevice> device=MTLCreateSystemDefaultDevice();
        if(!device){std::cerr<<"SKIP: no Metal device\n";return 77;}
        const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
        std::ofstream csv(dir/"metal_packed_comparison.csv"),metadata(dir/"metal_device.txt");
        metadata<<device.name.UTF8String<<'\n';
        csv<<std::setprecision(17)<<"width_cells,regime,block,leaf,backend,query_ns,candidates,rectangular_slots,dispatches,mask_positives,mask_bytes\n";
        MetalMask gpu(device),packed(device,true);
        precision_controls(gpu);precision_controls(packed);
        for(unsigned width:{1u,4u,16u,32u,64u}){
            if(width>maximum)continue;
            for(int regime=0;regime<2;++regime){
                const char* label=regime?"separated":"near_contact";
                auto a=s::fixtures::u_solid(width,regime?.02:.0025,.1),b=s::fixtures::remote();
                s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;
                s::CoupledFEMContext warm;
                std::vector<f::Mesh> samples;
                std::size_t impulses=0;
                for(int i=0;i<4;++i){
                    std::vector<s::CoupledFEMLeaf> leaves;
                    const auto r=s::step_coupled_fem(a,b,.00025,cfg,&leaves,&warm);
                    need(r.accepted_leaves==leaves.size()&&r.certified_min_j>1e-6,"invalid source");
                    impulses+=r.self_a.impulse_updates;
                    for(const auto& l:leaves)samples.push_back(kyoto::contact::physical_scaling::snapshot(l.before_a,l.after_a,cfg.skin));
                }
                need(!samples.empty()&&(regime||impulses>0),"vacuous physical workload");
                for(int block=-1;block<6;++block){
                    kyoto::contact::persistent::OrderedSweep persistent{true};
                    for(std::size_t i=0;i<samples.size();++i){
                        const auto& m=samples[i];const auto truth=f::exhaustive(m).pairs;
                        for(int slot=0;slot<3;++slot){
                            const int arm=(slot+block+3)%3;
                            const auto begin=Clock::now();
                            GpuResult r;
                            if(arm==1)r=gpu.query(m);
                            else if(arm==2)r=packed.query(m);
                            else r.candidates=persistent.query(m);
                            const auto end=Clock::now();
                            need(r.candidates.pairs==truth,"Metal or SAP candidate mismatch");
                            if(block>=0){
                                csv<<width<<','<<label<<','<<block<<','<<i<<','<<(arm==0?"persistent_axis_sap":arm==1?"metal_mask":"metal_packed")<<','
                                   <<std::chrono::duration<double,std::nano>(end-begin).count()<<','<<r.candidates.pairs.size()<<',';
                                if(arm!=0)csv<<r.rectangular_slots<<','<<r.dispatches<<','<<r.mask_positives<<','<<r.mask_bytes;
                                else csv<<",,,";
                                csv<<'\n';
                            }
                        }
                    }
                }
                std::cout<<"PASS37 Metal width="<<width<<" regime="<<label<<" candidates matched exhaustive\n";
            }
        }
        need(csv.good()&&metadata.good(),"Metal receipt write failed");return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
 }
}

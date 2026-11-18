// Phase 11 cross-API reference: native Vulkan compute exact AABB COUNT-ONLY.
//
// On Apple silicon use MoltenVK (Metal-backed Vulkan) with portability
// enumeration enabled. On Linux, Vulkan devices may be SOFTWARE Lavapipe.
// We ALWAYS print and save physical-device type and implementation name so
// a CPU Vulkan driver is never misrepresented as a hardware GPU benchmark.
//
// The native C++ CPU *3D hash-grid* reference is algorithmically distinct
// from this GPU O(N^2) exhaustive query. Counts and 2 independent 32-bit
// commutative pair fingerprints are verified, with brute CPU validation at
// <=1024 and exact analytic count for all-overlap if CPU oracle caps.
//
// No pair materialization, CCD, solver coupling, implicit global-island
// assumptions, shadow CPU timing or unbounded N^2 dispatches.
#include "ccp/cbp_pressure.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef CCP_PRESSURE_SPIRV
#error "Build with CMake so the compiled shader path is known"
#endif

namespace {
namespace fs=std::filesystem;
using namespace ccp::pressure;
using Clock=std::chrono::steady_clock;
struct Row {std::uint32_t count,sum32,xor32,tests;};
static_assert(sizeof(Row)==16);
void check(VkResult err,const char* where){
 if(err!=VK_SUCCESS)throw std::runtime_error(
    std::string(where)+" returned VkResult "+std::to_string(int(err)));
}
double elapsed(Clock::time_point begin){
 return std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
}
bool extension(const std::vector<VkExtensionProperties>& list,const char* name){
 return std::any_of(list.begin(),list.end(),[&](const auto& x){
   return std::strcmp(x.extensionName,name)==0;
 });
}
std::vector<VkExtensionProperties> instanceExts(){
 std::uint32_t n=0;
 check(vkEnumerateInstanceExtensionProperties(nullptr,&n,nullptr),
       "vkEnumerateInstanceExtensionProperties");
 std::vector<VkExtensionProperties> v(n);
 if(n)check(vkEnumerateInstanceExtensionProperties(nullptr,&n,v.data()),
            "vkEnumerateInstanceExtensionProperties(data)");
 return v;
}
std::vector<VkExtensionProperties> deviceExts(VkPhysicalDevice dev){
 std::uint32_t n=0;
 check(vkEnumerateDeviceExtensionProperties(dev,nullptr,&n,nullptr),
       "vkEnumerateDeviceExtensionProperties");
 std::vector<VkExtensionProperties> v(n);
 if(n)check(vkEnumerateDeviceExtensionProperties(dev,nullptr,&n,v.data()),
            "vkEnumerateDeviceExtensionProperties(data)");
 return v;
}
const char* kind(VkPhysicalDeviceType t){
 switch(t){
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:return "DISCRETE_GPU";
  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:return "INTEGRATED_GPU";
  case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:return "VIRTUAL_GPU";
  case VK_PHYSICAL_DEVICE_TYPE_CPU:return "SOFTWARE_CPU";
  default:return "UNKNOWN_DEVICE";
 }
}
struct Vulkan {
 VkInstance instance{VK_NULL_HANDLE};
 VkPhysicalDevice physical{VK_NULL_HANDLE};
 VkDevice device{VK_NULL_HANDLE};
 VkQueue queue{VK_NULL_HANDLE};
 VkCommandPool pool{VK_NULL_HANDLE};
 VkDescriptorSetLayout descriptorLayout{VK_NULL_HANDLE};
 VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
 VkShaderModule shader{VK_NULL_HANDLE};
 VkPipeline pipeline{VK_NULL_HANDLE};
 VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
 VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
 VkBuffer boxes{VK_NULL_HANDLE},rows{VK_NULL_HANDLE};
 VkDeviceMemory boxesMemory{VK_NULL_HANDLE},rowsMemory{VK_NULL_HANDLE};
 VkFence fence{VK_NULL_HANDLE};
 VkCommandBuffer cmd{VK_NULL_HANDLE};
 VkQueryPool timestamps{VK_NULL_HANDLE};
 VkPhysicalDeviceProperties properties{};
 std::uint32_t queueIndex{},maxN{},timestampBits{};
 bool timestampSupported{};
 void* boxesMapped{};
 void* rowsMapped{};
 ~Vulkan(){
  if(device!=VK_NULL_HANDLE)vkDeviceWaitIdle(device);
  if(boxesMapped)vkUnmapMemory(device,boxesMemory);
  if(rowsMapped)vkUnmapMemory(device,rowsMemory);
  if(timestamps)vkDestroyQueryPool(device,timestamps,nullptr);
  if(fence)vkDestroyFence(device,fence,nullptr);
  if(pool)vkDestroyCommandPool(device,pool,nullptr);
  if(boxes)vkDestroyBuffer(device,boxes,nullptr);
  if(rows)vkDestroyBuffer(device,rows,nullptr);
  if(boxesMemory)vkFreeMemory(device,boxesMemory,nullptr);
  if(rowsMemory)vkFreeMemory(device,rowsMemory,nullptr);
  if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);
  if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
  if(shader)vkDestroyShaderModule(device,shader,nullptr);
  if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
  if(descriptorLayout)vkDestroyDescriptorSetLayout(device,descriptorLayout,nullptr);
  if(device)vkDestroyDevice(device,nullptr);
  if(instance)vkDestroyInstance(instance,nullptr);
 }
 void createInstance(){
  const auto extensions=instanceExts();
  std::vector<const char*> names;
  VkInstanceCreateFlags flags=0;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
  if(extension(extensions,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)){
   names.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
   flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }
#endif
  const VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr,
     "Kyoto CBP Global Pressure",1,"Kyoto Research",1,VK_API_VERSION_1_1};
  VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  info.flags=flags;info.pApplicationInfo=&app;
  info.enabledExtensionCount=std::uint32_t(names.size());
  info.ppEnabledExtensionNames=names.data();
  check(vkCreateInstance(&info,nullptr,&instance),"vkCreateInstance");
 }
 void chooseDevice(){
  std::uint32_t n=0;
  check(vkEnumeratePhysicalDevices(instance,&n,nullptr),"vkEnumeratePhysicalDevices");
  if(!n)throw std::runtime_error(
    "No Vulkan physical devices: on macOS install Vulkan SDK + MoltenVK "
    "with VK_KHR_portability_enumeration; this is NOT a Metal benchmark");
  std::vector<VkPhysicalDevice> devices(n);
  check(vkEnumeratePhysicalDevices(instance,&n,devices.data()),"enumerate devices");
  int best=-100000;
  for(auto d:devices){
   VkPhysicalDeviceProperties props{};
   vkGetPhysicalDeviceProperties(d,&props);
   std::uint32_t nFamilies=0;
   vkGetPhysicalDeviceQueueFamilyProperties(d,&nFamilies,nullptr);
   std::vector<VkQueueFamilyProperties> families(nFamilies);
   vkGetPhysicalDeviceQueueFamilyProperties(d,&nFamilies,families.data());
   for(std::uint32_t q=0;q<nFamilies;++q){
    if(!(families[q].queueFlags&VK_QUEUE_COMPUTE_BIT))continue;
    int rank=0;
    if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)rank=400;
    if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)rank=300;
    if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)rank=200;
    if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)rank=100;
    if(rank>best){
     best=rank;physical=d;queueIndex=q;properties=props;
     timestampBits=families[q].timestampValidBits;
    }
   }
  }
  if(!physical)throw std::runtime_error("No Vulkan compute-capable queue available");
 }
 void createDevice(){
  const float priority=1.f;
  VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  q.queueFamilyIndex=queueIndex;q.queueCount=1;q.pQueuePriorities=&priority;
  const auto extensions=deviceExts(physical);
  std::vector<const char*> enabled;
  // MoltenVK advertises portability_subset; enable it explicitly when
  // required by the Vulkan loader/portability device.
  if(extension(extensions,"VK_KHR_portability_subset"))
   enabled.push_back("VK_KHR_portability_subset");
  VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  d.queueCreateInfoCount=1;d.pQueueCreateInfos=&q;
  d.enabledExtensionCount=std::uint32_t(enabled.size());
  d.ppEnabledExtensionNames=enabled.data();
  check(vkCreateDevice(physical,&d,nullptr,&device),"vkCreateDevice");
  vkGetDeviceQueue(device,queueIndex,0,&queue);
  VkCommandPoolCreateInfo p{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  p.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  p.queueFamilyIndex=queueIndex;
  check(vkCreateCommandPool(device,&p,nullptr,&pool),"create command pool");
  VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  a.commandPool=pool;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;a.commandBufferCount=1;
  check(vkAllocateCommandBuffers(device,&a,&cmd),"allocate command buffer");
 }
 std::uint32_t chooseMemory(std::uint32_t bits){
  VkPhysicalDeviceMemoryProperties props{};
  vkGetPhysicalDeviceMemoryProperties(physical,&props);
  for(std::uint32_t i=0;i<props.memoryTypeCount;++i)
   if((bits&(1u<<i))&&
      (props.memoryTypes[i].propertyFlags&
       (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
       (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    return i;
  throw std::runtime_error(
    "No Vulkan HOST_VISIBLE|HOST_COHERENT storage buffer memory. "
    "This benchmark cannot claim zero-copy access on this device.");
 }
 void buffer(VkDeviceSize bytes,VkBuffer& result,VkDeviceMemory& memory,void*& mapped){
  VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  b.size=bytes;b.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  b.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateBuffer(device,&b,nullptr,&result),"create storage buffer");
  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(device,result,&req);
  VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  a.allocationSize=req.size;a.memoryTypeIndex=chooseMemory(req.memoryTypeBits);
  check(vkAllocateMemory(device,&a,nullptr,&memory),"allocate storage memory");
  check(vkBindBufferMemory(device,result,memory,0),"bind storage buffer");
  check(vkMapMemory(device,memory,0,bytes,0,&mapped),"map storage memory");
 }
 void createPipeline(){
  std::ifstream file(CCP_PRESSURE_SPIRV,std::ios::binary|std::ios::ate);
  if(!file)throw std::runtime_error("Compiled Vulkan shader missing");
  const std::streamsize bytes=file.tellg();
  if(bytes<16||bytes%4)throw std::runtime_error("Invalid SPIR-V byte size");
  std::vector<std::uint32_t> spirv(std::size_t(bytes)/4,0);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(spirv.data()),bytes);
  if(!file)throw std::runtime_error("Truncated SPIR-V");
  VkShaderModuleCreateInfo sh{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  sh.codeSize=std::size_t(bytes);sh.pCode=spirv.data();
  check(vkCreateShaderModule(device,&sh,nullptr,&shader),"create SPIR-V module");
  VkDescriptorSetLayoutBinding bindings[2]{};
  for(auto i=0u;i<2;++i){
   bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
   bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
  }
  VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layout.bindingCount=2;layout.pBindings=bindings;
  check(vkCreateDescriptorSetLayout(device,&layout,nullptr,&descriptorLayout),"descriptor layout");
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,4};
  VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pi.setLayoutCount=1;pi.pSetLayouts=&descriptorLayout;
  pi.pushConstantRangeCount=1;pi.pPushConstantRanges=&push;
  check(vkCreatePipelineLayout(device,&pi,nullptr,&pipelineLayout),"pipeline layout");
  VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module=shader;stage.pName="main";
  VkComputePipelineCreateInfo p{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  p.stage=stage;p.layout=pipelineLayout;
  check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&p,nullptr,&pipeline),
        "create Vulkan compute pipeline");
  VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2};
  VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dp.maxSets=1;dp.poolSizeCount=1;dp.pPoolSizes=&ps;
  check(vkCreateDescriptorPool(device,&dp,nullptr,&descriptorPool),"descriptor pool");
  VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  ds.descriptorPool=descriptorPool;ds.descriptorSetCount=1;
  ds.pSetLayouts=&descriptorLayout;
  check(vkAllocateDescriptorSets(device,&ds,&descriptorSet),"descriptor set");
 }
 void createBuffers(std::uint32_t n){
  maxN=n;
  buffer(VkDeviceSize(n)*sizeof(Box),boxes,boxesMemory,boxesMapped);
  buffer(VkDeviceSize(n)*sizeof(Row),rows,rowsMemory,rowsMapped);
  const VkDescriptorBufferInfo inputs[2]{
   {boxes,0,VkDeviceSize(n)*sizeof(Box)},
   {rows,0,VkDeviceSize(n)*sizeof(Row)}
  };
  VkWriteDescriptorSet writes[2]{};
  for(std::uint32_t i=0;i<2;++i){
   writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   writes[i].dstSet=descriptorSet;
   writes[i].dstBinding=i;
   writes[i].descriptorCount=1;
   writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
   writes[i].pBufferInfo=&inputs[i];
  }
  vkUpdateDescriptorSets(device,2,writes,0,nullptr);
  VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  check(vkCreateFence(device,&f,nullptr,&fence),"create fence");
  timestampSupported=timestampBits>0;
  if(timestampSupported){
   VkQueryPoolCreateInfo q{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
   q.queryType=VK_QUERY_TYPE_TIMESTAMP;q.queryCount=2;
   if(vkCreateQueryPool(device,&q,nullptr,&timestamps)!=VK_SUCCESS)
    timestampSupported=false;
  }
 }
 // Hardware GPU time is the compute dispatch between TOP and BOTTOM command
 // timestamps. Host is submission and wait time; CPU preparation separate.
 std::pair<double,double> execute(const std::vector<Box>& positions){
  if(positions.size()!=maxN)throw std::runtime_error("Changed Vulkan input count");
  std::memcpy(boxesMapped,positions.data(),maxN*sizeof(Box));
  check(vkResetFences(device,1,&fence),"reset fence");
  check(vkResetCommandBuffer(cmd,0),"reset command buffer");
  VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  check(vkBeginCommandBuffer(cmd,&b),"begin Vulkan command buffer");
  if(timestampSupported){
   vkCmdResetQueryPool(cmd,timestamps,0,2);
   vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,0);
  }
  vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
  vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,
    pipelineLayout,0,1,&descriptorSet,0,nullptr);
  vkCmdPushConstants(cmd,pipelineLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&maxN);
  vkCmdDispatch(cmd,(maxN+127)/128,1,1);
  // Shader writes become visible to the host read AFTER the fence. Do not
  // rely on a driver-specific implicit memory visibility transition.
  VkMemoryBarrier visible{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  visible.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
  visible.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
       VK_PIPELINE_STAGE_HOST_BIT,0,1,&visible,0,nullptr,0,nullptr);
  if(timestampSupported)
   vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,1);
  check(vkEndCommandBuffer(cmd),"end command buffer");
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
  const auto start=Clock::now();
  check(vkQueueSubmit(queue,1,&submit,fence),"queue submit");
  check(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_C(120000000000)),
        "timeout waiting for bounded Vulkan query");
  const double host=elapsed(start);
  double gpu=-1.;
  if(timestampSupported){
   std::array<std::uint64_t,2> times{};
   if(vkGetQueryPoolResults(device,timestamps,0,2,sizeof(times),
     times.data(),sizeof(std::uint64_t),VK_QUERY_RESULT_64_BIT |
     VK_QUERY_RESULT_WAIT_BIT)==VK_SUCCESS && times[1]>=times[0])
    gpu=double(times[1]-times[0])*properties.limits.timestampPeriod/1.e6;
  }
  return {host,gpu};
 }
 Digest read(std::uint64_t& tested)const{
  const auto* r=static_cast<const Row*>(rowsMapped);
  Digest d;tested=0;
  for(std::size_t i=0;i<maxN;++i){
   d.count+=r[i].count;
   d.sum32+=r[i].sum32;
   d.xor32^=r[i].xor32;
   tested+=r[i].tests;
  }
  return d;
 }
};
}
int main(int argc,char** argv){
 try{
  if(argc!=9)throw std::invalid_argument(
    "Usage: ccp_cbp_pressure_vulkan OUT.csv SCENE N FRAMES SEED "
    "MAX_TESTS MAX_FRAME_MS MAX_GRID_ENTRIES");
  const fs::path output=argv[1];
  const std::string scene=argv[2];
  const auto n=number(argv[3],16,32768),frames=number(argv[4],1,240),
             seed=number(argv[5],1,2147483647),
             maxTests=number(argv[6],1000,600000000),
             frameMs=number(argv[7],20,120000),
             entries=number(argv[8],10000,20000000);
  if(!known(scene))throw std::invalid_argument("Invalid adversarial scene");
  if(fs::exists(output)||fs::exists(output.string()+".part"))
   throw std::runtime_error("Existing Vulkan evidence; refusing overwrite");
  if(!output.parent_path().empty())fs::create_directories(output.parent_path());
  Vulkan vk;
  vk.createInstance();
  vk.chooseDevice();
  vk.createDevice();
  vk.createPipeline();
  vk.createBuffers(std::uint32_t(n));
  std::ofstream csv(output.string()+".part");
  if(!csv)throw std::runtime_error("Cannot create Vulkan CSV");
  csv<<std::fixed<<std::setprecision(6);
  csv<<"scene,backend,n,frame,seed,vulkan_device,device_type,status,"
       "generate_ms,grid_build_ms,cpu_oracle_ms,host_submit_wait_ms,"
       "gpu_timestamp_ms,GPU_tests,GPU_candidates,CPU_grid_candidates,"
       "GPU_sum32,CPU_sum32,GPU_xor32,CPU_xor32,validation,"
       "pairs_materialized,output_buffer_bytes\n";
  std::cout<<"VULKAN DEVICE "<<vk.properties.deviceName<<" ("
           <<kind(vk.properties.deviceType)<<") driverVersion="
           <<vk.properties.driverVersion<<" timestampValidBits="
           <<vk.timestampBits<<"\n";
  const auto theoretical=n*(n-1)/2;
  for(std::uint64_t frame=0;frame<frames;++frame){
   const auto started=Clock::now();
   auto boxes=generate(std::size_t(n),std::uint32_t(frame),
       std::uint32_t(frames),std::uint32_t(seed),scene);
   const auto generateMs=elapsed(started);
   const auto gridStart=Clock::now();
   const auto grid=buildGrid(boxes,std::size_t(entries));
   const auto buildMs=elapsed(gridStart);
   const auto cpuStart=Clock::now();
   const auto truth=queryGrid(boxes,grid,maxTests,double(frameMs));
   const auto cpuMs=elapsed(cpuStart);
   if(n<=1024 && truth.complete && truth.digest!=brute(boxes))
    throw std::runtime_error("Independent small brute oracle failed");
   if(theoretical>maxTests){
    csv<<scene<<",vulkan_exhaustive,"<<n<<','<<frame<<','<<seed<<','
       <<'"'<<vk.properties.deviceName<<'"'<<','<<kind(vk.properties.deviceType)
       <<",BUDGET_SKIP,"<<generateMs<<','<<buildMs<<','<<cpuMs
       <<",0,0,0,0,"<<truth.digest.count
       <<",0,"<<truth.digest.sum32<<",0,"<<truth.digest.xor32
       <<",not_executed,0,0\n";
    continue;
   }
   auto [hostMs,gpuMs]=vk.execute(boxes);
   std::uint64_t checked=0;
   const auto got=vk.read(checked);
   if(checked!=theoretical)
    throw std::runtime_error("Vulkan shader did not visit all unordered pairs");
   const char* validation="UNVERIFIED";
   if(truth.complete){
    if(got.count!=truth.digest.count||
       got.sum32!=truth.digest.sum32||
       got.xor32!=truth.digest.xor32)
     throw std::runtime_error("Vulkan/independent CPU 3D grid pair-fingerprint mismatch");
    validation=n<=1024?"VULKAN_CPU_BRUTE":"VULKAN_CPU_GRID_DIGEST";
   }else if(scene=="all_overlap"){
    if(got.count!=theoretical)
     throw std::runtime_error("Vulkan known all-overlap count mismatch");
    validation="ANALYTIC_ALL_OVERLAP_COUNT_ONLY";
   }
   csv<<scene<<",vulkan_exhaustive,"<<n<<','<<frame<<','<<seed<<','
      <<'"'<<vk.properties.deviceName<<'"'<<','<<kind(vk.properties.deviceType)
      <<",COMPLETE,"<<generateMs<<','<<buildMs<<','<<cpuMs<<','
      <<hostMs<<','<<gpuMs<<','<<checked<<','<<got.count<<','
      <<(truth.complete?truth.digest.count:0)<<','<<got.sum32<<','
      <<(truth.complete?truth.digest.sum32:0)<<','<<got.xor32<<','
      <<(truth.complete?truth.digest.xor32:0)<<','<<validation
      <<",0,"<<n*sizeof(Row)<<"\n";
   std::cout<<"[VULKAN PRESSURE] "<<scene<<" frame "<<frame+1<<'/'<<frames
            <<" N="<<n<<" device="<<kind(vk.properties.deviceType)
            <<" full_pair_tests="<<checked<<" exact candidate COUNT="
            <<got.count<<" GPU_timestamp_ms="<<gpuMs
            <<" host_ms="<<hostMs<<" verification="<<validation<<"\n";
  }
  csv.flush();
  if(!csv)throw std::runtime_error("Incomplete Vulkan research CSV");
  csv.close();fs::rename(output.string()+".part",output);
  std::cout<<"PASS Vulkan compute-count stress; verify actual physical device "
           <<"type before claiming hardware acceleration. Count-only does "
           <<"NOT model pair-output bandwidth, CCD, or physics response.\n";
 }catch(const std::exception& e){
  std::cerr<<"VULKAN PRESSURE FAILED: "<<e.what()<<"\n";return 1;
 }
}

#import <Foundation/Foundation.h>
#include "metal_pipeline_preparation.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace starfox::render::shadows;
using namespace starfox::render::shadows::detail;
namespace {
std::atomic_bool blocked{true};
std::array<std::atomic_uint,4> calls{};
std::atomic_uint active{},maximum_active{};
std::thread::id owner;
id<MTLComputePipelineState> resource;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
template<class Predicate> void await(Predicate ready) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!ready()) {
        require(std::chrono::steady_clock::now()<deadline,"Timed out waiting for test compiler");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
id<MTLComputePipelineState> compile(id<MTLDevice> device,NativeMetalRtProgram program) {
    require(device!=nil,"Compiler lost retained device");
    require(std::this_thread::get_id()!=owner,"Compilation ran on owner/UI thread");
    const auto index=static_cast<unsigned>(program);
    ++calls[index];
    const unsigned concurrent=++active;
    maximum_active.store(std::max(maximum_active.load(),concurrent));
    struct Done {~Done(){--active;}} done;
    while(blocked.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(program==NativeMetalRtProgram::reflection) throw std::runtime_error("sentinel compiler failure");
    if(program==NativeMetalRtProgram::empty_reflection) return nil;
    return resource;
}
}
int main() { @autoreleasepool { try {
    owner=std::this_thread::get_id();
    // Intentionally fake retained objects: these tests cover the actual ARC,
    // dispatch and publication implementation without requiring an RT GPU.
    // They are NOT shader-output or physical Mac acceptance tests.
    id<MTLDevice> device=(id<MTLDevice>)[NSObject new];
    resource=(id<MTLComputePipelineState>)[NSObject new];
    auto first=metal_pipeline_preparation(device);
    auto second=metal_pipeline_preparation(device);
    require(first==second,"Mono/stereo owners did not share their device cache");
    require(!first->pending(),"Unrequested pipelines were compiled eagerly");
    auto pending=first->request(NativeMetalRtProgram::shadow,compile);
    require(pending.phase==MetalPipelinePhase::preparing && !pending.pipeline,"Pending compiler published a resource");
    await([]{return calls[0].load()==1;});
    for(unsigned i=0;i<1000;++i) {
        auto repeated=second->request(NativeMetalRtProgram::shadow,compile);
        require(repeated.phase==MetalPipelinePhase::preparing && !repeated.pipeline,"Unfinished result escaped");
    }
    require(calls[0]==1 && first->pending(),"Pending requests launched duplicate compilers");
    // Compiler owns only the separate cache/device, not a destroyed presenter.
    std::weak_ptr<MetalPipelinePreparation> retained=first;
    first.reset();second.reset();
    require(!retained.expired(),"Owner release destroyed a pending compiler's storage");
    auto resumed=metal_pipeline_preparation(device);
    require(resumed==retained.lock(),"Recreated owner duplicated a pending pipeline");
    id<MTLDevice> other_device=(id<MTLDevice>)[NSObject new];
    auto other=metal_pipeline_preparation(other_device);
    require(other!=resumed && !other->pending(),"Another device inherited old pipeline state");
    other->request(NativeMetalRtProgram::shadow,compile);
    resumed->request(NativeMetalRtProgram::empty_shadow,compile);
    resumed->request(NativeMetalRtProgram::reflection,compile);
    resumed->request(NativeMetalRtProgram::empty_reflection,compile);
    require(calls[1]==0 && calls[2]==0 && calls[3]==0,"Queued compilers ran concurrently");
    blocked=false;
    await([&]{return !resumed->pending() && !other->pending();});
    auto ready=resumed->request(NativeMetalRtProgram::shadow,compile);
    require(ready.phase==MetalPipelinePhase::ready && ready.pipeline==resource,"Ready result was lost");
    require(resumed->request(NativeMetalRtProgram::empty_shadow,compile).pipeline==resource,"Empty-scene pipeline was lost");
    auto failed=resumed->request(NativeMetalRtProgram::reflection,compile);
    require(failed.phase==MetalPipelinePhase::failed && !failed.pipeline
        && failed.error=="sentinel compiler failure","Compiler error was lost or treated as ready");
    auto empty=resumed->request(NativeMetalRtProgram::empty_reflection,compile);
    require(empty.phase==MetalPipelinePhase::failed && !empty.pipeline && !empty.error.empty(),"Nil compiler result was accepted");
    for(unsigned i=0;i<1000;++i) resumed->request(NativeMetalRtProgram::reflection,compile);
    require(calls[0]==2 && calls[1]==1 && calls[2]==1 && calls[3]==1,"Completed/failed work was retried per frame");
    require(maximum_active==1,"Native pipeline compilation was not serialized");
    resumed.reset();other.reset();
    await([&]{return retained.expired();});
    std::cout<<"PASS: nonblocking polling, shared cache, serial compilation, failure/nil caching, device isolation, pending-owner release; fake Metal objects only\n";
    return 0;
} catch(const std::exception& error) {
    blocked=false;
    std::cerr<<error.what()<<'\n';return 1;
} }}

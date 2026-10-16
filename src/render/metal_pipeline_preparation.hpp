#pragma once
#import <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include "starfox/render/native_metal_rt_programs.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace starfox::render::shadows::detail {

enum class MetalPipelinePhase { idle, preparing, ready, failed };
struct MetalPipelineSnapshot {
    MetalPipelinePhase phase{};
    id<MTLComputePipelineState> pipeline{nil};
    std::string error;
};

// CPU compilation only: no SDL object, command buffer, presenter or Impl is
// borrowed by a job. A pending job retains this cache and its Metal device,
// so closing/recreating the renderer never joins a compiler or leaves a
// callback pointing into a destroyed renderer. Results are published once.
class MetalPipelinePreparation : public std::enable_shared_from_this<MetalPipelinePreparation> {
    struct Entry {
        std::atomic<MetalPipelinePhase> phase{MetalPipelinePhase::idle};
        id<MTLComputePipelineState> pipeline{nil};
        std::string error;
    };
    id<MTLDevice> device_;
    std::array<Entry,4> entries_;
    static dispatch_queue_t compiler_queue() {
        // Reflection compilation is memory hungry. Serialize the four kinds
        // across mono/stereo/motion owners instead of spawning compilers for
        // each eye or compiling all of them at once.
        static dispatch_queue_t queue=dispatch_queue_create(
            "com.starfox.enhanced.metal-pipelines",
            dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,QOS_CLASS_UTILITY,0));
        return queue;
    }
public:
    using Compiler=id<MTLComputePipelineState> (*)(id<MTLDevice>,NativeMetalRtProgram);
    explicit MetalPipelinePreparation(id<MTLDevice> device):device_(device) {}
    id<MTLDevice> device() const noexcept {return device_;}
    MetalPipelineSnapshot request(NativeMetalRtProgram program,Compiler compile) {
        const auto index=static_cast<unsigned>(program);
        if(index>=entries_.size() || !compile) throw std::runtime_error("Invalid Metal pipeline request");
        auto& entry=entries_[index];
        auto expected=MetalPipelinePhase::idle;
        if(entry.phase.compare_exchange_strong(expected,MetalPipelinePhase::preparing)) {
            auto owner=shared_from_this();
            dispatch_async(compiler_queue(),^{ @autoreleasepool {
                auto& pending=owner->entries_[index];
                try {
                    pending.pipeline=compile(owner->device_,program);
                    if(!pending.pipeline) throw std::runtime_error("Metal pipeline compiler returned no resource");
                    pending.phase.store(MetalPipelinePhase::ready,std::memory_order_release);
                } catch(const std::exception& error) {
                    pending.error=error.what();
                    pending.phase.store(MetalPipelinePhase::failed,std::memory_order_release);
                } catch(...) {
                    pending.error="Unknown Metal pipeline preparation failure";
                    pending.phase.store(MetalPipelinePhase::failed,std::memory_order_release);
                }
            }});
        }
        const auto phase=entry.phase.load(std::memory_order_acquire);
        // Never touch result/error while the compiler might still write them.
        if(phase==MetalPipelinePhase::ready) return {phase,entry.pipeline,{}};
        if(phase==MetalPipelinePhase::failed) return {phase,nil,entry.error};
        return {phase,nil,{}};
    }
    bool pending() const noexcept {
        for(const auto& entry:entries_)
            if(entry.phase.load(std::memory_order_acquire)==MetalPipelinePhase::preparing) return true;
        return false;
    }
};

inline std::shared_ptr<MetalPipelinePreparation> metal_pipeline_preparation(id<MTLDevice> device) {
    if(!device) throw std::runtime_error("Missing Metal pipeline device");
    static std::mutex mutex;
    static std::vector<std::weak_ptr<MetalPipelinePreparation>> caches;
    std::lock_guard lock(mutex);
    for(auto at=caches.begin();at!=caches.end();) {
        if(auto cache=at->lock()) {
            if(cache->device()==device) return cache;
            ++at;
        } else at=caches.erase(at);
    }
    auto cache=std::make_shared<MetalPipelinePreparation>(device);
    caches.push_back(cache);
    return cache;
}

} // namespace starfox::render::shadows::detail

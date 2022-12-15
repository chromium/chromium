// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_provider.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/config/skia_limits.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "ipc/common/surface_handle.h"
#include "ipc/raster_in_process_context.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace ui {

// static
scoped_refptr<InProcessContextProvider>
InProcessContextProvider::CreateOffscreen(
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool is_worker) {
  gpu::ContextCreationAttribs attribs;
  attribs.alpha_size = 8;
  attribs.blue_size = 8;
  attribs.green_size = 8;
  attribs.red_size = 8;
  attribs.depth_size = 0;
  attribs.stencil_size = 8;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;
  attribs.enable_raster_interface = true;
  attribs.enable_gles2_interface = !is_worker;
  attribs.enable_oop_rasterization = is_worker;
  return new InProcessContextProvider(attribs, gpu_memory_buffer_manager,
                                      is_worker);
}

InProcessContextProvider::InProcessContextProvider(
    const gpu::ContextCreationAttribs& attribs,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool support_locking)
    : support_locking_(support_locking), attribs_(attribs) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  context_thread_checker_.DetachFromThread();
}

InProcessContextProvider::~InProcessContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

void InProcessContextProvider::AddRef() const {
  base::RefCountedThreadSafe<InProcessContextProvider>::AddRef();
}

void InProcessContextProvider::Release() const {
  base::RefCountedThreadSafe<InProcessContextProvider>::Release();
}

gpu::ContextResult InProcessContextProvider::BindToCurrentSequence() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (bind_tried_)
    return bind_result_;
  bind_tried_ = true;

  auto* holder = viz::TestGpuServiceHolder::GetInstance();

  if (attribs_.enable_oop_rasterization) {
    DCHECK(!attribs_.enable_gles2_interface);
    DCHECK(!attribs_.enable_grcontext);

    raster_context_ = std::make_unique<gpu::RasterInProcessContext>();
    bind_result_ = raster_context_->Initialize(
        holder->task_executor(), attribs_, gpu::SharedMemoryLimits(),
        holder->gpu_service()->gr_shader_cache(), nullptr);

    impl_base_ = raster_context_->GetImplementation();
  } else {
    gles2_context_ = std::make_unique<gpu::GLInProcessContext>();
    bind_result_ = gles2_context_->Initialize(
        viz::TestGpuServiceHolder::GetInstance()->task_executor(), attribs_,
        gpu::SharedMemoryLimits());

    impl_base_ = gles2_context_->GetImplementation();
  }

  if (bind_result_ != gpu::ContextResult::kSuccess)
    return bind_result_;

  cache_controller_ = std::make_unique<viz::ContextCacheController>(
      impl_base_, base::SingleThreadTaskRunner::GetCurrentDefault());
  if (support_locking_)
    cache_controller_->SetLock(GetLock());

  if (gles2_context_) {
    gles2_raster_impl_ =
        std::make_unique<gpu::raster::RasterImplementationGLES>(
            ContextGL(), ContextSupport());
  }

  return bind_result_;
}

const gpu::Capabilities& InProcessContextProvider::ContextCapabilities() const {
  CheckValidThreadOrLockAcquired();
  return impl_base_->capabilities();
}

const gpu::GpuFeatureInfo& InProcessContextProvider::GetGpuFeatureInfo() const {
  CheckValidThreadOrLockAcquired();

  return gles2_context_ ? gles2_context_->GetGpuFeatureInfo()
                        : raster_context_->GetGpuFeatureInfo();
}

gpu::gles2::GLES2Interface* InProcessContextProvider::ContextGL() {
  CheckValidThreadOrLockAcquired();
  if (!gles2_context_)
    return nullptr;

  return gles2_context_->GetImplementation();
}

gpu::raster::RasterInterface* InProcessContextProvider::RasterInterface() {
  CheckValidThreadOrLockAcquired();
  return raster_context_ ? raster_context_->GetImplementation()
                         : gles2_raster_impl_.get();
}

gpu::ContextSupport* InProcessContextProvider::ContextSupport() {
  return impl_base_;
}

class GrDirectContext* InProcessContextProvider::GrContext() {
  CheckValidThreadOrLockAcquired();

  if (attribs_.enable_oop_rasterization)
    return nullptr;

  if (gr_context_)
    return gr_context_->get();

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  gpu::DefaultGrCacheLimitsForTests(&max_resource_cache_bytes,
                                    &max_glyph_cache_texture_bytes);
  gr_context_ = std::make_unique<skia_bindings::GrContextForGLES2Interface>(
      ContextGL(), ContextSupport(), ContextCapabilities(),
      max_resource_cache_bytes, max_glyph_cache_texture_bytes);
  cache_controller_->SetGrContext(gr_context_->get());

  return gr_context_->get();
}

gpu::SharedImageInterface* InProcessContextProvider::SharedImageInterface() {
  return gles2_context_ ? gles2_context_->GetSharedImageInterface()
                        : raster_context_->GetSharedImageInterface();
}

viz::ContextCacheController* InProcessContextProvider::CacheController() {
  CheckValidThreadOrLockAcquired();
  return cache_controller_.get();
}

base::Lock* InProcessContextProvider::GetLock() {
  if (!support_locking_)
    return nullptr;
  return &context_lock_;
}

void InProcessContextProvider::AddObserver(viz::ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void InProcessContextProvider::RemoveObserver(viz::ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

void InProcessContextProvider::SendOnContextLost() {
  for (auto& observer : observers_)
    observer.OnContextLost();
}

void InProcessContextProvider::CheckValidThreadOrLockAcquired() const {
#if DCHECK_IS_ON()
  if (support_locking_) {
    context_lock_.AssertAcquired();
  } else {
    DCHECK(context_thread_checker_.CalledOnValidThread());
  }
#endif
}

}  // namespace ui

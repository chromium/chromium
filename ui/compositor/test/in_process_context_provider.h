// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
#define UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/raster_in_process_context.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GLInProcessContext;
class GpuMemoryBufferManager;
class ImplementationBase;
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace ui {

// TODO(crbug.com/1292507): Merge into viz::TestInProcessContextProvider once
// on-screen context support is no longer needed.
class InProcessContextProvider
    : public base::RefCountedThreadSafe<InProcessContextProvider>,
      public viz::ContextProvider,
      public viz::RasterContextProvider {
 public:
  // Uses default attributes for creating an offscreen context. If `is_worker`
  // is true then the context will support locking and OOP-R (through
  // RasterInterface) and won't support GLES2 or GrContext.
  static scoped_refptr<InProcessContextProvider> CreateOffscreen(
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      bool is_worker);

  InProcessContextProvider(const InProcessContextProvider&) = delete;
  InProcessContextProvider& operator=(const InProcessContextProvider&) = delete;

  // viz::ContextProvider / viz::RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentSequence() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrDirectContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  viz::ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  void AddObserver(viz::ContextLostObserver* obs) override;
  void RemoveObserver(viz::ContextLostObserver* obs) override;

  // Calls OnContextLost() on all observers. This doesn't modify the context.
  void SendOnContextLost();

 private:
  friend class base::RefCountedThreadSafe<InProcessContextProvider>;

  InProcessContextProvider(
      const gpu::ContextCreationAttribs& attribs,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      bool support_locking);
  ~InProcessContextProvider() override;

  void CheckValidThreadOrLockAcquired() const;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  std::unique_ptr<gpu::GLInProcessContext> gles2_context_;
  std::unique_ptr<gpu::RasterInProcessContext> raster_context_;
  raw_ptr<gpu::ImplementationBase> impl_base_ = nullptr;

  // Initialized only when `gles2_context_` is used.
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<gpu::raster::RasterInterface> gles2_raster_impl_;

  std::unique_ptr<viz::ContextCacheController> cache_controller_;

  const bool support_locking_;
  bool bind_tried_ = false;
  gpu::ContextResult bind_result_;

  gpu::ContextCreationAttribs attribs_;

  base::Lock context_lock_;

  base::ObserverList<viz::ContextLostObserver>::Unchecked observers_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

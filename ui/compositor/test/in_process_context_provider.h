// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
#define UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GLInProcessContext;
class GpuMemoryBufferManager;
class ImageFactory;
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace ui {

class InProcessContextProvider
    : public base::RefCountedThreadSafe<InProcessContextProvider>,
      public viz::ContextProvider,
      public viz::RasterContextProvider {
 public:
  static scoped_refptr<InProcessContextProvider> Create(
      const gpu::ContextCreationAttribs& attribs,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      gpu::SurfaceHandle window,
      const std::string& debug_name,
      bool support_locking);

  // Uses default attributes for creating an offscreen context.
  static scoped_refptr<InProcessContextProvider> CreateOffscreen(
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      bool support_locking);

  // viz::ContextProvider / viz::RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentThread() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  viz::ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  void AddObserver(viz::ContextLostObserver* obs) override;
  void RemoveObserver(viz::ContextLostObserver* obs) override;

  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // on the default framebuffer.
  uint32_t GetCopyTextureInternalFormat();

  // Calls OnContextLost() on all observers. This doesn't modify the context.
  void SendOnContextLost();

 private:
  friend class base::RefCountedThreadSafe<InProcessContextProvider>;

  InProcessContextProvider(
      const gpu::ContextCreationAttribs& attribs,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      gpu::SurfaceHandle window,
      const std::string& debug_name,
      bool support_locking);
  ~InProcessContextProvider() override;

  void CheckValidThreadOrLockAcquired() const {
#if DCHECK_IS_ON()
    if (support_locking_) {
      context_lock_.AssertAcquired();
    } else {
      DCHECK(context_thread_checker_.CalledOnValidThread());
    }
#endif
  }

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  std::unique_ptr<gpu::GLInProcessContext> context_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<gpu::raster::RasterInterface> raster_context_;
  std::unique_ptr<viz::ContextCacheController> cache_controller_;

  const bool support_locking_ ALLOW_UNUSED_TYPE;
  bool bind_tried_ = false;
  gpu::ContextResult bind_result_;

  gpu::ContextCreationAttribs attribs_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  gpu::ImageFactory* image_factory_;
  gpu::SurfaceHandle window_;
  std::string debug_name_;

  base::Lock context_lock_;

  base::ObserverList<viz::ContextLostObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(InProcessContextProvider);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

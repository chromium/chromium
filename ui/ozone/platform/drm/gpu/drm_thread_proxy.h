// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"
#include "ui/ozone/public/mojom/device_cursor.mojom.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

class DrmWindowProxy;
class InterThreadMessagingProxy;

// Mediates the communication between GPU main/compositor/IO threads and the DRM
// thread. It serves proxy objects that are safe to call on the GPU threads. The
// proxy objects then deal with safely posting the messages to the DRM thread.
class DrmThreadProxy {
 public:
  using OverlayCapabilitiesCallback =
      base::OnceCallback<void(gfx::AcceleratedWidget,
                              const std::vector<OverlaySurfaceCandidate>&,
                              const std::vector<OverlayStatus>&)>;

  DrmThreadProxy();
  ~DrmThreadProxy();

  void BindThreadIntoMessagingProxy(InterThreadMessagingProxy* messaging_proxy);

  void StartDrmThread(base::OnceClosure receiver_drainer);

  std::unique_ptr<DrmWindowProxy> CreateDrmWindowProxy(
      gfx::AcceleratedWidget widget);

  void CreateBuffer(gfx::AcceleratedWidget widget,
                    const gfx::Size& size,
                    gfx::BufferFormat format,
                    gfx::BufferUsage usage,
                    uint32_t flags,
                    std::unique_ptr<GbmBuffer>* buffer,
                    scoped_refptr<DrmFramebuffer>* framebuffer);

  using CreateBufferAsyncCallback =
      base::OnceCallback<void(std::unique_ptr<GbmBuffer>,
                              scoped_refptr<DrmFramebuffer>)>;
  void CreateBufferAsync(gfx::AcceleratedWidget widget,
                         const gfx::Size& size,
                         gfx::BufferFormat format,
                         gfx::BufferUsage usage,
                         uint32_t flags,
                         CreateBufferAsyncCallback callback);

  void CreateBufferFromHandle(gfx::AcceleratedWidget widget,
                              const gfx::Size& size,
                              gfx::BufferFormat format,
                              gfx::NativePixmapHandle handle,
                              std::unique_ptr<GbmBuffer>* buffer,
                              scoped_refptr<DrmFramebuffer>* framebuffer);

  // Sets a callback that will be notified when display configuration may have
  // changed to clear the overlay configuration cache. |callback| will be run on
  // origin thread.
  void SetClearOverlayCacheCallback(base::RepeatingClosure reset_callback);

  // Checks if overlay |candidates| can be displayed asynchronously and then
  // runs |callback|. Testing the overlay configuration requires posting a task
  // to the DRM thread, but |callback| will be run on origin thread.
  void CheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlaySurfaceCandidate>& candidates,
      OverlayCapabilitiesCallback callback);

  void AddDrmDeviceReceiver(
      mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver);

 private:
  DrmThread drm_thread_;

  DISALLOW_COPY_AND_ASSIGN(DrmThreadProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_

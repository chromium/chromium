// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"
#include "ui/ozone/platform/drm/mojom/device_cursor.mojom.h"
#include "ui/ozone/public/drm_modifiers_filter.h"
#include "ui/ozone/public/hardware_capabilities.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

class DrmWindowProxy;
class InterThreadMessagingProxy;

// Mediates the communication between GPU main/compositor/IO threads and the DRM
// thread. It serves proxy objects that are safe to call on the GPU threads. The
// proxy objects then deal with safely posting the messages to the DRM thread.
class DrmThreadProxy {
 public:
  DrmThreadProxy();

  DrmThreadProxy(const DrmThreadProxy&) = delete;
  DrmThreadProxy& operator=(const DrmThreadProxy&) = delete;

  ~DrmThreadProxy();

  void BindThreadIntoMessagingProxy(InterThreadMessagingProxy* messaging_proxy);

  void StartDrmThread(base::OnceClosure receiver_drainer);

  std::unique_ptr<DrmWindowProxy> CreateDrmWindowProxy(
      gfx::AcceleratedWidget widget);

  void CreateBuffer(gfx::AcceleratedWidget widget,
                    const gfx::Size& size,
                    const gfx::Size& framebuffer_size,
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
  // changed, so we should update state for managing overlays.
  // |callback| will be run on origin thread.
  void SetDisplaysConfiguredCallback(base::RepeatingClosure callback);

  // Checks if overlay |candidates| can be displayed asynchronously and then
  // runs |callback|. Testing the overlay configuration requires posting a task
  // to the DRM thread, but |callback| will be run on origin thread.
  void CheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlaySurfaceCandidate>& candidates,
      DrmThread::OverlayCapabilitiesCallback callback);

  // Similar to CheckOverlayCapabilities() but returns the result synchronously.
  std::vector<OverlayStatus> CheckOverlayCapabilitiesSync(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlaySurfaceCandidate>& candidates);

  void GetHardwareCapabilities(
      gfx::AcceleratedWidget widget,
      const HardwareCapabilitiesCallback& receive_callback);

  void AddDrmDeviceReceiver(
      mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver);

  bool WaitUntilDrmThreadStarted();
  scoped_refptr<base::SingleThreadTaskRunner> GetDrmThreadTaskRunner();

  // Passes a DRM modifiers filter through to the DRM thread.
  void SetDrmModifiersFilter(std::unique_ptr<DrmModifiersFilter> filter);

 private:
  DrmThread drm_thread_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_PROXY_H_

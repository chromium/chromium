// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"
#include "ui/ozone/public/interfaces/drm_device.mojom.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace base {
class FilePath;
}

namespace display {
class DisplayMode;
struct GammaRampRGBEntry;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {

class DrmDeviceManager;
class DrmFramebuffer;
class DrmGpuDisplayManager;
class GbmBuffer;
class ScreenManager;

struct DrmOverlayPlane;

// Holds all the DRM related state and performs all DRM related operations.
//
// The DRM thread is used to insulate DRM operations from potential blocking
// behaviour on the GPU main thread in order to reduce the potential for jank
// (for example jank in the cursor if the GPU main thread is performing heavy
// operations). The inverse is also true as blocking operations on the DRM
// thread (such as modesetting) no longer block the GPU main thread.
class DrmThread : public base::Thread,
                  public ozone::mojom::DeviceCursor,
                  public ozone::mojom::DrmDevice {
 public:
  DrmThread();
  ~DrmThread() override;

  void Start(base::OnceClosure binding_completer);

  // Must be called on the DRM thread. All methods for use from the GPU thread.
  // DrmThreadProxy (on GPU)thread) is the client for these methods.
  void CreateBuffer(gfx::AcceleratedWidget widget,
                    const gfx::Size& size,
                    gfx::BufferFormat format,
                    gfx::BufferUsage usage,
                    uint32_t flags,
                    std::unique_ptr<GbmBuffer>* buffer,
                    scoped_refptr<DrmFramebuffer>* framebuffer);
  void CreateBufferFromFds(gfx::AcceleratedWidget widget,
                           const gfx::Size& size,
                           gfx::BufferFormat format,
                           std::vector<base::ScopedFD> fds,
                           const std::vector<gfx::NativePixmapPlane>& planes,
                           std::unique_ptr<GbmBuffer>* buffer,
                           scoped_refptr<DrmFramebuffer>* framebuffer);
  void GetScanoutFormats(gfx::AcceleratedWidget widget,
                         std::vector<gfx::BufferFormat>* scanout_formats);
  void AddBindingCursorDevice(ozone::mojom::DeviceCursorRequest request);
  void AddBindingDrmDevice(ozone::mojom::DrmDeviceRequest request);

  // DrmWindowProxy (on GPU thread) is the client for these methods.
  void SchedulePageFlip(gfx::AcceleratedWidget widget,
                        std::vector<DrmOverlayPlane> planes,
                        SwapCompletionOnceCallback submission_callback,
                        PresentationOnceCallback presentation_callback);

  void IsDeviceAtomic(gfx::AcceleratedWidget widget, bool* is_atomic);

  // ozone::mojom::DrmDevice
  void StartDrmDevice(StartDrmDeviceCallback callback) override;
  void CreateWindow(gfx::AcceleratedWidget widget) override;
  void DestroyWindow(gfx::AcceleratedWidget widget) override;
  void SetWindowBounds(gfx::AcceleratedWidget widget,
                       const gfx::Rect& bounds) override;
  void TakeDisplayControl(base::OnceCallback<void(bool)> callback) override;
  void RelinquishDisplayControl(
      base::OnceCallback<void(bool)> callback) override;
  void RefreshNativeDisplays(
      base::OnceCallback<void(MovableDisplaySnapshots)> callback) override;
  void AddGraphicsDevice(const base::FilePath& path, base::File file) override;
  void RemoveGraphicsDevice(const base::FilePath& path) override;
  void DisableNativeDisplay(
      int64_t id,
      base::OnceCallback<void(int64_t, bool)> callback) override;
  void ConfigureNativeDisplay(
      int64_t id,
      std::unique_ptr<display::DisplayMode> mode,
      const gfx::Point& origin,
      base::OnceCallback<void(int64_t, bool)> callback) override;
  void GetHDCPState(int64_t display_id,
                    base::OnceCallback<void(int64_t, bool, display::HDCPState)>
                        callback) override;
  void SetHDCPState(int64_t display_id,
                    display::HDCPState state,
                    base::OnceCallback<void(int64_t, bool)> callback) override;
  void SetColorMatrix(int64_t display_id,
                      const std::vector<float>& color_matrix) override;
  void SetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut) override;
  void CheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const OverlaySurfaceCandidateList& overlays,
      base::OnceCallback<void(gfx::AcceleratedWidget,
                              const OverlaySurfaceCandidateList&,
                              const OverlayStatusList&)> callback) override;

  // ozone::mojom::DeviceCursor
  void SetCursor(gfx::AcceleratedWidget widget,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& location,
                 int32_t frame_delay_ms) override;
  void MoveCursor(gfx::AcceleratedWidget widget,
                  const gfx::Point& location) override;

  // base::Thread:
  void Init() override;

 private:
  void OnPlanesReadyForPageFlip(gfx::AcceleratedWidget widget,
                                SwapCompletionOnceCallback submission_callback,
                                PresentationOnceCallback presentation_callback,
                                std::vector<DrmOverlayPlane> planes);

  std::unique_ptr<DrmDeviceManager> device_manager_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmGpuDisplayManager> display_manager_;

  base::OnceClosure complete_early_binding_requests_;

  // The mojo implementation requires a BindingSet because the DrmThread serves
  // requests from two different client threads.
  mojo::BindingSet<ozone::mojom::DeviceCursor> cursor_bindings_;

  // The mojo implementation of DrmDevice requires a BindingSet because the
  // DrmThread services requests from different client threads when operating in
  // mus mode
  mojo::BindingSet<ozone::mojom::DrmDevice> drm_bindings_;

  base::WeakPtrFactory<DrmThread> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DrmThread);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

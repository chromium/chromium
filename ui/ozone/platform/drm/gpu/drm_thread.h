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
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/public/mojom/device_cursor.mojom.h"
#include "ui/ozone/public/mojom/drm_device.mojom.h"
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

  void Start(base::OnceClosure receiver_completer,
             std::unique_ptr<DrmDeviceGenerator> device_generator);

  // Runs |task| once a DrmDevice is registered and |window| was created via
  // CreateWindow(). |done| will be signaled if it's not null.
  void RunTaskAfterWindowReady(gfx::AcceleratedWidget window,
                               base::OnceClosure task,
                               base::WaitableEvent* done);

  // Must be called on the DRM thread. All methods for use from the GPU thread.
  // DrmThreadProxy (on GPU)thread) is the client for these methods.
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
  void SetClearOverlayCacheCallback(base::RepeatingClosure callback);
  void AddDrmDeviceReceiver(
      mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver);

  // DrmWindowProxy (on GPU thread) is the client for these methods.
  void SchedulePageFlip(gfx::AcceleratedWidget widget,
                        std::vector<DrmOverlayPlane> planes,
                        SwapCompletionOnceCallback submission_callback,
                        PresentationOnceCallback presentation_callback);

  void IsDeviceAtomic(gfx::AcceleratedWidget widget, bool* is_atomic);

  // ozone::mojom::DrmDevice
  void CreateWindow(gfx::AcceleratedWidget widget,
                    const gfx::Rect& initial_bounds) override;
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
  void GetDeviceCursor(
      mojo::PendingAssociatedReceiver<ozone::mojom::DeviceCursor> receiver)
      override;

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
  struct TaskInfo {
    base::OnceClosure task;
    base::WaitableEvent* done;

    TaskInfo(base::OnceClosure task, base::WaitableEvent* done);
    TaskInfo(TaskInfo&& other);
    ~TaskInfo();
  };

  void OnPlanesReadyForPageFlip(gfx::AcceleratedWidget widget,
                                SwapCompletionOnceCallback submission_callback,
                                PresentationOnceCallback presentation_callback,
                                std::vector<DrmOverlayPlane> planes);

  // Called when a DrmDevice or DrmWindow is created. Runs tasks that are now
  // unblocked.
  void ProcessPendingTasks();

  std::unique_ptr<DrmDeviceManager> device_manager_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmGpuDisplayManager> display_manager_;

  base::OnceClosure complete_early_receiver_requests_;

  // The mojo implementation requires an AssociatedReceiverSet because the
  // DrmThread serves requests from two different client threads.
  mojo::AssociatedReceiverSet<ozone::mojom::DeviceCursor> cursor_receivers_;

  // This is a ReceiverSet because the regular Receiver causes the sequence
  // checker in InterfaceEndpointClient to fail during teardown.
  // TODO(samans): Figure out why.
  mojo::ReceiverSet<ozone::mojom::DrmDevice> drm_receivers_;

  // The AcceleratedWidget from the last call to CreateWindow.
  gfx::AcceleratedWidget last_created_window_ = gfx::kNullAcceleratedWidget;

  // The tasks that are blocked on a DrmDevice and a certain AcceleratedWidget
  // becoming available.
  base::flat_map<gfx::AcceleratedWidget, std::vector<TaskInfo>> pending_tasks_;

  // Holds the DrmDeviceGenerator that DrmDeviceManager will use. Will be passed
  // on to DrmDeviceManager after the thread starts.
  std::unique_ptr<DrmDeviceGenerator> device_generator_;

  base::WeakPtrFactory<DrmThread> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DrmThread);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

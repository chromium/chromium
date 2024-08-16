// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/mojom/device_cursor.mojom.h"
#include "ui/ozone/platform/drm/mojom/drm_device.mojom.h"
#include "ui/ozone/public/drm_modifiers_filter.h"
#include "ui/ozone/public/hardware_capabilities.h"
#include "ui/ozone/public/overlay_surface_candidate.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace base {
class FilePath;
}  // namespace base

namespace display {
struct ColorCalibration;
struct ColorTemperatureAdjustment;
struct DisplayConfigurationParams;
struct GammaAdjustment;
}  // namespace display

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

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
  using OverlayCapabilitiesCallback =
      base::OnceCallback<void(gfx::AcceleratedWidget,
                              const std::vector<OverlaySurfaceCandidate>&,
                              const std::vector<OverlayStatus>&)>;

  DrmThread();

  DrmThread(const DrmThread&) = delete;
  DrmThread& operator=(const DrmThread&) = delete;

  ~DrmThread() override;

  void Start(base::OnceClosure receiver_completer,
             std::unique_ptr<DrmDeviceGenerator> device_generator);

  // Runs |task| once a DrmDevice is registered. |done|
  // will be signaled if it's not null.
  void RunTaskAfterDeviceReady(base::OnceClosure task,
                               base::WaitableEvent* done);

  // Must be called on the DRM thread. All methods for use from the GPU thread.
  // DrmThreadProxy (on GPU)thread) is the client for these methods.
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
  void SetDisplaysConfiguredCallback(base::RepeatingClosure callback);
  void AddDrmDeviceReceiver(
      mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver);

  // Verifies if the display controller can successfully scanout the given set
  // of OverlaySurfaceCandidates and return the status associated with each
  // candidate.
  void CheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlaySurfaceCandidate>& candidates,
      OverlayCapabilitiesCallback callback);

  // Similar to CheckOverlayCapabilities() but stores the result in |result|
  // instead of running a callback.
  void CheckOverlayCapabilitiesSync(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlaySurfaceCandidate>& candidates,
      std::vector<OverlayStatus>* result);
  // Calls `receive_callback` with a `HardwareCapabilities` containing
  // information about overlay support on the current hardware.
  void GetHardwareCapabilities(gfx::AcceleratedWidget widget,
                               HardwareCapabilitiesCallback receive_callback);

  // DrmWindowProxy (on GPU thread) is the client for these methods.
  void SchedulePageFlip(gfx::AcceleratedWidget widget,
                        std::vector<DrmOverlayPlane> planes,
                        SwapCompletionOnceCallback submission_callback,
                        PresentationOnceCallback presentation_callback);

  void IsDeviceAtomic(gfx::AcceleratedWidget widget, bool* is_atomic);

  // Sets a filter that the DRM thread can invoke to filter out modifiers
  // incompatible with use in GPU main and Viz threads.
  void SetDrmModifiersFilter(std::unique_ptr<DrmModifiersFilter> filter);

  // ozone::mojom::DrmDevice
  void CreateWindow(gfx::AcceleratedWidget widget,
                    const gfx::Rect& initial_bounds) override;
  void DestroyWindow(gfx::AcceleratedWidget widget) override;
  void SetWindowBounds(gfx::AcceleratedWidget widget,
                       const gfx::Rect& bounds) override;
  void TakeDisplayControl(base::OnceCallback<void(bool)> callback) override;
  void RelinquishDisplayControl(
      base::OnceCallback<void(bool)> callback) override;
  void ShouldDisplayEventTriggerConfiguration(
      const EventPropertyMap& event_props,
      base::OnceCallback<void(bool)> callback) override;
  void RefreshNativeDisplays(
      base::OnceCallback<void(MovableDisplaySnapshots)> callback) override;
  void AddGraphicsDevice(const base::FilePath& path,
                         mojo::PlatformHandle fd_mojo_handle) override;
  void RemoveGraphicsDevice(const base::FilePath& path) override;
  void ConfigureNativeDisplays(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      display::ModesetFlags modeset_flags,
      ConfigureNativeDisplaysCallback callback) override;
  void SetHdcpKeyProp(int64_t display_id,
                      const std::string& key,
                      SetHdcpKeyPropCallback callback) override;
  void GetHDCPState(int64_t display_id,
                    base::OnceCallback<void(int64_t,
                                            bool,
                                            display::HDCPState,
                                            display::ContentProtectionMethod)>
                        callback) override;
  void SetHDCPState(int64_t display_id,
                    display::HDCPState state,
                    display::ContentProtectionMethod protection_method,
                    base::OnceCallback<void(int64_t, bool)> callback) override;
  void SetColorTemperatureAdjustment(
      int64_t display_id,
      const display::ColorTemperatureAdjustment& cta) override;
  void SetColorCalibration(
      int64_t display_id,
      const display::ColorCalibration& calibration) override;
  void SetGammaAdjustment(int64_t display_id,
                          const display::GammaAdjustment& adjustment) override;
  void SetPrivacyScreen(int64_t display_id,
                        bool enabled,
                        base::OnceCallback<void(bool)> callback) override;
  void GetSeamlessRefreshRates(
      int64_t display_id,
      GetSeamlessRefreshRatesCallback callback) override;

  void GetDeviceCursor(
      mojo::PendingAssociatedReceiver<ozone::mojom::DeviceCursor> receiver)
      override;

  // ozone::mojom::DeviceCursor
  void SetCursor(gfx::AcceleratedWidget widget,
                 const std::vector<SkBitmap>& bitmaps,
                 const std::optional<gfx::Point>& location,
                 base::TimeDelta frame_delay) override;
  void MoveCursor(gfx::AcceleratedWidget widget,
                  const gfx::Point& location) override;

  // base::Thread:
  void Init() override;
  void CleanUp() override;

 private:
  struct TaskInfo {
    base::OnceClosure task;
    raw_ptr<base::WaitableEvent> done;

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

  // The tasks that are blocked on a DrmDevice becoming available.
  std::vector<TaskInfo> pending_tasks_;

  // Holds the DrmDeviceGenerator that DrmDeviceManager will use. Will be passed
  // on to DrmDeviceManager after the thread starts.
  std::unique_ptr<DrmDeviceGenerator> device_generator_;

  base::WeakPtrFactory<DrmThread> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

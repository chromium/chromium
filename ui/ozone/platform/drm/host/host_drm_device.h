// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/mojom/device_cursor.mojom.h"
#include "ui/ozone/public/mojom/drm_device.mojom.h"

namespace display {
class DisplaySnapshot;
}

namespace ui {
class DrmDisplayHostManager;
class DrmOverlayManagerHost;
class GpuThreadObserver;
class DrmDeviceConnector;
class HostCursorProxy;

// This is the Viz host-side library for the DRM device service provided by the
// viz process.
class HostDrmDevice : public base::RefCountedThreadSafe<HostDrmDevice>,
                      public GpuThreadAdapter {
 public:
  explicit HostDrmDevice(DrmCursor* cursor);

  void ProvideManagers(DrmDisplayHostManager* display_manager,
                       DrmOverlayManagerHost* overlay_manager);

  void OnGpuServiceLaunchedOnIOThread(
      mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner);

  // Invoked by DrmDeviceConnector on loss of GPU service.
  void OnGpuServiceLost();

  // GpuThreadAdapter
  void AddGpuThreadObserver(GpuThreadObserver* observer) override;
  void RemoveGpuThreadObserver(GpuThreadObserver* observer) override;
  bool IsConnected() override;

  // Services needed for DrmDisplayHostMananger.
  void RegisterHandlerForDrmDisplayHostManager(
      DrmDisplayHostManager* handler) override;
  void UnRegisterHandlerForDrmDisplayHostManager() override;

  bool GpuTakeDisplayControl() override;
  bool GpuRefreshNativeDisplays() override;
  bool GpuRelinquishDisplayControl() override;
  bool GpuAddGraphicsDeviceOnUIThread(const base::FilePath& path,
                                      base::ScopedFD fd) override;
  void GpuAddGraphicsDeviceOnIOThread(const base::FilePath& path,
                                      base::ScopedFD fd) override;
  bool GpuRemoveGraphicsDevice(const base::FilePath& path) override;

  // Services needed for DrmOverlayManagerHost.
  void RegisterHandlerForDrmOverlayManager(
      DrmOverlayManagerHost* handler) override;
  void UnRegisterHandlerForDrmOverlayManager() override;
  bool GpuCheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const OverlaySurfaceCandidateList& new_params) override;

  // Services needed by DrmDisplayHost
  bool GpuConfigureNativeDisplay(int64_t display_id,
                                 const ui::DisplayMode_Params& display_mode,
                                 const gfx::Point& point) override;
  bool GpuDisableNativeDisplay(int64_t display_id) override;
  bool GpuGetHDCPState(int64_t display_id) override;
  bool GpuSetHDCPState(int64_t display_id, display::HDCPState state) override;
  bool GpuSetColorMatrix(int64_t display_id,
                         const std::vector<float>& color_matrix) override;
  bool GpuSetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut) override;

  // Services needed by DrmWindowHost
  bool GpuDestroyWindow(gfx::AcceleratedWidget widget) override;
  bool GpuCreateWindow(gfx::AcceleratedWidget widget,
                       const gfx::Rect& initial_bounds) override;
  bool GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                              const gfx::Rect& bounds) override;

 private:
  friend class base::RefCountedThreadSafe<HostDrmDevice>;
  ~HostDrmDevice() override;

  void HostOnGpuServiceLaunched();

  void OnDrmServiceStarted();

  // TODO(rjkroege): Get rid of the need for this method in a subsequent CL.
  void PollForSingleThreadReady(int previous_delay);

  void GpuCheckOverlayCapabilitiesCallback(
      gfx::AcceleratedWidget widget,
      const OverlaySurfaceCandidateList& overlays,
      const OverlayStatusList& returns) const;

  void GpuConfigureNativeDisplayCallback(int64_t display_id,
                                         bool success) const;

  void GpuRefreshNativeDisplaysCallback(
      std::vector<std::unique_ptr<display::DisplaySnapshot>> displays) const;
  void GpuDisableNativeDisplayCallback(int64_t display_id, bool success) const;
  void GpuTakeDisplayControlCallback(bool success) const;
  void GpuRelinquishDisplayControlCallback(bool success) const;
  void GpuGetHDCPStateCallback(int64_t display_id,
                               bool success,
                               display::HDCPState state) const;
  void GpuSetHDCPStateCallback(int64_t display_id, bool success) const;

  void OnGpuServiceLaunchedOnUIThread(
      mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device);

  // Mojo implementation of the DrmDevice. Will be bound on the "main" thread.
  mojo::Remote<ui::ozone::mojom::DrmDevice> drm_device_;
  mojo::Remote<ui::ozone::mojom::DrmDevice> drm_device_on_io_thread_;

  DrmDisplayHostManager* display_manager_;  // Not owned.
  DrmOverlayManagerHost* overlay_manager_;  // Not owned.
  DrmCursor* const cursor_;                 // Not owned.

  std::unique_ptr<HostCursorProxy> cursor_proxy_;

  THREAD_CHECKER(on_io_thread_);  // Needs to be rebound as is allocated on the
                                  // UI thread.
  THREAD_CHECKER(on_ui_thread_);

  bool connected_ = false;
  base::ObserverList<GpuThreadObserver>::Unchecked gpu_thread_observers_;

  DISALLOW_COPY_AND_ASSIGN(HostDrmDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_

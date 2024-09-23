// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"
#include "ui/ozone/platform/drm/mojom/device_cursor.mojom.h"
#include "ui/ozone/platform/drm/mojom/drm_device.mojom.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {
class DrmDisplayHostManager;
class GpuThreadObserver;
class DrmDeviceConnector;
class HostCursorProxy;

// This is the Viz host-side library for the DRM device service provided by the
// viz process.
class HostDrmDevice : public base::RefCountedThreadSafe<HostDrmDevice>,
                      public GpuThreadAdapter {
 public:
  explicit HostDrmDevice(DrmCursor* cursor);

  HostDrmDevice(const HostDrmDevice&) = delete;
  HostDrmDevice& operator=(const HostDrmDevice&) = delete;

  void SetDisplayManager(DrmDisplayHostManager* display_manager);

  void OnGpuServiceLaunched(
      mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device);

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
  void GpuAddGraphicsDevice(const base::FilePath& path,
                            base::ScopedFD fd) override;
  bool GpuRemoveGraphicsDevice(const base::FilePath& path) override;
  void GpuShouldDisplayEventTriggerConfiguration(
      const EventPropertyMap& event_props) override;

  // Services needed by DrmDisplayHost
  void GpuConfigureNativeDisplays(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      display::ConfigureCallback callback,
      display::ModesetFlags modeset_flags) override;
  bool GpuSetHdcpKeyProp(int64_t display_id, const std::string& key) override;
  bool GpuGetHDCPState(int64_t display_id) override;
  bool GpuSetHDCPState(
      int64_t display_id,
      display::HDCPState state,
      display::ContentProtectionMethod protection_method) override;
  void GpuSetColorTemperatureAdjustment(
      int64_t display_id,
      const display::ColorTemperatureAdjustment& cta) override;
  void GpuSetColorCalibration(
      int64_t display_id,
      const display::ColorCalibration& calibration) override;
  void GpuSetGammaAdjustment(
      int64_t display_id,
      const display::GammaAdjustment& adjustment) override;
  void GpuSetPrivacyScreen(int64_t display_id,
                           bool enabled,
                           display::SetPrivacyScreenCallback callback) override;
  void GpuGetSeamlessRefreshRates(
      int64_t display_id,
      display::GetSeamlessRefreshRatesCallback callback) override;

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

  void GpuRefreshNativeDisplaysCallback(MovableDisplaySnapshots displays) const;
  void GpuTakeDisplayControlCallback(bool success) const;
  void GpuRelinquishDisplayControlCallback(bool success) const;
  void GpuShouldDisplayEventTriggerConfigurationCallback(
      bool should_trigger) const;
  void GpuSetHdcpKeyPropCallback(int64_t display_id, bool success) const;
  void GpuGetHDCPStateCallback(
      int64_t display_id,
      bool success,
      display::HDCPState state,
      display::ContentProtectionMethod protection_method) const;
  void GpuSetHDCPStateCallback(int64_t display_id, bool success) const;

  // Mojo implementation of the DrmDevice. Will be bound on the "main" thread.
  mojo::Remote<ui::ozone::mojom::DrmDevice> drm_device_;

  raw_ptr<DrmDisplayHostManager> display_manager_;  // Not owned.
  const raw_ptr<DrmCursor> cursor_;                 // Not owned.

  std::unique_ptr<HostCursorProxy> cursor_proxy_;

  THREAD_CHECKER(on_ui_thread_);

  bool connected_ = false;
  base::ObserverList<GpuThreadObserver>::Unchecked gpu_thread_observers_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_GPU_THREAD_ADAPTER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_GPU_THREAD_ADAPTER_H_

#include "base/files/scoped_file.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/common/display_types.h"

namespace base {
class FilePath;
}

namespace ui {

class DrmDisplayHostManager;
class GpuThreadObserver;

// Provides the services that the various host components need
// to use either a GPU process or thread for their implementation.
class GpuThreadAdapter {
 public:
  virtual ~GpuThreadAdapter() = default;

  virtual bool IsConnected() = 0;
  virtual void AddGpuThreadObserver(GpuThreadObserver* observer) = 0;
  virtual void RemoveGpuThreadObserver(GpuThreadObserver* observer) = 0;

  // Methods for Display management.
  virtual void RegisterHandlerForDrmDisplayHostManager(
      DrmDisplayHostManager* handler) = 0;
  virtual void UnRegisterHandlerForDrmDisplayHostManager() = 0;

  // Services needed for DrmDisplayHostMananger
  virtual bool GpuTakeDisplayControl() = 0;
  virtual bool GpuRefreshNativeDisplays() = 0;
  virtual bool GpuRelinquishDisplayControl() = 0;
  virtual void GpuAddGraphicsDevice(const base::FilePath& path,
                                    base::ScopedFD fd) = 0;
  virtual bool GpuRemoveGraphicsDevice(const base::FilePath& path) = 0;
  virtual void GpuShouldDisplayEventTriggerConfiguration(
      const EventPropertyMap& event_props) = 0;

  // Services needed by DrmDisplayHost
  virtual void GpuConfigureNativeDisplays(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      display::ConfigureCallback callback,
      display::ModesetFlags modeset_flags) = 0;
  virtual bool GpuSetHdcpKeyProp(int64_t display_id,
                                 const std::string& key) = 0;
  virtual bool GpuGetHDCPState(int64_t display_id) = 0;
  virtual bool GpuSetHDCPState(
      int64_t display_id,
      display::HDCPState state,
      display::ContentProtectionMethod protection_method) = 0;
  virtual void GpuSetColorTemperatureAdjustment(
      int64_t display_id,
      const display::ColorTemperatureAdjustment& cta) = 0;
  virtual void GpuSetColorCalibration(
      int64_t display_id,
      const display::ColorCalibration& calibration) = 0;
  virtual void GpuSetGammaAdjustment(
      int64_t display_id,
      const display::GammaAdjustment& adjustment) = 0;
  virtual void GpuSetPrivacyScreen(
      int64_t display_id,
      bool enabled,
      display::SetPrivacyScreenCallback callback) = 0;

  virtual void GpuGetSeamlessRefreshRates(
      int64_t display_id,
      display::GetSeamlessRefreshRatesCallback callback) = 0;

  // Services needed by DrmWindowHost
  virtual bool GpuDestroyWindow(gfx::AcceleratedWidget widget) = 0;
  virtual bool GpuCreateWindow(gfx::AcceleratedWidget widget,
                               const gfx::Rect& initial_bounds) = 0;
  virtual bool GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                      const gfx::Rect& bounds) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_GPU_THREAD_ADAPTER_H_

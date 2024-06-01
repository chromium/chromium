// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"

namespace display {
class DisplaySnapshot;
}

namespace ui {

class GpuThreadAdapter;

class DrmDisplayHost : public GpuThreadObserver {
 public:
  DrmDisplayHost(GpuThreadAdapter* sender,
                 std::unique_ptr<display::DisplaySnapshot> params,
                 bool is_dummy);

  DrmDisplayHost(const DrmDisplayHost&) = delete;
  DrmDisplayHost& operator=(const DrmDisplayHost&) = delete;

  ~DrmDisplayHost() override;

  display::DisplaySnapshot* snapshot() const { return snapshot_.get(); }
  bool is_dummy() const { return is_dummy_; }

  void UpdateDisplaySnapshot(std::unique_ptr<display::DisplaySnapshot> params);
  void SetHdcpKeyProp(const std::string& key,
                      display::SetHdcpKeyPropCallback callback);
  void GetHDCPState(display::GetHDCPStateCallback callback);
  void SetHDCPState(display::HDCPState state,
                    display::ContentProtectionMethod protection_method,
                    display::SetHDCPStateCallback callback);
  void SetColorTemperatureAdjustment(
      const display::ColorTemperatureAdjustment& cta);
  void SetColorCalibration(const display::ColorCalibration& calibration);
  void SetGammaAdjustment(const display::GammaAdjustment& adjustment);
  void SetPrivacyScreen(bool enabled,
                        display::SetPrivacyScreenCallback callback);
  void GetSeamlessRefreshRates(
      display::GetSeamlessRefreshRatesCallback callback) const;

  // Called when the IPC from the GPU process arrives to answer the above
  // commands.
  void OnHdcpKeyPropSetReceived(bool success);
  void OnHDCPStateReceived(bool status,
                           display::HDCPState state,
                           display::ContentProtectionMethod protection_method);
  void OnHDCPStateUpdated(bool status);

  // GpuThreadObserver:
  void OnGpuProcessLaunched() override;
  void OnGpuThreadReady() override;
  void OnGpuThreadRetired() override;

 private:
  // Calls all the callbacks with failure.
  void ClearCallbacks();

  const raw_ptr<GpuThreadAdapter> sender_;  // Not owned.

  std::unique_ptr<display::DisplaySnapshot> snapshot_;

  // Used during startup to signify that any display configuration should be
  // synchronous and succeed.
  bool is_dummy_;

  display::SetHdcpKeyPropCallback set_hdcp_key_prop_callback_;
  display::GetHDCPStateCallback get_hdcp_callback_;
  display::SetHDCPStateCallback set_hdcp_callback_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_

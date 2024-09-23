// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/display/types/native_display_delegate.h"

namespace ui {

class DrmDisplayHostManager;

class DrmNativeDisplayDelegate : public display::NativeDisplayDelegate {
 public:
  explicit DrmNativeDisplayDelegate(DrmDisplayHostManager* display_manager);

  DrmNativeDisplayDelegate(const DrmNativeDisplayDelegate&) = delete;
  DrmNativeDisplayDelegate& operator=(const DrmNativeDisplayDelegate&) = delete;

  ~DrmNativeDisplayDelegate() override;

  void OnConfigurationChanged();
  void OnDisplaySnapshotsInvalidated();

  // display::NativeDisplayDelegate overrides:
  void Initialize() override;
  void TakeDisplayControl(display::DisplayControlCallback callback) override;
  void RelinquishDisplayControl(
      display::DisplayControlCallback callback) override;
  void GetDisplays(display::GetDisplaysCallback callback) override;
  void Configure(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      display::ConfigureCallback callback,
      display::ModesetFlags modeset_flags) override;
  void SetHdcpKeyProp(int64_t display_id,
                      const std::string& key,
                      display::SetHdcpKeyPropCallback callback) override;
  void GetHDCPState(const display::DisplaySnapshot& output,
                    display::GetHDCPStateCallback callback) override;
  void SetHDCPState(const display::DisplaySnapshot& output,
                    display::HDCPState state,
                    display::ContentProtectionMethod protection_method,
                    display::SetHDCPStateCallback callback) override;
  void SetColorTemperatureAdjustment(
      int64_t display_id,
      const display::ColorTemperatureAdjustment& cta) override;
  void SetColorCalibration(
      int64_t display_id,
      const display::ColorCalibration& calibration) override;
  void SetGammaAdjustment(int64_t display_id,
                          const display::GammaAdjustment& gamma) override;
  void SetPrivacyScreen(int64_t display_id,
                        bool enabled,
                        display::SetPrivacyScreenCallback callback) override;
  void GetSeamlessRefreshRates(
      int64_t display_id,
      display::GetSeamlessRefreshRatesCallback callback) const override;

  void AddObserver(display::NativeDisplayObserver* observer) override;
  void RemoveObserver(display::NativeDisplayObserver* observer) override;
  display::FakeDisplayController* GetFakeDisplayController() override;

 private:
  const raw_ptr<DrmDisplayHostManager> display_manager_;  // Not owned.

  base::ObserverList<display::NativeDisplayObserver>::Unchecked observers_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_

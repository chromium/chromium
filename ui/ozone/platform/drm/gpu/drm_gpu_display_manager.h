// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/types/display_constants.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"

using drmModeModeInfo = struct _drmModeModeInfo;

namespace display {
struct ColorCalibration;
struct ColorTemperatureAdjustment;
struct DisplayConfigurationParams;
struct GammaAdjustment;
}  // namespace display

namespace ui {

class DrmDeviceManager;
class DrmDisplay;
class ScreenManager;

class DrmGpuDisplayManager {
 public:
  DrmGpuDisplayManager(ScreenManager* screen_manager,
                       DrmDeviceManager* drm_device_manager);

  DrmGpuDisplayManager(const DrmGpuDisplayManager&) = delete;
  DrmGpuDisplayManager& operator=(const DrmGpuDisplayManager&) = delete;

  ~DrmGpuDisplayManager();

  // Sets a callback that will be notified when display configuration may have
  // changed, so we should update state for managing overlays.
  void SetDisplaysConfiguredCallback(base::RepeatingClosure callback);

  // Returns a list of the connected displays. When this is called the list of
  // displays is refreshed.
  MovableDisplaySnapshots GetDisplays();

  // Takes/releases the control of the DRM devices.
  bool TakeDisplayControl();
  void RelinquishDisplayControl();

  // Whether or not a udev display change event triggered by a DRM property
  // should go through or get blocked.
  bool ShouldDisplayEventTriggerConfiguration(
      const EventPropertyMap& event_props);

  bool ConfigureDisplays(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      display::ModesetFlags modeset_flags,
      std::vector<display::DisplayConfigurationParams>& out_requests);
  bool SetHdcpKeyProp(int64_t display_id, const std::string& key);
  bool GetHDCPState(int64_t display_id,
                    display::HDCPState* state,
                    display::ContentProtectionMethod* protection_method);
  bool SetHDCPState(int64_t display_id,
                    display::HDCPState state,
                    display::ContentProtectionMethod protection_method);
  void SetColorTemperatureAdjustment(
      int64_t display_id,
      const display::ColorTemperatureAdjustment& cta);
  void SetColorCalibration(int64_t display_id,
                           const display::ColorCalibration& calibration);
  void SetGammaAdjustment(int64_t display_id,
                          const display::GammaAdjustment& adjustment);
  void SetBackgroundColor(int64_t display_id, const uint64_t background_color);
  bool SetPrivacyScreen(int64_t display_id, bool enabled);
  std::optional<std::vector<float>> GetSeamlessRefreshRates(
      int64_t display_id) const;

 private:
  friend class DrmGpuDisplayManagerTest;

  DrmDisplay* FindDisplay(int64_t display_id) const;
  DrmDisplay* FindDisplayByConnectorId(uint32_t connector_id) const;

  // Notify ScreenManager of all the displays that were present before the
  // update but are gone after the update.
  void NotifyScreenManager(
      const std::vector<std::unique_ptr<DrmDisplay>>& new_displays,
      const std::vector<std::unique_ptr<DrmDisplay>>& old_displays) const;

  // Test modesets with |controllers_to_configure|, but with all
  // possible permutations of CRTC-connector pairings. Returns true if one of
  // the permutation leads to a successful test modest.
  bool RetryTestConfigureDisplaysWithAlternateCrtcs(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      const std::vector<ControllerConfigParams>& controllers_to_configure);

  // Replace the CRTC of all displays and display controllers specified in
  // |controllers_to_configure| by its connector with their new CRTC.
  bool UpdateDisplaysWithNewCrtcs(
      const std::vector<ControllerConfigParams>& controllers_to_configure);

  // Get the display state associated with |config_requests| if there was a
  // successful test configuration before the commit modeset call.
  std::vector<ControllerConfigParams> GetLatestModesetTestConfig(
      const std::vector<display::DisplayConfigurationParams>& config_requests);

  // Finds a mode that matches the size and timing specified by |request_mode|
  // and returns an owned copy. Prioritizes choosing modes natively belonging to
  // |display|, and attempts panel-fitting from |all_displays| if needed. If
  // |is_seamless| is true, performs additional verification that the returned
  // mode can be configured seamlessly. Returns nullptr if no matching mode was
  // found.
  std::unique_ptr<drmModeModeInfo> FindModeForDisplay(
      const display::DisplayMode& request_mode,
      const DrmDisplay& display,
      bool is_seamless);

  const raw_ptr<ScreenManager> screen_manager_;         // Not owned.
  const raw_ptr<DrmDeviceManager> drm_device_manager_;  // Not owned.

  std::vector<std::unique_ptr<DrmDisplay>> displays_;

  base::RepeatingClosure displays_configured_callback_;

  // A map of successful test display config request and the config params. The
  // map is cleared on every commit modeset, or whenever the displays and
  // controllers are recreated in GetDisplays().
  base::flat_map<std::string, std::vector<ControllerConfigParams>>
      successful_test_config_params_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_

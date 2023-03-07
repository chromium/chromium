// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"
#include "ui/ozone/platform/drm/common/display_types.h"

using drmModeModeInfo = struct _drmModeModeInfo;

namespace display {
struct GammaRampRGBEntry;
}  // namespace display

namespace gfx {
class ColorSpace;
}  // namespace gfx

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
      uint32_t modeset_flag);
  bool SetHdcpKeyProp(int64_t display_id, const std::string& key);
  bool GetHDCPState(int64_t display_id,
                    display::HDCPState* state,
                    display::ContentProtectionMethod* protection_method);
  bool SetHDCPState(int64_t display_id,
                    display::HDCPState state,
                    display::ContentProtectionMethod protection_method);
  void SetColorMatrix(int64_t display_id,
                      const std::vector<float>& color_matrix);
  void SetBackgroundColor(int64_t display_id, const uint64_t background_color);
  void SetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut);
  bool SetPrivacyScreen(int64_t display_id, bool enabled);

  void SetColorSpace(int64_t crtc_id, const gfx::ColorSpace& color_space);

 private:
  friend class DrmGpuDisplayManagerTest;

  DrmDisplay* FindDisplay(int64_t display_id);

  // Notify ScreenManager of all the displays that were present before the
  // update but are gone after the update.
  void NotifyScreenManager(
      const std::vector<std::unique_ptr<DrmDisplay>>& new_displays,
      const std::vector<std::unique_ptr<DrmDisplay>>& old_displays) const;

  ScreenManager* const screen_manager_;         // Not owned.
  DrmDeviceManager* const drm_device_manager_;  // Not owned.

  std::vector<std::unique_ptr<DrmDisplay>> displays_;

  base::RepeatingClosure displays_configured_callback_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_

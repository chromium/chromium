// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/common/display_types.h"

namespace display {
class DisplayMode;
struct GammaRampRGBEntry;
}

namespace ui {

class DrmDeviceManager;
class DrmDisplay;
class ScreenManager;

class DrmGpuDisplayManager {
 public:
  DrmGpuDisplayManager(ScreenManager* screen_manager,
                       DrmDeviceManager* drm_device_manager);
  ~DrmGpuDisplayManager();

  // Sets a callback that will be notified when display configuration may have
  // changed to clear the overlay configuration cache.
  void SetClearOverlayCacheCallback(base::RepeatingClosure callback);

  // Returns a list of the connected displays. When this is called the list of
  // displays is refreshed.
  MovableDisplaySnapshots GetDisplays();

  // Takes/releases the control of the DRM devices.
  bool TakeDisplayControl();
  void RelinquishDisplayControl();

  bool ConfigureDisplay(int64_t id,
                        const display::DisplayMode& display_mode,
                        const gfx::Point& origin);
  bool DisableDisplay(int64_t id);
  bool GetHDCPState(int64_t display_id, display::HDCPState* state);
  bool SetHDCPState(int64_t display_id, display::HDCPState state);
  void SetColorMatrix(int64_t display_id,
                      const std::vector<float>& color_matrix);
  void SetBackgroundColor(int64_t display_id,
                          const uint64_t background_color);
  void SetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut);

 private:
  DrmDisplay* FindDisplay(int64_t display_id);

  // Notify ScreenManager of all the displays that were present before the
  // update but are gone after the update.
  void NotifyScreenManager(
      const std::vector<std::unique_ptr<DrmDisplay>>& new_displays,
      const std::vector<std::unique_ptr<DrmDisplay>>& old_displays) const;

  ScreenManager* const screen_manager_;         // Not owned.
  DrmDeviceManager* const drm_device_manager_;  // Not owned.

  std::vector<std::unique_ptr<DrmDisplay>> displays_;

  base::RepeatingClosure clear_overlay_cache_callback_;

  DISALLOW_COPY_AND_ASSIGN(DrmGpuDisplayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_H_

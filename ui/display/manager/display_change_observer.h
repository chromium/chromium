// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_CHANGE_OBSERVER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_CHANGE_OBSERVER_H_

#include <stdint.h>

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace gfx {
class RoundedCornersF;
}

namespace display {

class DisplayManager;
class DisplaySnapshot;

// An object that observes changes in display configuration and updates
// DisplayManager.
class DISPLAY_MANAGER_EXPORT DisplayChangeObserver
    : public DisplayConfigurator::StateController,
      public DisplayConfigurator::Observer,
      public ui::InputDeviceEventObserver {
 public:
  // Returns the mode list for internal display.
  DISPLAY_EXPORT static ManagedDisplayInfo::ManagedDisplayModeList
  GetInternalManagedDisplayModeList(const ManagedDisplayInfo& display_info,
                                    const DisplaySnapshot& output);

  // Returns the resolution list.
  DISPLAY_EXPORT static ManagedDisplayInfo::ManagedDisplayModeList
  GetExternalManagedDisplayModeList(const DisplaySnapshot& output);

  explicit DisplayChangeObserver(DisplayManager* display_manager);

  DisplayChangeObserver(const DisplayChangeObserver&) = delete;
  DisplayChangeObserver& operator=(const DisplayChangeObserver&) = delete;

  ~DisplayChangeObserver() override;

  // DisplayConfigurator::StateController overrides:
  MultipleDisplayState GetStateForDisplayIds(
      const DisplayConfigurator::DisplayStateList& outputs) override;
  bool GetSelectedModeForDisplayId(int64_t display_id,
                                   ManagedDisplayMode* out_mode) const override;

  // Overridden from DisplayConfigurator::Observer:
  void OnDisplayConfigurationChanged(
      const DisplayConfigurator::DisplayStateList& outputs) override;
  void OnDisplayConfigurationChangeFailed(
      const DisplayConfigurator::DisplayStateList& displays,
      MultipleDisplayState failed_new_state) override;

  // Overridden from ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // Static methods exposed for testing.
  DISPLAY_EXPORT static float FindDeviceScaleFactor(
      float dpi,
      const gfx::Size& size_in_pixels);

  static ManagedDisplayInfo CreateManagedDisplayInfo(
      const DisplaySnapshot* snapshot,
      const DisplayMode* mode_info,
      bool native,
      float device_scale_factor,
      float dpi,
      const std::string& name,
      const gfx::RoundedCornersF& panel_radii = gfx::RoundedCornersF());

 private:
  friend class DisplayChangeObserverTestBase;

  void UpdateInternalDisplay(
      const DisplayConfigurator::DisplayStateList& display_states);

  ManagedDisplayInfo CreateManagedDisplayInfoInternal(
      const DisplaySnapshot* snapshot,
      const DisplayMode* mode_info);

  // The panel radii of the internal display that is specified via command-line
  // switch `display::switches::kDisplayProperties`.
  std::optional<gfx::RoundedCornersF> internal_panel_radii_;

  // |display_manager_| is not owned and must outlive DisplayChangeObserver.
  raw_ptr<DisplayManager> display_manager_;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_CHANGE_OBSERVER_H_

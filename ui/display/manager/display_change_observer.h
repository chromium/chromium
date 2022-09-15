// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_CHANGE_OBSERVER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_CHANGE_OBSERVER_H_

#include <stdint.h>

#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/events/devices/input_device_event_observer.h"

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

  // Overriden from DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const DisplayConfigurator::DisplayStateList& outputs) override;
  void OnDisplayModeChangeFailed(
      const DisplayConfigurator::DisplayStateList& displays,
      MultipleDisplayState failed_new_state) override;

  // Overriden from ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // Exposed for testing.
  DISPLAY_EXPORT static float FindDeviceScaleFactor(
      float dpi,
      const gfx::Size& size_in_pixels);

 private:
  friend class DisplayChangeObserverTest;

  void UpdateInternalDisplay(
      const DisplayConfigurator::DisplayStateList& display_states);

  ManagedDisplayInfo CreateManagedDisplayInfo(const DisplaySnapshot* snapshot,
                                              const DisplayMode* mode_info);

  // |display_manager_| is not owned and must outlive DisplayChangeObserver.
  DisplayManager* display_manager_;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_CHANGE_OBSERVER_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_MANAGER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_MANAGER_H_

#include <vector>

#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/display_constants.h"

namespace display {

struct DisplayConfigureRequest;
class DisplaySnapshot;

class DisplayLayoutManager {
 public:
  virtual ~DisplayLayoutManager() {}

  virtual DisplayConfigurator::SoftwareMirroringController*
  GetSoftwareMirroringController() const = 0;

  virtual DisplayConfigurator::StateController* GetStateController() const = 0;

  // Returns the current display state.
  virtual MultipleDisplayState GetDisplayState() const = 0;

  // Returns the current power state.
  virtual chromeos::DisplayPowerState GetPowerState() const = 0;

  // Based on the given |displays|, display state and power state, it will
  // create display configuration requests which will then be used to
  // configure the hardware. The requested configuration is stored in
  // |requests|.
  virtual bool GetDisplayLayout(
      const std::vector<DisplaySnapshot*>& displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      RefreshRateThrottleState new_throttle_state,
      bool new_vrr_enabled_state,
      std::vector<DisplayConfigureRequest>* requests) const = 0;

  virtual std::vector<DisplaySnapshot*> GetDisplayStates() const = 0;

  virtual bool IsMirroring() const = 0;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_MANAGER_H_

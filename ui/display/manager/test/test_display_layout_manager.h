// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_
#define UI_DISPLAY_MANAGER_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_layout_manager.h"

namespace display::test {

class TestDisplayLayoutManager : public DisplayLayoutManager {
 public:
  TestDisplayLayoutManager(
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
      MultipleDisplayState display_state);

  TestDisplayLayoutManager(const TestDisplayLayoutManager&) = delete;
  TestDisplayLayoutManager& operator=(const TestDisplayLayoutManager&) = delete;

  ~TestDisplayLayoutManager() override;

  void set_displays(
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>&
          displays) {
    displays_ = displays;
  }

  void set_display_state(MultipleDisplayState display_state) {
    display_state_ = display_state;
  }

  // DisplayLayoutManager:
  DisplayConfigurator::StateController* GetStateController() const override;
  DisplayConfigurator::SoftwareMirroringController*
  GetSoftwareMirroringController() const override;
  MultipleDisplayState GetDisplayState() const override;
  chromeos::DisplayPowerState GetPowerState() const override;
  bool GetDisplayLayout(
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      const base::flat_set<int64_t>& new_vrr_enabled_state,
      std::vector<DisplayConfigureRequest>* requests) const override;
  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> GetDisplayStates()
      const override;
  bool IsMirroring() const override;

 private:
  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> displays_;
  MultipleDisplayState display_state_;
};

}  // namespace display::test

#endif  // UI_DISPLAY_MANAGER_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_

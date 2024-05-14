// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/test_display_layout_manager.h"

#include "base/memory/raw_ptr.h"
#include "ui/display/types/display_snapshot.h"

namespace display::test {

TestDisplayLayoutManager::TestDisplayLayoutManager(
    const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
    MultipleDisplayState display_state)
    : displays_(displays), display_state_(display_state) {}

TestDisplayLayoutManager::~TestDisplayLayoutManager() {}

DisplayConfigurator::StateController*
TestDisplayLayoutManager::GetStateController() const {
  return nullptr;
}

DisplayConfigurator::SoftwareMirroringController*
TestDisplayLayoutManager::GetSoftwareMirroringController() const {
  return nullptr;
}

MultipleDisplayState TestDisplayLayoutManager::GetDisplayState() const {
  return display_state_;
}

chromeos::DisplayPowerState TestDisplayLayoutManager::GetPowerState() const {
  NOTREACHED_IN_MIGRATION();
  return chromeos::DISPLAY_POWER_ALL_ON;
}

bool TestDisplayLayoutManager::GetDisplayLayout(
    const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    const base::flat_set<int64_t>& new_vrr_enabled_state,
    std::vector<DisplayConfigureRequest>* requests) const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>
TestDisplayLayoutManager::GetDisplayStates() const {
  return displays_;
}

bool TestDisplayLayoutManager::IsMirroring() const {
  return display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR;
}

}  // namespace display::test

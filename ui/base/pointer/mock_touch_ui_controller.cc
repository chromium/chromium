// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/mock_touch_ui_controller.h"

namespace ui {

MockTouchUiController::MockTouchUiController(
    ::ui::TouchUiController::TouchUiState touch_ui_state)
    : ::ui::TouchUiController(touch_ui_state) {}

MockTouchUiController::~MockTouchUiController() = default;

#if BUILDFLAG(USE_BLINK)
void MockTouchUiController::SetMockConnectedPointerDevices(
    const std::vector<PointerDevice>& devices) {
  mock_connected_pointer_devices_ = devices;
}

int MockTouchUiController::MaxTouchPoints() const {
  const auto iter = std::max_element(
      mock_connected_pointer_devices_.begin(),
      mock_connected_pointer_devices_.end(),
      [](const PointerDevice& left, const PointerDevice& right) -> bool {
        return left.max_active_contacts < right.max_active_contacts;
      });
  return (iter != mock_connected_pointer_devices_.end())
             ? iter->max_active_contacts
             : 0;
}

std::optional<PointerDevice> MockTouchUiController::GetPointerDevice(
    PointerDevice::Key key) const {
  const auto iter = std::find(mock_connected_pointer_devices_.begin(),
                              mock_connected_pointer_devices_.end(), key);
  return (iter != mock_connected_pointer_devices_.end())
             ? std::make_optional(*iter)
             : std::nullopt;
}

std::vector<PointerDevice> MockTouchUiController::GetPointerDevices() const {
  return mock_connected_pointer_devices_;
}
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace ui

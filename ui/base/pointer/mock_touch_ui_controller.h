// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_POINTER_MOCK_TOUCH_UI_CONTROLLER_H_
#define UI_BASE_POINTER_MOCK_TOUCH_UI_CONTROLLER_H_

#include <optional>
#include <vector>

#include "ui/base/pointer/touch_ui_controller.h"

namespace ui {

class MockTouchUiController final : public TouchUiController {
 public:
  explicit MockTouchUiController(
      TouchUiController::TouchUiState touch_ui_state =
          TouchUiController::TouchUiState::kAuto);
  MockTouchUiController(const MockTouchUiController&) = delete;
  MockTouchUiController& operator=(const MockTouchUiController&) = delete;
  ~MockTouchUiController() final;

#if BUILDFLAG(USE_BLINK)
  void SetMockConnectedPointerDevices(
      const std::vector<PointerDevice>& devices);
  using TouchUiController::GetLastKnownPointerDevicesForTesting;
  using TouchUiController::SetTouchUiState;

 protected:
  // TouchUiController:
  int MaxTouchPoints() const final;
  std::optional<PointerDevice> GetPointerDevice(
      PointerDevice::Key key) const final;
  std::vector<PointerDevice> GetPointerDevices() const final;

 private:
  std::vector<PointerDevice> mock_connected_pointer_devices_;
#endif  // BUILDFLAG(USE_BLINK)
};

}  // namespace ui

#endif  // UI_BASE_POINTER_MOCK_TOUCH_UI_CONTROLLER_H_

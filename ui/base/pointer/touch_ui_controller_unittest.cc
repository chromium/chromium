// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"

namespace ui {

namespace {

class TestObserver {
 public:
  explicit TestObserver(TouchUiController* controller)
      : subscription_(controller->RegisterCallback(
            base::BindLambdaForTesting([this]() { ++touch_ui_changes_; }))) {}
  ~TestObserver() = default;

  int touch_ui_changes() const { return touch_ui_changes_; }

 private:
  int touch_ui_changes_ = 0;
  base::CallbackListSubscription subscription_;
};

}  // namespace

// Verifies that non-touch is the default.
TEST(TouchUiControllerTest, DefaultIsNonTouch) {
  TouchUiController controller;
  EXPECT_FALSE(controller.touch_ui());
}

// Verifies that kDisabled maps to non-touch.
TEST(TouchUiControllerTest, DisabledIsNonTouch) {
  TouchUiController controller(TouchUiController::TouchUiState::kDisabled);
  EXPECT_FALSE(controller.touch_ui());
}

// Verifies that kAuto maps to non-touch (the default).
TEST(TouchUiControllerTest, AutoIsNonTouch) {
  TouchUiController controller(TouchUiController::TouchUiState::kAuto);
  EXPECT_FALSE(controller.touch_ui());
}

// Verifies that kEnabled maps to touch.
TEST(TouchUiControllerTest, EnabledIsNonTouch) {
  TouchUiController controller(TouchUiController::TouchUiState::kEnabled);
  EXPECT_TRUE(controller.touch_ui());
}

// Verifies that when the mode is set to non-touch and the tablet mode toggles,
// the touch UI state does not change.
TEST(TouchUiControllerTest, TabletToggledOnTouchUiDisabled) {
  TouchUiController controller(TouchUiController::TouchUiState::kDisabled);
  TestObserver observer(&controller);

  controller.OnTabletModeToggled(true);
  EXPECT_FALSE(controller.touch_ui());
  EXPECT_EQ(0, observer.touch_ui_changes());

  controller.OnTabletModeToggled(false);
  EXPECT_FALSE(controller.touch_ui());
  EXPECT_EQ(0, observer.touch_ui_changes());
}

// Verifies that when the mode is set to auto and the tablet mode toggles, the
// touch UI state changes and the observer gets called back.
TEST(TouchUiControllerTest, TabletToggledOnTouchUiAuto) {
  TouchUiController controller(TouchUiController::TouchUiState::kAuto);
  TestObserver observer(&controller);

  controller.OnTabletModeToggled(true);
  EXPECT_TRUE(controller.touch_ui());
  EXPECT_EQ(1, observer.touch_ui_changes());

  controller.OnTabletModeToggled(false);
  EXPECT_FALSE(controller.touch_ui());
  EXPECT_EQ(2, observer.touch_ui_changes());
}

}  // namespace ui

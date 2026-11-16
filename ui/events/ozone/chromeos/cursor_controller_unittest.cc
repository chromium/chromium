// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/chromeos/cursor_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

const gfx::AcceleratedWidget kTestWindow = 1;

}  // namespace

class CursorControllerTest : public testing::Test {
 public:
  CursorControllerTest() {}

  CursorControllerTest(const CursorControllerTest&) = delete;
  CursorControllerTest& operator=(const CursorControllerTest&) = delete;

  ~CursorControllerTest() override {}

  void TearDown() override {
    ui::CursorController* cursor_controller = CursorController::GetInstance();
    cursor_controller->ClearCursorConfigForWindow(kTestWindow);
    cursor_controller->SetInternalDisplayRotation(display::Display::ROTATE_0);
  }
};

TEST_F(CursorControllerTest, UnconfiguredIdentity) {
  ui::CursorController* cursor_controller = CursorController::GetInstance();

  // Check that unconfigured windows use identity.
  gfx::Vector2dF delta(2.f, 3.f);
  cursor_controller->ApplyCursorConfigForWindow(kTestWindow, &delta);
  EXPECT_FLOAT_EQ(2.f, delta.x());
  EXPECT_FLOAT_EQ(3.f, delta.y());
}

TEST_F(CursorControllerTest, ClearedIdentity) {
  ui::CursorController* cursor_controller = CursorController::GetInstance();

  // Check that configured & cleared windows use identity.
  cursor_controller->SetCursorConfigForWindow(
      kTestWindow, display::Display::ROTATE_180, 3.2f);
  cursor_controller->ClearCursorConfigForWindow(kTestWindow);
  gfx::Vector2dF delta(3.f, 5.f);
  cursor_controller->ApplyCursorConfigForWindow(kTestWindow, &delta);
  EXPECT_FLOAT_EQ(3.f, delta.x());
  EXPECT_FLOAT_EQ(5.f, delta.y());
}

TEST_F(CursorControllerTest, RotatedHighDpi) {
  ui::CursorController* cursor_controller = CursorController::GetInstance();

  // Check that 90deg rotated highdpi window transforms correctly.
  cursor_controller->SetCursorConfigForWindow(kTestWindow,
                                              display::Display::ROTATE_90, 2.f);
  gfx::Vector2dF delta(3.f, 5.f);
  cursor_controller->ApplyCursorConfigForWindow(kTestWindow, &delta);
  EXPECT_FLOAT_EQ(-10.f, delta.x());
  EXPECT_FLOAT_EQ(6.f, delta.y());
}

TEST_F(CursorControllerTest, InternalDisplayRotation) {
  ui::CursorController* cursor_controller = CursorController::GetInstance();

  // Simulate a portrait-mounted internal panel (+90deg hardware orientation)
  // rotated 90deg by the user, yielding a 180deg total panel rotation.
  cursor_controller->SetInternalDisplayRotation(display::Display::ROTATE_90);
  cursor_controller->SetCursorConfigForWindow(
      kTestWindow, display::Display::ROTATE_180, 2.f);

  gfx::Vector2dF touchpad_delta(3.f, 5.f);
  cursor_controller->ApplyInternalDisplayRotation(&touchpad_delta);
  cursor_controller->ApplyCursorConfigForWindow(kTestWindow, &touchpad_delta);
  EXPECT_FLOAT_EQ(-10.f, touchpad_delta.x());
  EXPECT_FLOAT_EQ(6.f, touchpad_delta.y());
}

}  // namespace ui

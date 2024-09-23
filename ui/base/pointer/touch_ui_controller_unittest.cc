// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/mock_touch_ui_controller.h"

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

class TouchUiControllerTest : public testing::Test {
 public:
  using TouchUiState = ::ui::TouchUiController::TouchUiState;

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

}  // namespace

// Verifies that non-touch is the default.
TEST_F(TouchUiControllerTest, DefaultIsNonTouch) {
  MockTouchUiController controller;
  EXPECT_FALSE(controller.touch_ui());
}

// Verifies that kDisabled maps to non-touch.
TEST_F(TouchUiControllerTest, DisabledIsNonTouch) {
  MockTouchUiController controller(TouchUiState::kDisabled);
  EXPECT_FALSE(controller.touch_ui());
}

// Verifies that kAuto maps to non-touch (the default).
TEST_F(TouchUiControllerTest, AutoIsNonTouch) {
  MockTouchUiController controller(TouchUiState::kAuto);
  EXPECT_FALSE(controller.touch_ui());
}

// Verifies that kEnabled maps to touch.
TEST_F(TouchUiControllerTest, EnabledIsNonTouch) {
  MockTouchUiController controller(TouchUiState::kEnabled);
  EXPECT_TRUE(controller.touch_ui());
}

// Verifies that when the mode is set to non-touch and the tablet mode toggles,
// the touch UI state does not change.
TEST_F(TouchUiControllerTest, TabletToggledOnTouchUiDisabled) {
  MockTouchUiController controller(TouchUiState::kDisabled);
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
TEST_F(TouchUiControllerTest, TabletToggledOnTouchUiAuto) {
  MockTouchUiController controller(TouchUiState::kAuto);
  TestObserver observer(&controller);

  controller.OnTabletModeToggled(true);
  EXPECT_TRUE(controller.touch_ui());
  EXPECT_EQ(1, observer.touch_ui_changes());

  controller.OnTabletModeToggled(false);
  EXPECT_FALSE(controller.touch_ui());
  EXPECT_EQ(2, observer.touch_ui_changes());
}

#if BUILDFLAG(USE_BLINK)
TEST_F(TouchUiControllerTest, DetectPointerDevices) {
  constexpr const char kOnStartupHistogram[] = "Input.Digitizer.OnStartup";
  constexpr const char kOnConnectedHistogram[] = "Input.Digitizer.OnConnected";
  constexpr const char kOnDisconnectedHistogram[] =
      "Input.Digitizer.OnDisconnected";
  constexpr const char kMaxTouchPointsDirectPenHistogram[] =
      "Input.Digitizer.MaxTouchPoints.DirectPen";
  constexpr const char kMaxTouchPointsIndirectPenHistogram[] =
      "Input.Digitizer.MaxTouchPoints.IndirectPen";
  constexpr const char kMaxTouchPointsTouchHistogram[] =
      "Input.Digitizer.MaxTouchPoints.Touch";
  constexpr const char kMaxTouchPointsTouchPadHistogram[] =
      "Input.Digitizer.MaxTouchPoints.TouchPad";
  constexpr const char kMaxTouchPointsSupportedBySystemAtStartupHistogram[] =
      "Input.Digitizer.MaxTouchPointsSupportedBySystemAtStartup";

  static const PointerDevice kDirectPenDevice = {
      .key = PointerDevice::Key(0),
      .digitizer = PointerDigitizerType::kDirectPen,
      .max_active_contacts = 11};
  static const PointerDevice kIndirectPenDevice = {
      .key = PointerDevice::Key(1),
      .digitizer = PointerDigitizerType::kIndirectPen,
      .max_active_contacts = 20};
  static const PointerDevice kTouchKeyDevice = {
      .key = PointerDevice::Key(2),
      .digitizer = PointerDigitizerType::kTouch,
      .max_active_contacts = 1};
  static const PointerDevice kTouchPadDevice = {
      .key = PointerDevice::Key(3),
      .digitizer = PointerDigitizerType::kTouchPad,
      .max_active_contacts = 5};
  base::HistogramTester histogram_tester;

  // Expected to fire histograms for digitizer(s) found at startup,
  // per-digitizer type max touch points, and aggregate max touch points.
  MockTouchUiController controller(TouchUiState::kAuto);
  controller.SetMockConnectedPointerDevices((std::vector<PointerDevice>{
      kTouchPadDevice, kDirectPenDevice, kTouchKeyDevice}));
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(kOnStartupHistogram, 3);
  histogram_tester.ExpectBucketCount(kOnStartupHistogram,
                                     PointerDigitizerType::kDirectPen, 1);
  histogram_tester.ExpectBucketCount(kOnStartupHistogram,
                                     PointerDigitizerType::kTouch, 1);
  histogram_tester.ExpectBucketCount(kOnStartupHistogram,
                                     PointerDigitizerType::kTouchPad, 1);
  histogram_tester.ExpectTotalCount(kMaxTouchPointsDirectPenHistogram, 1);
  histogram_tester.ExpectBucketCount(kMaxTouchPointsDirectPenHistogram, 11, 1);
  histogram_tester.ExpectTotalCount(kMaxTouchPointsTouchHistogram, 1);
  histogram_tester.ExpectBucketCount(kMaxTouchPointsTouchHistogram, 1, 1);
  histogram_tester.ExpectTotalCount(kMaxTouchPointsTouchPadHistogram, 1);
  histogram_tester.ExpectBucketCount(kMaxTouchPointsTouchPadHistogram, 5, 1);
  histogram_tester.ExpectTotalCount(
      kMaxTouchPointsSupportedBySystemAtStartupHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kMaxTouchPointsSupportedBySystemAtStartupHistogram, 11, 1);

  // Simulate receiving platform specific notifications that pointer devices
  // have been connected or disconnected.
  // Expected to fire histograms for digitizer(s) connected or disconnected,
  // and per-digitizer type max touch points for newly connected devices.
  controller.SetMockConnectedPointerDevices(
      {kTouchPadDevice, kIndirectPenDevice});
  controller.OnPointerDeviceDisconnected(kDirectPenDevice.key);
  controller.OnPointerDeviceDisconnected(kTouchKeyDevice.key);
  controller.OnPointerDeviceConnected(kIndirectPenDevice.key);
  EXPECT_EQ(controller.GetLastKnownPointerDevicesForTesting(),
            (std::vector<PointerDevice>{kTouchPadDevice, kIndirectPenDevice}));

  histogram_tester.ExpectTotalCount(kOnConnectedHistogram, 1);
  histogram_tester.ExpectBucketCount(kOnConnectedHistogram,
                                     PointerDigitizerType::kIndirectPen, 1);
  histogram_tester.ExpectTotalCount(kOnDisconnectedHistogram, 2);
  histogram_tester.ExpectBucketCount(kOnDisconnectedHistogram,
                                     PointerDigitizerType::kTouch, 1);
  histogram_tester.ExpectBucketCount(kOnDisconnectedHistogram,
                                     PointerDigitizerType::kDirectPen, 1);
  histogram_tester.ExpectTotalCount(kMaxTouchPointsIndirectPenHistogram, 1);
  histogram_tester.ExpectBucketCount(kMaxTouchPointsIndirectPenHistogram, 20,
                                     1);

  // The following aren't affected by the events above.
  histogram_tester.ExpectTotalCount(kMaxTouchPointsDirectPenHistogram, 1);
  histogram_tester.ExpectTotalCount(kMaxTouchPointsTouchHistogram, 1);
  histogram_tester.ExpectTotalCount(kMaxTouchPointsTouchPadHistogram, 1);

  // The following should never change after their initially logged at startup.
  histogram_tester.ExpectTotalCount(kOnStartupHistogram, 3);
  histogram_tester.ExpectTotalCount(
      kMaxTouchPointsSupportedBySystemAtStartupHistogram, 1);
}
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace ui

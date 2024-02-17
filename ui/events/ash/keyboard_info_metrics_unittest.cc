// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_info_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ash {

namespace {

using DeviceType = ui::KeyboardCapability::DeviceType;
using KeyboardTopRowLayout = ui::KeyboardCapability::KeyboardTopRowLayout;
using KeyboardInfo = ui::KeyboardCapability::KeyboardInfo;

}  // namespace

class KeyboardInfoMetricsTest : public testing::Test {
 public:
  KeyboardInfoMetricsTest() = default;
  ~KeyboardInfoMetricsTest() override = default;

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(KeyboardInfoMetricsTest, Layout1) {
  KeyboardInfo internal_keyboard_info;
  internal_keyboard_info.top_row_layout =
      KeyboardTopRowLayout::kKbdTopRowLayout1;
  internal_keyboard_info.device_type = DeviceType::kDeviceInternalKeyboard;
  ui::RecordKeyboardInfoMetrics(internal_keyboard_info,
                                /*has_assistant_key=*/false);

  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ui::KeyboardTopRowLayoutForMetric::kLayout1, 1);
}

TEST_F(KeyboardInfoMetricsTest, Layout2) {
  KeyboardInfo internal_keyboard_info;
  internal_keyboard_info.top_row_layout =
      KeyboardTopRowLayout::kKbdTopRowLayout2;
  internal_keyboard_info.device_type = DeviceType::kDeviceInternalKeyboard;
  ui::RecordKeyboardInfoMetrics(internal_keyboard_info,
                                /*has_assistant_key=*/false);

  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ui::KeyboardTopRowLayoutForMetric::kLayout2, 1);
}

TEST_F(KeyboardInfoMetricsTest, Layout2WithAssistant) {
  KeyboardInfo internal_keyboard_info;
  internal_keyboard_info.top_row_layout =
      KeyboardTopRowLayout::kKbdTopRowLayout2;
  internal_keyboard_info.device_type = DeviceType::kDeviceInternalKeyboard;
  ui::RecordKeyboardInfoMetrics(internal_keyboard_info,
                                /*has_assistant_key=*/true);

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType", 1);
  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ui::KeyboardTopRowLayoutForMetric::kLayout2WithAssistant, 1);
}

TEST_F(KeyboardInfoMetricsTest, Layout3) {
  KeyboardInfo internal_keyboard_info;
  internal_keyboard_info.top_row_layout =
      KeyboardTopRowLayout::kKbdTopRowLayoutWilco;
  internal_keyboard_info.device_type = DeviceType::kDeviceInternalKeyboard;
  ui::RecordKeyboardInfoMetrics(internal_keyboard_info,
                                /*has_assistant_key=*/false);

  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ui::KeyboardTopRowLayoutForMetric::kLayout3, 1);
}

TEST_F(KeyboardInfoMetricsTest, Layout4) {
  KeyboardInfo internal_keyboard_info;
  internal_keyboard_info.top_row_layout =
      KeyboardTopRowLayout::kKbdTopRowLayoutDrallion;
  internal_keyboard_info.device_type = DeviceType::kDeviceInternalKeyboard;
  ui::RecordKeyboardInfoMetrics(internal_keyboard_info,
                                /*has_assistant_key=*/false);

  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ui::KeyboardTopRowLayoutForMetric::kLayout4, 1);
}

TEST_F(KeyboardInfoMetricsTest, LayoutCustom1) {
  KeyboardInfo internal_keyboard_info;
  internal_keyboard_info.top_row_layout =
      KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
  internal_keyboard_info.device_type = DeviceType::kDeviceInternalKeyboard;
  ui::RecordKeyboardInfoMetrics(internal_keyboard_info,
                                /*has_assistant_key=*/false);

  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ui::KeyboardTopRowLayoutForMetric::kLayoutCustom1, 1);
}

class LayoutsBesidesLayout2WithAssistantKey
    : public KeyboardInfoMetricsTest,
      public testing::WithParamInterface<KeyboardTopRowLayout> {};

// Test that non-Layout2 keyboards don't emit the wrong type when
// has_assistant_key is true. Note that this case shouldn't come up in reality.
INSTANTIATE_TEST_SUITE_P(
    All,
    LayoutsBesidesLayout2WithAssistantKey,
    testing::ValuesIn(std::vector<KeyboardTopRowLayout>{
        KeyboardTopRowLayout::kKbdTopRowLayout1,
        /* Purposefully excluding Layout2 (see comment above)*/
        KeyboardTopRowLayout::kKbdTopRowLayoutWilco,
        KeyboardTopRowLayout::kKbdTopRowLayoutDrallion,
        KeyboardTopRowLayout::kKbdTopRowLayoutCustom,
    }));

TEST_P(LayoutsBesidesLayout2WithAssistantKey, Layout2WithAssistantNotEmitted) {
  KeyboardTopRowLayout keyboard_top_row_layout = GetParam();

  KeyboardInfo internal_keyboard_info;
  internal_keyboard_info.top_row_layout = keyboard_top_row_layout;
  internal_keyboard_info.device_type = DeviceType::kDeviceInternalKeyboard;
  ui::RecordKeyboardInfoMetrics(internal_keyboard_info,
                                /*has_assistant_key=*/true);

  // When has_assistant_key is true, kLayout2WithAssistant should not be
  // recorded unless the keyboard layout is Layout2.
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType",
      ui::KeyboardTopRowLayoutForMetric::kLayout2WithAssistant, 0);
}

class NonInternalTopRowLayoutTest
    : public KeyboardInfoMetricsTest,
      public testing::WithParamInterface<DeviceType> {};

// Test that non kDeviceInternalKeyboards do not emit the top row layout metric.
INSTANTIATE_TEST_SUITE_P(
    All,
    NonInternalTopRowLayoutTest,
    testing::ValuesIn(std::vector<DeviceType>{
        DeviceType::kDeviceUnknown,
        DeviceType::kDeviceInternalRevenKeyboard,
        DeviceType::kDeviceExternalAppleKeyboard,
        DeviceType::kDeviceExternalChromeOsKeyboard,
        DeviceType::kDeviceExternalNullTopRowChromeOsKeyboard,
        DeviceType::kDeviceExternalGenericKeyboard,
        DeviceType::kDeviceExternalUnknown,
        DeviceType::kDeviceHotrodRemote,
        DeviceType::kDeviceVirtualCoreKeyboard,
    }));

TEST_P(NonInternalTopRowLayoutTest,
       NonInternalKeyboardDoesNotEmitTopRowLayout) {
  DeviceType device_type = GetParam();

  KeyboardInfo external_keyboard_info;
  external_keyboard_info.top_row_layout =
      KeyboardTopRowLayout::kKbdTopRowLayoutWilco;
  external_keyboard_info.device_type = device_type;
  ui::RecordKeyboardInfoMetrics(external_keyboard_info,
                                /*has_assistant_key=*/false);

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.InternalKeyboard.TopRowLayoutType", 0);
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_mode_histogram_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

class AXHistogramLoggerTest : public ::testing::Test {
 protected:
  AXHistogramLoggerTest() = default;
  ~AXHistogramLoggerTest() override = default;

  void SetAXMode(AXMode mode) {
    previous_mode_ = mode_;
    mode_ = mode;
    RecordAccessibilityModeHistograms(prefix_, mode_, previous_mode_);
  }

  AXHistogramPrefix prefix_ = AXHistogramPrefix::kNone;
  AXMode mode_;
  AXMode previous_mode_;
};

TEST_F(AXHistogramLoggerTest, ModeTest) {
  const std::string histogram_name = "Accessibility.ModeFlag";
  base::HistogramTester histogram_tester;

  SetAXMode(kAXModeBasic);
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_NATIVE_APIS,
      1);
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS,
      1);
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_LABEL_IMAGES,
      0);

  SetAXMode(AXMode::kLabelImages | AXMode::kNativeAPIs);
  // Previously active so state change is not an unset to set transition.
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_NATIVE_APIS,
      1);
  // Set to unset transition is not logged, and previous histogram value remains
  // unchanged.
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS,
      1);
  // Freshly added flag.
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_LABEL_IMAGES,
      1);

  SetAXMode(AXMode::kWebContents);
  // New unset to set transition.
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS,
      2);
}

TEST_F(AXHistogramLoggerTest, BundleTest) {
  const std::string histogram_name = "Accessibility.Bundle";
  base::HistogramTester histogram_tester;

  SetAXMode(kAXModeBasic);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AXMode::BundleHistogramValue::kBasic, 1);
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::BundleHistogramValue::kComplete, 0);

  SetAXMode(kAXModeComplete);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AXMode::BundleHistogramValue::kBasic, 1);
  histogram_tester.ExpectBucketCount(
      histogram_name, AXMode::BundleHistogramValue::kComplete, 1);
}

TEST_F(AXHistogramLoggerTest, FormsTest) {
  const std::string histogram_name =
      "Accessibility.ExperimentalModeFlag.FormControls";
  base::HistogramTester histogram_tester;

  SetAXMode(kAXModeBasic);
  histogram_tester.ExpectBucketCount(histogram_name, true, 0);

  SetAXMode(kAXModeFormControls);
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);

  SetAXMode(AXMode(AXMode::kHTML, AXMode::kExperimentalFormControls));
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);

  SetAXMode(kAXModeBasic);
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);

  SetAXMode(kAXModeFormControls);
  histogram_tester.ExpectBucketCount(histogram_name, true, 2);
}

TEST_F(AXHistogramLoggerTest, RendererTest) {
  base::HistogramTester histogram_tester;
  prefix_ = AXHistogramPrefix::kBlink;
  SetAXMode(kAXModeFormControls);
  histogram_tester.ExpectBucketCount(
      "Blink.Accessibility.ModeFlag",
      AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.Accessibility.Bundle", AXMode::BundleHistogramValue::kFormControls,
      1);
  histogram_tester.ExpectBucketCount(
      "Blink.Accessibility.ExperimentalModeFlag.FormControls", true, 1);
}

}  // anonymous namespace

}  // namespace ui

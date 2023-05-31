// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter_factory.h"

#include <linux/input.h>

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/touch_filter/heuristic_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/open_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"
#include "ui/events/ozone/features.h"

namespace ui {

class PalmDetectionFilterFactoryTest : public testing::Test {
 public:
  PalmDetectionFilterFactoryTest() = default;

  PalmDetectionFilterFactoryTest(const PalmDetectionFilterFactoryTest&) =
      delete;
  PalmDetectionFilterFactoryTest& operator=(
      const PalmDetectionFilterFactoryTest&) = delete;

  void SetUp() override {
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kEveTouchScreen, &eve_touchscreen_info_));
    EXPECT_TRUE(CapabilitiesToDeviceInfo(kNocturneTouchScreen,
                                         &nocturne_touchscreen_info_));
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kNocturneStylus, &nocturne_stylus_info_));
    EXPECT_TRUE(CapabilitiesToDeviceInfo(kEveStylus, &eve_stylus_info_));
    EXPECT_TRUE(CapabilitiesToDeviceInfo(kKohakuTouchscreen,
                                         &kohaku_touchscreen_info_));
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  EventDeviceInfo eve_touchscreen_info_, eve_stylus_info_,
      nocturne_touchscreen_info_, nocturne_stylus_info_,
      kohaku_touchscreen_info_;
  SharedPalmDetectionFilterState shared_palm_state_;
};

class PalmDetectionFilterFactoryDeathTest
    : public PalmDetectionFilterFactoryTest {};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PalmDetectionFilterFactoryTest, RadiusesFromLSBRelease) {
  {
    base::test::ScopedChromeOSVersionInfo version(
        "CHROMEOS_RELEASE_BOARD=hatch\n", base::Time());
    EXPECT_EQ("0.1010944, 3.51837568",
              internal::FetchNeuralPalmRadiusPolynomial(
                  kohaku_touchscreen_info_, ""));
  }
  {
    base::test::ScopedChromeOSVersionInfo version(
        "CHROMEOS_RELEASE_BOARD=reef\n", base::Time());
    EXPECT_EQ("0.17889799, 4.22584412",
              internal::FetchNeuralPalmRadiusPolynomial(
                  kohaku_touchscreen_info_, ""));
  }
  {
    base::test::ScopedChromeOSVersionInfo version(
        "CHROMEOS_RELEASE_BOARD=octopus\n", base::Time());
    EXPECT_EQ("", internal::FetchNeuralPalmRadiusPolynomial(
                      kohaku_touchscreen_info_, ""));
  }
}
#endif

TEST_F(PalmDetectionFilterFactoryTest, RadiusFromSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kOzoneNNPalmSwitchName, "{\"radius-polynomial\" : \"15.8,22.7,30.01\"}");
  EXPECT_EQ("15.8,22.7,30.01", internal::FetchNeuralPalmRadiusPolynomial(
                                   kohaku_touchscreen_info_, ""));
}

TEST_F(PalmDetectionFilterFactoryTest, AllDisabled) {
  scoped_feature_list_->InitWithFeatures(
      {}, {ui::kEnableHeuristicPalmDetectionFilter,
           ui::kEnableNeuralPalmDetectionFilter});
  std::unique_ptr<PalmDetectionFilter> palm_filter =
      CreatePalmDetectionFilter(eve_touchscreen_info_, &shared_palm_state_);
  EXPECT_EQ(OpenPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
  palm_filter = CreatePalmDetectionFilter(nocturne_touchscreen_info_,
                                          &shared_palm_state_);
  EXPECT_EQ(OpenPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
}

TEST_F(PalmDetectionFilterFactoryTest, HeuristicEnabledForEve) {
  scoped_feature_list_->InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(ui::kEnableHeuristicPalmDetectionFilter,
                                       {})},
      {ui::kEnableNeuralPalmDetectionFilter});
  std::unique_ptr<PalmDetectionFilter> palm_filter =
      CreatePalmDetectionFilter(eve_touchscreen_info_, &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());

  palm_filter = CreatePalmDetectionFilter(nocturne_touchscreen_info_,
                                          &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());

  // And the stylus.
  palm_filter =
      CreatePalmDetectionFilter(nocturne_stylus_info_, &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
  palm_filter =
      CreatePalmDetectionFilter(eve_stylus_info_, &shared_palm_state_);
  EXPECT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
}

TEST_F(PalmDetectionFilterFactoryTest, HeuristicTimesSet) {
  scoped_feature_list_->InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(
          ui::kEnableHeuristicPalmDetectionFilter,
          {{"heuristic_palm_cancel_threshold_seconds", "0.8"},
           {"heuristic_palm_hold_threshold_seconds", "15.327"}})},
      {ui::kEnableNeuralPalmDetectionFilter});

  std::unique_ptr<PalmDetectionFilter> palm_filter = CreatePalmDetectionFilter(
      nocturne_touchscreen_info_, &shared_palm_state_);
  ASSERT_EQ(HeuristicStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
  HeuristicStylusPalmDetectionFilter* heuristic_filter =
      static_cast<HeuristicStylusPalmDetectionFilter*>(palm_filter.get());
  EXPECT_EQ(base::Seconds(0.8), heuristic_filter->CancelTime());
  EXPECT_EQ(base::Seconds(15.327), heuristic_filter->HoldTime());
}

TEST_F(PalmDetectionFilterFactoryTest, NeuralBeatsHeuristic) {
  scoped_feature_list_->InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(ui::kEnableHeuristicPalmDetectionFilter,
                                       {}),
       base::test::FeatureRefAndParams(ui::kEnableNeuralPalmDetectionFilter,
                                       {})},
      {});
  std::unique_ptr<PalmDetectionFilter> palm_filter = CreatePalmDetectionFilter(
      nocturne_touchscreen_info_, &shared_palm_state_);
  ASSERT_EQ(NeuralStylusPalmDetectionFilter::kFilterName,
            palm_filter->FilterNameForTesting());
}

TEST_F(PalmDetectionFilterFactoryTest, ParseTest) {
  EXPECT_EQ(std::vector<float>(), internal::ParseRadiusPolynomial(""));
  // Note we test for whitespace trimming.
  EXPECT_EQ(std::vector<float>({3.7, 2.91, 15.19191}),
            internal::ParseRadiusPolynomial("3.7, 2.91, 15.19191  "));
}

TEST_F(PalmDetectionFilterFactoryDeathTest, BadParseRecovery) {
  // In DCHECK builds, die. Otherwise, expect {}
  EXPECT_DCHECK_DEATH_WITH(EXPECT_EQ(std::vector<float>(),
                                     internal::ParseRadiusPolynomial("cheese")),
                           "Unable to parse.*cheese");
}

TEST_F(PalmDetectionFilterFactoryDeathTest, BadNeuralParamParse) {
  scoped_feature_list_->InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(
          ui::kEnableNeuralPalmDetectionFilter,
          {
              {"neural_palm_radius_polynomial", "1.0,chicken"},
          })},
      {ui::kEnableHeuristicPalmDetectionFilter});
  EXPECT_DCHECK_DEATH_WITH(CreatePalmDetectionFilter(nocturne_touchscreen_info_,
                                                     &shared_palm_state_),
                           "Unable to parse.*chicken");
}

}  // namespace ui

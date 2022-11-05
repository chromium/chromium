// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

class TestNativeTheme : public NativeTheme {
 public:
  TestNativeTheme() : NativeTheme(false) {}
  TestNativeTheme(const TestNativeTheme&) = delete;
  TestNativeTheme& operator=(const TestNativeTheme&) = delete;
  ~TestNativeTheme() override = default;

  // NativeTheme:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override {
    return gfx::Size();
  }
  void Paint(cc::PaintCanvas* canvas,
             const ui::ColorProvider* color_provider,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             ColorScheme color_scheme = ColorScheme::kDefault,
             const absl::optional<SkColor>& accent_color =
                 absl::nullopt) const override {}
  bool SupportsNinePatch(Part part) const override { return false; }
  gfx::Size GetNinePatchCanvasSize(Part part) const override {
    return gfx::Size();
  }
  gfx::Rect GetNinePatchAperture(Part part) const override {
    return gfx::Rect();
  }
};

}  // namespace

TEST(NativeThemeTest, TestOnNativeThemeUpdatedMetricsEmitted) {
  base::HistogramTester histogram_tester;
  TestNativeTheme theme;
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentProcessingOnNativeThemeUpdatedEvent", 0);
  histogram_tester.ExpectUniqueSample(
      "Views.Browser.NumColorProvidersInitializedDuringOnNativeThemeUpdated", 0,
      0);

  theme.NotifyOnNativeThemeUpdated();
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentProcessingOnNativeThemeUpdatedEvent", 1);
  histogram_tester.ExpectUniqueSample(
      "Views.Browser.NumColorProvidersInitializedDuringOnNativeThemeUpdated", 0,
      1);

  theme.NotifyOnNativeThemeUpdated();
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentProcessingOnNativeThemeUpdatedEvent", 2);
  histogram_tester.ExpectUniqueSample(
      "Views.Browser.NumColorProvidersInitializedDuringOnNativeThemeUpdated", 0,
      2);
}

}  // namespace ui

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/color_utils_android.h"

#include <stdint.h>

#include <limits>
#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

namespace {

// Intentionally avoids reusing the constant defined in color_helpers.h to catch
// mistakes that accidentally change the value.
constexpr int64_t kAndroidInvalidColor =
    static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;

// https://developer.android.com/reference/android/graphics/Color.html defines
// the various color constants.
constexpr int kAndroidBlack = -16777216;
constexpr int kAndroidWhite = -1;
constexpr int kAndroidRed = -65536;
constexpr int kAndroidDkgray = -12303292;
constexpr int kAndroidTransparent = 0;

}  // namespace

TEST(ColorHelpersTest, Null) {
  EXPECT_EQ(kAndroidInvalidColor, OptionalSkColorToJavaColor(std::nullopt));
  EXPECT_FALSE(JavaColorToOptionalSkColor(kAndroidInvalidColor).has_value());
}

TEST(ColorHelpersTest, Roundtrip) {
  EXPECT_EQ(kAndroidBlack, OptionalSkColorToJavaColor(SK_ColorBLACK));
  EXPECT_EQ(SK_ColorBLACK, JavaColorToOptionalSkColor(kAndroidBlack));

  EXPECT_EQ(kAndroidWhite, OptionalSkColorToJavaColor(SK_ColorWHITE));
  EXPECT_EQ(SK_ColorWHITE, JavaColorToOptionalSkColor(kAndroidWhite));

  EXPECT_EQ(kAndroidRed, OptionalSkColorToJavaColor(SK_ColorRED));
  EXPECT_EQ(SK_ColorRED, JavaColorToOptionalSkColor(kAndroidRed));

  EXPECT_EQ(kAndroidDkgray, OptionalSkColorToJavaColor(SK_ColorDKGRAY));
  EXPECT_EQ(SK_ColorDKGRAY, JavaColorToOptionalSkColor(kAndroidDkgray));

  EXPECT_EQ(kAndroidTransparent,
            OptionalSkColorToJavaColor(SK_ColorTRANSPARENT));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            JavaColorToOptionalSkColor(kAndroidTransparent));
}

}  // namespace ui

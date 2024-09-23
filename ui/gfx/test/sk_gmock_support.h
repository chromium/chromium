// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEST_SK_GMOCK_SUPPORT_H_
#define UI_GFX_TEST_SK_GMOCK_SUPPORT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/test/sk_color_eq.h"

namespace gfx::test {

MATCHER_P2(IsCloseToBitmap, expected_bmp, max_per_channel_deviation, "") {
  // Number of pixels with an error
  int error_pixels_count = 0;

  gfx::Rect error_bounding_rect = gfx::Rect();

  // Check that bitmaps have identical dimensions.
  if (arg.width() != expected_bmp.width()) {
    *result_listener << "where widths do not match, actual: " << arg.width()
                     << ", expected: " << expected_bmp.width();
    return false;
  }
  if (arg.height() != expected_bmp.height()) {
    *result_listener << "where heights do not match, actual: " << arg.height()
                     << ", expected: " << expected_bmp.height();
    return false;
  }

  for (int x = 0; x < arg.width(); ++x) {
    for (int y = 0; y < arg.height(); ++y) {
      SkColor actual_color = arg.getColor(x, y);
      SkColor expected_color = expected_bmp.getColor(x, y);
      if (!ColorsClose(actual_color, expected_color,
                       max_per_channel_deviation)) {
        ++error_pixels_count;
        error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
      }
    }
  }

  if (error_pixels_count != 0) {
    *result_listener
        << "Number of pixel with an error, given max_per_channel_deviation of "
        << max_per_channel_deviation << ": " << error_pixels_count
        << "\nError Bounding Box : " << error_bounding_rect.ToString() << "\n";
    int sample_x = error_bounding_rect.x();
    int sample_y = error_bounding_rect.y();
    std::string expected_color = color_utils::SkColorToRgbaString(
        expected_bmp.getColor(sample_x, sample_y));
    std::string actual_color =
        color_utils::SkColorToRgbaString(arg.getColor(sample_x, sample_y));
    *result_listener << "Sample pixel comparison at " << sample_x << "x"
                     << sample_y << ": Expected " << expected_color
                     << ", actual " << actual_color;
    return false;
  }

  return true;
}

MATCHER_P(EqualsBitmap, expected_bmp, "") {
  return testing::ExplainMatchResult(
      IsCloseToBitmap(expected_bmp,
                      /*max_per_channel_deviation=*/0),
      arg, result_listener);
}

}  // namespace gfx::test

#endif  // UI_GFX_TEST_SK_GMOCK_SUPPORT_H_

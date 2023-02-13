// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/large_image_view.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace message_center {
namespace {

struct TestCase {
  constexpr TestCase(const gfx::Size& input_param,
                     const gfx::Size& output_param)
      : input(input_param), expected_output(output_param) {}
  const gfx::Size input;
  const gfx::Size expected_output;
};

}  // namespace

using LargeImageViewTest = testing::Test;

// Verifies that a large image view's preferred size is expected after setting
// an image.
TEST_F(LargeImageViewTest, CheckPreferredSize) {
  const std::array<TestCase, 3> kTestCases{
      // A small size so that clamping this size should be a no-op.
      TestCase{
          /*input_param=*/gfx::Size{300, 300},
          /*output_param=*/gfx::Size{300, 300},
      },

      // The height of the resized image is 400 (reason: 300/450*600), which is
      // smaller than the height threshold. Therefore, only resizing is needed.
      TestCase{
          /*input_param=*/gfx::Size{450, 600},
          /*output_param=*/gfx::Size{300, 400},
      },

      // The height of the resized image is 600 (reason: 300/600*1200), which is
      // greater than the height threshold. Therefore, both resizing and
      // cropping are needed to clamp this size.
      TestCase{
          /*input_param=*/gfx::Size{600, 1200},
          /*output_param=*/gfx::Size{300, 500},
      }};

  LargeImageView view(/*max_size=*/{300, 500});
  for (const TestCase& test_case : kTestCases) {
    SkBitmap icon_bitmap;
    const gfx::Size& input_image_size = test_case.input;
    icon_bitmap.allocN32Pixels(input_image_size.width(),
                               input_image_size.height());
    view.SetImage(gfx::ImageSkia::CreateFrom1xBitmap(icon_bitmap));
    EXPECT_EQ(view.GetPreferredSize(), test_case.expected_output);
  }
}

}  // namespace message_center

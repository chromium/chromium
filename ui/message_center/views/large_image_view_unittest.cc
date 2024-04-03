// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/large_image_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace message_center {
namespace {

struct TestCase {
  constexpr TestCase(const gfx::Size& input_param,
                     const gfx::Size& output_param)
      : input(input_param), expected_output(output_param) {}
  const gfx::Size input;
  const gfx::Size expected_output;
};

// Returns a test image of `image_size`.
gfx::ImageSkia CreateTestImageForSize(const gfx::Size& image_size) {
  return gfx::test::CreateImageSkia(image_size.width(), image_size.height());
}

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
    view.SetImage(CreateTestImageForSize(test_case.input));
    EXPECT_EQ(view.GetPreferredSize({}), test_case.expected_output);
  }
}

TEST_F(LargeImageViewTest, VerifyDrawnImage) {
  LargeImageView view(/*max_size=*/{150, 300});
  view.SetBoundsRect(/*bounds=*/{200, 300});
  view.SetImage(CreateTestImageForSize(/*image_size=*/{300, 400}));

  // The original image is processed by the following steps:
  // Step 1: Resize. The size of the resized image is gfx::Size{150, 200}.
  // Step 2: Cap by the max size. gfx::Size{150, 200} is already smaller than
  // the max size.
  // Step 3: Cap by `view` contents size. After this step, the size is still
  // gfx::Size{150, 200}.
  EXPECT_EQ(view.drawn_image().size(), gfx::Size(150, 200));

  // Set `view` with a smaller size.
  view.SetBoundsRect(/*bounds=*/{100, 100});

  // Because only view bounds change, the size of the image before Step 3 is
  // gfx::Size{150, 200}. After being capped by `view` contents size, the final
  // image size is gfx::Size{100, 100}.
  EXPECT_EQ(view.drawn_image().size(), gfx::Size(100, 100));
}

}  // namespace message_center

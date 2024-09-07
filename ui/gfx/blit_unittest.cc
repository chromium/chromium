// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/memory/platform_shared_memory_region.h"
#include "build/build_config.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// A TestPixelMap is a thin shim layer that allows writing a square canvas of
// 32-bit pixels as a flat array of 8-bit value, which makes the tests a bit
// less fiddly to read and maintain. At construction time, each 8-bit value is
// used to fill in each of the ARGB channels of the 32-bit pixels.
struct TestPixelMap {
  static constexpr size_t kWidth = 5;
  static constexpr size_t kHeight = 5;

  static uint32_t ExpandByte(uint8_t b) {
    return (b << 24) | (b << 16) | (b << 8) | b;
  }

  TestPixelMap(std::initializer_list<uint8_t> values) {
    std::transform(values.begin(), values.end(), pixels.begin(),
                   &TestPixelMap::ExpandByte);
  }

  uint32_t AtXY(size_t x, size_t y) const { return pixels[y * kWidth + x]; }

  std::array<uint32_t, kWidth * kHeight> pixels;
};

// Fills the given canvas with the values by duplicating the values into each
// color channel for the corresponding pixel.
//
// Example values = {{0x0, 0x01}, {0x12, 0xFF}} would give a canvas with:
//   0x00000000 0x01010101
//   0x12121212 0xFFFFFFFF
void SetToCanvas(SkCanvas* canvas, const TestPixelMap& values) {
  ASSERT_EQ(TestPixelMap::kHeight,
            base::checked_cast<size_t>(canvas->imageInfo().height()));
  ASSERT_EQ(TestPixelMap::kWidth,
            base::checked_cast<size_t>(canvas->imageInfo().width()));

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(TestPixelMap::kWidth, TestPixelMap::kHeight);
  canvas->writePixels(info, values.pixels.data(), TestPixelMap::kWidth * 4, 0,
                      0);
}

// Checks each pixel in the given canvas and see if it is made up of the given
// values, where each value has been duplicated into each channel of the given
// bitmap (see SetToCanvas above).
void VerifyCanvasValues(SkCanvas* canvas, const TestPixelMap& values) {
  SkBitmap bitmap = skia::ReadPixels(canvas);
  ASSERT_EQ(TestPixelMap::kHeight, base::checked_cast<size_t>(bitmap.height()));
  ASSERT_EQ(TestPixelMap::kWidth, base::checked_cast<size_t>(bitmap.width()));

  for (size_t y = 0; y < TestPixelMap::kHeight; y++) {
    for (size_t x = 0; x < TestPixelMap::kWidth; x++) {
      ASSERT_EQ(values.AtXY(x, y), *bitmap.getAddr32(x, y));
    }
  }
}

}  // namespace

TEST(Blit, ScrollCanvas) {
  const int kCanvasWidth = 5;
  const int kCanvasHeight = 5;
  std::unique_ptr<SkCanvas> canvas =
      skia::CreatePlatformCanvas(kCanvasWidth, kCanvasHeight, false);
  const TestPixelMap initial_values({0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x11,
                                     0x12, 0x13, 0x14, 0x20, 0x21, 0x22, 0x23,
                                     0x24, 0x30, 0x31, 0x32, 0x33, 0x34, 0x40,
                                     0x41, 0x42, 0x43, 0x44});

  SetToCanvas(canvas.get(), initial_values);
  VerifyCanvasValues(canvas.get(), initial_values);

  // Scroll none and make sure it's a NOP.
  gfx::ScrollCanvas(canvas.get(),
                    gfx::Rect(0, 0, kCanvasWidth, kCanvasHeight),
                    gfx::Vector2d(0, 0));
  VerifyCanvasValues(canvas.get(), initial_values);

  // Scroll with a empty clip and make sure it's a NOP.
  gfx::Rect empty_clip(1, 1, 0, 0);
  gfx::ScrollCanvas(canvas.get(), empty_clip, gfx::Vector2d(0, 1));
  VerifyCanvasValues(canvas.get(), initial_values);

  // Scroll the center 3 pixels up one.
  gfx::Rect center_three(1, 1, 3, 3);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(0, -1));
  const TestPixelMap scroll_up_expected(
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x21, 0x22, 0x23,
       0x14, 0x20, 0x31, 0x32, 0x33, 0x24, 0x30, 0x31, 0x32,
       0x33, 0x34, 0x40, 0x41, 0x42, 0x43, 0x44});
  VerifyCanvasValues(canvas.get(), scroll_up_expected);

  // Reset and scroll the center 3 pixels down one.
  SetToCanvas(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(0, 1));
  const TestPixelMap scroll_down_expected(
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x11, 0x12, 0x13,
       0x14, 0x20, 0x11, 0x12, 0x13, 0x24, 0x30, 0x21, 0x22,
       0x23, 0x34, 0x40, 0x41, 0x42, 0x43, 0x44});
  VerifyCanvasValues(canvas.get(), scroll_down_expected);

  // Reset and scroll the center 3 pixels right one.
  SetToCanvas(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(1, 0));
  const TestPixelMap scroll_right_expected(
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x11, 0x11, 0x12,
       0x14, 0x20, 0x21, 0x21, 0x22, 0x24, 0x30, 0x31, 0x31,
       0x32, 0x34, 0x40, 0x41, 0x42, 0x43, 0x44});
  VerifyCanvasValues(canvas.get(), scroll_right_expected);

  // Reset and scroll the center 3 pixels left one.
  SetToCanvas(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(-1, 0));
  const TestPixelMap scroll_left_expected(
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x12, 0x13, 0x13,
       0x14, 0x20, 0x22, 0x23, 0x23, 0x24, 0x30, 0x32, 0x33,
       0x33, 0x34, 0x40, 0x41, 0x42, 0x43, 0x44});
  VerifyCanvasValues(canvas.get(), scroll_left_expected);

  // Diagonal scroll.
  SetToCanvas(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(2, 2));
  const TestPixelMap scroll_diagonal_expected(
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x11, 0x12, 0x13,
       0x14, 0x20, 0x21, 0x22, 0x23, 0x24, 0x30, 0x31, 0x32,
       0x11, 0x34, 0x40, 0x41, 0x42, 0x43, 0x44});
  VerifyCanvasValues(canvas.get(), scroll_diagonal_expected);
}

#if BUILDFLAG(IS_WIN)

TEST(Blit, WithSharedMemory) {
  const int kCanvasWidth = 5;
  const int kCanvasHeight = 5;
  base::subtle::PlatformSharedMemoryRegion section =
      base::subtle::PlatformSharedMemoryRegion::CreateWritable(kCanvasWidth *
                                                               kCanvasHeight);
  ASSERT_TRUE(section.IsValid());
  std::unique_ptr<SkCanvas> canvas =
      skia::CreatePlatformCanvasWithSharedSection(
          kCanvasWidth, kCanvasHeight, false, section.GetPlatformHandle(),
          skia::RETURN_NULL_ON_FAILURE);
  ASSERT_TRUE(canvas);
  // Closes a HANDLE associated with |section|, |canvas| must remain valid.
  section = base::subtle::PlatformSharedMemoryRegion();

  const TestPixelMap initial_values({0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x11,
                                     0x12, 0x13, 0x14, 0x20, 0x21, 0x22, 0x23,
                                     0x24, 0x30, 0x31, 0x32, 0x33, 0x34, 0x40,
                                     0x41, 0x42, 0x43, 0x44});
  SetToCanvas(canvas.get(), initial_values);
  VerifyCanvasValues(canvas.get(), initial_values);
}

#endif


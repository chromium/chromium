// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

// Fills the given canvas with the values by duplicating the values into each
// color channel for the corresponding pixel.
//
// Example values = {{0x0, 0x01}, {0x12, 0xFF}} would give a canvas with:
//   0x00000000 0x01010101
//   0x12121212 0xFFFFFFFF
template <int w, int h>
void SetToCanvas(SkCanvas* canvas, uint8_t values[h][w]) {
  ASSERT_EQ(w, canvas->imageInfo().width());
  ASSERT_EQ(h, canvas->imageInfo().height());

  // This wouldn't be necessary if we extended the values in the inputs, but
  // the uint8_t values are a little bit easier to read and maintain.
  uint32_t extendedValues[w*h];
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint8_t value = values[y][x];
      extendedValues[y*w+x] =
          (value << 24) | (value << 16) | (value << 8) | value;
    }
  }

  SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
  canvas->writePixels(info, extendedValues, w*4, 0, 0);
}

// Checks each pixel in the given canvas and see if it is made up of the given
// values, where each value has been duplicated into each channel of the given
// bitmap (see SetToCanvas above).
template <int w, int h>
void VerifyCanvasValues(SkCanvas* canvas, uint8_t values[h][w]) {
  SkBitmap bitmap = skia::ReadPixels(canvas);
  ASSERT_EQ(w, bitmap.width());
  ASSERT_EQ(h, bitmap.height());

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint8_t value = values[y][x];
      uint32_t expected = (value << 24) | (value << 16) | (value << 8) | value;
      ASSERT_EQ(expected, *bitmap.getAddr32(x, y));
    }
  }
}

}  // namespace

TEST(Blit, ScrollCanvas) {
  static const int kCanvasWidth = 5;
  static const int kCanvasHeight = 5;
  std::unique_ptr<SkCanvas> canvas =
      skia::CreatePlatformCanvas(kCanvasWidth, kCanvasHeight, false);
  uint8_t initial_values[kCanvasHeight][kCanvasWidth] = {
      {0x00, 0x01, 0x02, 0x03, 0x04},
      {0x10, 0x11, 0x12, 0x13, 0x14},
      {0x20, 0x21, 0x22, 0x23, 0x24},
      {0x30, 0x31, 0x32, 0x33, 0x34},
      {0x40, 0x41, 0x42, 0x43, 0x44}};
  SetToCanvas<5, 5>(canvas.get(), initial_values);

  // Sanity check on input.
  VerifyCanvasValues<5, 5>(canvas.get(), initial_values);

  // Scroll none and make sure it's a NOP.
  gfx::ScrollCanvas(canvas.get(),
                    gfx::Rect(0, 0, kCanvasWidth, kCanvasHeight),
                    gfx::Vector2d(0, 0));
  VerifyCanvasValues<5, 5>(canvas.get(), initial_values);

  // Scroll with a empty clip and make sure it's a NOP.
  gfx::Rect empty_clip(1, 1, 0, 0);
  gfx::ScrollCanvas(canvas.get(), empty_clip, gfx::Vector2d(0, 1));
  VerifyCanvasValues<5, 5>(canvas.get(), initial_values);

  // Scroll the center 3 pixels up one.
  gfx::Rect center_three(1, 1, 3, 3);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(0, -1));
  uint8_t scroll_up_expected[kCanvasHeight][kCanvasWidth] = {
      {0x00, 0x01, 0x02, 0x03, 0x04},
      {0x10, 0x21, 0x22, 0x23, 0x14},
      {0x20, 0x31, 0x32, 0x33, 0x24},
      {0x30, 0x31, 0x32, 0x33, 0x34},
      {0x40, 0x41, 0x42, 0x43, 0x44}};
  VerifyCanvasValues<5, 5>(canvas.get(), scroll_up_expected);

  // Reset and scroll the center 3 pixels down one.
  SetToCanvas<5, 5>(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(0, 1));
  uint8_t scroll_down_expected[kCanvasHeight][kCanvasWidth] = {
      {0x00, 0x01, 0x02, 0x03, 0x04},
      {0x10, 0x11, 0x12, 0x13, 0x14},
      {0x20, 0x11, 0x12, 0x13, 0x24},
      {0x30, 0x21, 0x22, 0x23, 0x34},
      {0x40, 0x41, 0x42, 0x43, 0x44}};
  VerifyCanvasValues<5, 5>(canvas.get(), scroll_down_expected);

  // Reset and scroll the center 3 pixels right one.
  SetToCanvas<5, 5>(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(1, 0));
  uint8_t scroll_right_expected[kCanvasHeight][kCanvasWidth] = {
      {0x00, 0x01, 0x02, 0x03, 0x04},
      {0x10, 0x11, 0x11, 0x12, 0x14},
      {0x20, 0x21, 0x21, 0x22, 0x24},
      {0x30, 0x31, 0x31, 0x32, 0x34},
      {0x40, 0x41, 0x42, 0x43, 0x44}};
  VerifyCanvasValues<5, 5>(canvas.get(), scroll_right_expected);

  // Reset and scroll the center 3 pixels left one.
  SetToCanvas<5, 5>(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(-1, 0));
  uint8_t scroll_left_expected[kCanvasHeight][kCanvasWidth] = {
      {0x00, 0x01, 0x02, 0x03, 0x04},
      {0x10, 0x12, 0x13, 0x13, 0x14},
      {0x20, 0x22, 0x23, 0x23, 0x24},
      {0x30, 0x32, 0x33, 0x33, 0x34},
      {0x40, 0x41, 0x42, 0x43, 0x44}};
  VerifyCanvasValues<5, 5>(canvas.get(), scroll_left_expected);

  // Diagonal scroll.
  SetToCanvas<5, 5>(canvas.get(), initial_values);
  gfx::ScrollCanvas(canvas.get(), center_three, gfx::Vector2d(2, 2));
  uint8_t scroll_diagonal_expected[kCanvasHeight][kCanvasWidth] = {
      {0x00, 0x01, 0x02, 0x03, 0x04},
      {0x10, 0x11, 0x12, 0x13, 0x14},
      {0x20, 0x21, 0x22, 0x23, 0x24},
      {0x30, 0x31, 0x32, 0x11, 0x34},
      {0x40, 0x41, 0x42, 0x43, 0x44}};
  VerifyCanvasValues<5, 5>(canvas.get(), scroll_diagonal_expected);
}

#if defined(OS_WIN)

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

  uint8_t initial_values[kCanvasHeight][kCanvasWidth] = {
      {0x00, 0x01, 0x02, 0x03, 0x04},
      {0x10, 0x11, 0x12, 0x13, 0x14},
      {0x20, 0x21, 0x22, 0x23, 0x24},
      {0x30, 0x31, 0x32, 0x33, 0x34},
      {0x40, 0x41, 0x42, 0x43, 0x44}};
  SetToCanvas<5, 5>(canvas.get(), initial_values);

  // Sanity check on input.
  VerifyCanvasValues<5, 5>(canvas.get(), initial_values);
}

#endif


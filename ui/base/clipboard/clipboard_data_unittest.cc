// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_data.h"

#include <memory>
#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace ui {

// Tests that two ClipboardData objects won't be equal if they don't have the
// same bitmap.
TEST(ClipboardDataTest, BitmapTest) {
  ClipboardData data1;
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  data1.SetBitmapData(test_bitmap);

  ClipboardData data2;
  EXPECT_NE(data1, data2);

  data2.SetBitmapData(test_bitmap);
  EXPECT_EQ(data1, data2);
}

// Tests that two ClipboardData objects won't be equal if they don't have the
// same data source.
TEST(ClipboardDataTest, DataSrcTest) {
  GURL url("https://www.example.com");
  ClipboardData data1;
  data1.set_source(std::make_optional<DataTransferEndpoint>(url));

  ClipboardData data2;
  EXPECT_NE(data1, data2);

  data2.set_source(std::make_optional<DataTransferEndpoint>(url));
  EXPECT_EQ(data1, data2);
}

TEST(ClipboardDataTest, Equivalence) {
  ClipboardData data1;
  SkBitmap test_bitmap1 = gfx::test::CreateBitmap(3, 2);
  data1.SetBitmapData(test_bitmap1);

  ClipboardData data2(data1);

  // Clipboards are equivalent if they have matching bitmaps.
  EXPECT_EQ(data1, data2);
  EXPECT_TRUE(data1.GetBitmapIfPngNotEncoded().has_value());
  EXPECT_TRUE(data2.GetBitmapIfPngNotEncoded().has_value());

  // The PNGs have not yet been encoded.
  EXPECT_EQ(data1.maybe_png(), std::nullopt);
  EXPECT_EQ(data2.maybe_png(), std::nullopt);

  // Encode the PNG of one of the clipboards.
  auto png1 = clipboard_util::EncodeBitmapToPng(
      data1.GetBitmapIfPngNotEncoded().value());
  data1.SetPngDataAfterEncoding(png1);

  // Comparing the clipboards when only one has an encoded PNG checks the cached
  // bitmap. They should still be equal.
  EXPECT_EQ(data1, data2);

  // Put an already-encoded image on a clipboard.
  ClipboardData data3;
  data3.SetPngData(png1);

  // Comparing a clipboard when one has only an encoded PNG and the other has
  // only a bitmap fails, EVEN IF the bitmap would encode into the same PNG.
  EXPECT_NE(data2, data3);
  // Comparing clipboards with the same encoded PNG succeeds as expected.
  EXPECT_EQ(data1, data3);
}

}  // namespace ui

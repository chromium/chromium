// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_data.h"

#include <memory>

#include "base/strings/string_piece_forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui {

// Tests that two ClipboardData objects won't be equal if they don't have the
// same bitmap.
TEST(ClipboardDataTest, BitMapTest) {
  ClipboardData data1;
  SkBitmap test_bitmap;
  test_bitmap.allocN32Pixels(3, 2);
  test_bitmap.eraseARGB(255, 0, 255, 0);
  data1.SetBitmapData(test_bitmap);

  ClipboardData data2;
  EXPECT_NE(data1, data2);

  data2.SetBitmapData(test_bitmap);
  EXPECT_EQ(data1, data2);
}

// Tests that two ClipboardData objects won't be equal if they don't have the
// same data source.
TEST(ClipboardDataTest, DataSrcTest) {
  url::Origin origin(url::Origin::Create(GURL("www.example.com")));
  ClipboardData data1;
  data1.set_source(std::make_unique<DataTransferEndpoint>(origin));

  ClipboardData data2;
  EXPECT_NE(data1, data2);

  data2.set_source(std::make_unique<DataTransferEndpoint>(origin));
  EXPECT_EQ(data1, data2);
}

}  // namespace ui
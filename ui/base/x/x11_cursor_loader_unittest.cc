// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_loader.h"

#undef Bool

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/byte_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {
std::vector<XCursorLoader::Image> ParseFile(base::span<const uint32_t> data,
                                            uint32_t preferred_size) {
  std::vector<uint8_t> vec(data.size() * 4u);
  for (size_t i = 0; i < data.size(); ++i) {
    auto bytes = base::span(vec).subspan(i * 4u).first<4u>();
    bytes.copy_from(base::numerics::U32ToLittleEndian(data[i]));
  }
  return ParseCursorFile(base::RefCountedBytes::TakeVector(&vec),
                         preferred_size);
}

}  // namespace

TEST(XCursorLoaderTest, Basic) {
  std::vector<uint32_t> file{
      // magic number
      0x72756358,
      // bytes in header
      28,
      // version
      1,
      // number of TOC entries
      1,

      // type (image)
      0xfffd0002,
      // subtype (image size)
      1,
      // position
      28,

      // bytes in header
      16,
      // chunk type (image)
      0xfffd0002,
      // chunk subtype (image size)
      1,
      // chunk type version
      1,
      // image width
      1,
      // image height
      1,
      // xhot
      1234,
      // yhot
      5678,
      // delay
      123,
      // chunk data (ARGB image)
      0xff123456,
  };
  auto images = ParseFile(file, 1);
  ASSERT_EQ(images.size(), 1ul);
  EXPECT_EQ(images[0].frame_delay.InMilliseconds(), 123);
  EXPECT_EQ(images[0].bitmap.width(), 1);
  EXPECT_EQ(images[0].bitmap.height(), 1);
  EXPECT_EQ(images[0].hotspot.x(), 1234);
  EXPECT_EQ(images[0].hotspot.y(), 5678);
  EXPECT_EQ(images[0].bitmap.getColor(0, 0), 0xff123456);
}

TEST(XCursorLoaderTest, BestSize) {
  std::vector<uint32_t> file{
      // magic number
      0x72756358,
      // bytes in header
      28,
      // version
      1,
      // number of TOC entries
      3,

      // type (image)
      0xfffd0002,
      // subtype (image size)
      1,
      // position
      52,

      // type (image)
      0xfffd0002,
      // subtype (image size)
      2,
      // position
      92,

      // type (image)
      0xfffd0002,
      // subtype (image size)
      3,
      // position
      144,

      // bytes in header
      16,
      // chunk type (image)
      0xfffd0002,
      // chunk subtype (image size)
      1,
      // chunk type version
      1,
      // image width
      1,
      // image height
      1,
      // xhot
      0,
      // yhot
      0,
      // delay
      0,
      // chunk data (ARGB image)
      0xffffffff,

      // bytes in header
      16,
      // chunk type (image)
      0xfffd0002,
      // chunk subtype (image size)
      2,
      // chunk type version
      1,
      // image width
      2,
      // image height
      2,
      // xhot
      0,
      // yhot
      0,
      // delay
      0,
      // chunk data (ARGB image)
      0xffffffff,
      0xffffffff,
      0xffffffff,
      0xffffffff,

      // bytes in header
      16,
      // chunk type (image)
      0xfffd0002,
      // chunk subtype (image size)
      3,
      // chunk type version
      1,
      // image width
      3,
      // image height
      3,
      // xhot
      0,
      // yhot
      0,
      // delay
      0,
      // chunk data (ARGB image)
      0xffffffff,
      0xffffffff,
      0xffffffff,
      0xffffffff,
      0xffffffff,
      0xffffffff,
      0xffffffff,
      0xffffffff,
      0xffffffff,
  };
  auto images = ParseFile(file, 2);
  ASSERT_EQ(images.size(), 1ul);
  EXPECT_EQ(images[0].bitmap.width(), 2);
  EXPECT_EQ(images[0].bitmap.height(), 2);
}

TEST(XCursorLoaderTest, Animated) {
  std::vector<uint32_t> file{
      // magic number
      0x72756358,
      // bytes in header
      28,
      // version
      1,
      // number of TOC entries
      2,

      // type (image)
      0xfffd0002,
      // subtype (image size)
      1,
      // position
      40,

      // type (image)
      0xfffd0002,
      // subtype (image size)
      1,
      // position
      80,

      // bytes in header
      16,
      // chunk type (image)
      0xfffd0002,
      // chunk subtype (image size)
      1,
      // chunk type version
      1,
      // image width
      1,
      // image height
      1,
      // xhot
      0,
      // yhot
      0,
      // delay
      500,
      // chunk data (ARGB image)
      0xff123456,

      // bytes in header
      16,
      // chunk type (image)
      0xfffd0002,
      // chunk subtype (image size)
      1,
      // chunk type version
      1,
      // image width
      1,
      // image height
      1,
      // xhot
      0,
      // yhot
      0,
      // delay
      500,
      // chunk data (ARGB image)
      0xff123456,
  };
  auto images = ParseFile(file, 1);
  ASSERT_EQ(images.size(), 2ul);
  EXPECT_EQ(images[0].frame_delay.InMilliseconds(), 500);
  EXPECT_EQ(images[1].frame_delay.InMilliseconds(), 500);
}

}  // namespace ui

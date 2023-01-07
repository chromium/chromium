// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/buffer_w_stream.h"

#include <stdint.h>
#include <algorithm>
#include <vector>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace {

using BufferWStreamTest = testing::Test;

TEST(BufferWStreamTest, WriteBuffer) {
  BufferWStream stream;

  std::vector<uint8_t> buffer;
  for (uint8_t i = 0; i < 10; i++) {
    buffer.push_back(i);
  }
  ASSERT_TRUE(stream.write(buffer.data(), buffer.size()));
  std::vector<uint8_t> buffer_from_stream = stream.TakeBuffer();
  EXPECT_TRUE(base::ranges::equal(buffer, buffer_from_stream));

  std::vector<uint8_t> buffer2;
  for (uint8_t i = 5; i > 0; i--) {
    buffer2.push_back(i);
  }

  // Checks that the stream is cleared after TakeBuffer() so that it won't
  // contain old data.
  ASSERT_TRUE(stream.write(buffer2.data(), buffer2.size()));
  buffer_from_stream = stream.TakeBuffer();
  EXPECT_TRUE(base::ranges::equal(buffer2, buffer_from_stream));

  // Checks that calling write() twice works expectedly.
  ASSERT_TRUE(stream.write(buffer.data(), buffer.size()));
  ASSERT_TRUE(stream.write(buffer2.data(), buffer2.size()));
  buffer_from_stream = stream.TakeBuffer();
  EXPECT_TRUE(base::ranges::equal(buffer.begin(), buffer.end(),
                                  buffer_from_stream.begin(),
                                  buffer_from_stream.begin() + buffer.size()));
  EXPECT_TRUE(base::ranges::equal(buffer2.begin(), buffer2.end(),
                                  buffer_from_stream.begin() + buffer.size(),
                                  buffer_from_stream.end()));
}

}  // namespace
}  // namespace gfx

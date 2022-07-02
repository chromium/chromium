// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"

#include "puffin/memory_stream.h"
#include "puffin/src/include/puffin/brotli_util.h"
#include "puffin/src/puffin_stream.h"

namespace puffin {

namespace {

// echo "puffin test" | xxd -i
const Buffer kTestString = {0x70, 0x75, 0x66, 0x66, 0x69, 0x6e,
                            0x20, 0x74, 0x65, 0x73, 0x74, 0x0a};
}  // namespace

TEST(BrotliUtilTest, CompressAndDecompressTest) {
  Buffer compressed;
  ASSERT_TRUE(BrotliEncode(kTestString.data(), kTestString.size(),
                           MemoryStream::CreateForWrite(&compressed)));
  ASSERT_FALSE(compressed.empty());

  Buffer decompressed;
  ASSERT_TRUE(BrotliDecode(compressed.data(), compressed.size(),
                           MemoryStream::CreateForWrite(&decompressed)));
  ASSERT_EQ(kTestString, decompressed);
}

}  // namespace puffin

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/jpeg_codec.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

namespace {

// A JPEG image used by TopSitesMigrationTest, whose size is 1x1.
// This image causes an invalid-read error to libjpeg-turbo 1.0.1.
constexpr uint8_t kTopSitesMigrationTestImage[] =
    "\xff\xd8\xff\xe0\x00\x10\x4a\x46\x49\x46\x00\x01\x01\x00\x00\x01"
    "\x00\x01\x00\x00\xff\xdb\x00\x43\x00\x03\x02\x02\x03\x02\x02\x03"
    "\x03\x03\x03\x04\x03\x03\x04\x05\x08\x05\x05\x04\x04\x05\x0a\x07"
    "\x07\x06\x08\x0c\x0a\x0c\x0c\x0b\x0a\x0b\x0b\x0d\x0e\x12\x10\x0d"
    "\x0e\x11\x0e\x0b\x0b\x10\x16\x10\x11\x13\x14\x15\x15\x15\x0c\x0f"
    "\x17\x18\x16\x14\x18\x12\x14\x15\x14\xff\xdb\x00\x43\x01\x03\x04"
    "\x04\x05\x04\x05\x09\x05\x05\x09\x14\x0d\x0b\x0d\x14\x14\x14\x14"
    "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14"
    "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14"
    "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\xff\xc0"
    "\x00\x11\x08\x00\x01\x00\x01\x03\x01\x22\x00\x02\x11\x01\x03\x11"
    "\x01\xff\xc4\x00\x1f\x00\x00\x01\x05\x01\x01\x01\x01\x01\x01\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09"
    "\x0a\x0b\xff\xc4\x00\xb5\x10\x00\x02\x01\x03\x03\x02\x04\x03\x05"
    "\x05\x04\x04\x00\x00\x01\x7d\x01\x02\x03\x00\x04\x11\x05\x12\x21"
    "\x31\x41\x06\x13\x51\x61\x07\x22\x71\x14\x32\x81\x91\xa1\x08\x23"
    "\x42\xb1\xc1\x15\x52\xd1\xf0\x24\x33\x62\x72\x82\x09\x0a\x16\x17"
    "\x18\x19\x1a\x25\x26\x27\x28\x29\x2a\x34\x35\x36\x37\x38\x39\x3a"
    "\x43\x44\x45\x46\x47\x48\x49\x4a\x53\x54\x55\x56\x57\x58\x59\x5a"
    "\x63\x64\x65\x66\x67\x68\x69\x6a\x73\x74\x75\x76\x77\x78\x79\x7a"
    "\x83\x84\x85\x86\x87\x88\x89\x8a\x92\x93\x94\x95\x96\x97\x98\x99"
    "\x9a\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xb2\xb3\xb4\xb5\xb6\xb7"
    "\xb8\xb9\xba\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xd2\xd3\xd4\xd5"
    "\xd6\xd7\xd8\xd9\xda\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xf1"
    "\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xff\xc4\x00\x1f\x01\x00\x03"
    "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x00\x00\x00\x00\x00\x00\x01"
    "\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\xff\xc4\x00\xb5\x11\x00"
    "\x02\x01\x02\x04\x04\x03\x04\x07\x05\x04\x04\x00\x01\x02\x77\x00"
    "\x01\x02\x03\x11\x04\x05\x21\x31\x06\x12\x41\x51\x07\x61\x71\x13"
    "\x22\x32\x81\x08\x14\x42\x91\xa1\xb1\xc1\x09\x23\x33\x52\xf0\x15"
    "\x62\x72\xd1\x0a\x16\x24\x34\xe1\x25\xf1\x17\x18\x19\x1a\x26\x27"
    "\x28\x29\x2a\x35\x36\x37\x38\x39\x3a\x43\x44\x45\x46\x47\x48\x49"
    "\x4a\x53\x54\x55\x56\x57\x58\x59\x5a\x63\x64\x65\x66\x67\x68\x69"
    "\x6a\x73\x74\x75\x76\x77\x78\x79\x7a\x82\x83\x84\x85\x86\x87\x88"
    "\x89\x8a\x92\x93\x94\x95\x96\x97\x98\x99\x9a\xa2\xa3\xa4\xa5\xa6"
    "\xa7\xa8\xa9\xaa\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xc2\xc3\xc4"
    "\xc5\xc6\xc7\xc8\xc9\xca\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xe2"
    "\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9"
    "\xfa\xff\xda\x00\x0c\x03\x01\x00\x02\x11\x03\x11\x00\x3f\x00\xf9"
    "\xd2\x8a\x28\xaf\xc3\x0f\xf5\x4c\xff\xd9";

// This is a copy of the above JPEG, with the Start-of-Frame header manually
// rewritten to indicate an image size of 25000x25000. An image this large would
// require more than 2GB of RAM to decode, so the decoder will reject the image
// as soon as the header is parsed.
constexpr uint8_t kExtremelyLargeTestImage[] =
    "\xff\xd8\xff\xe0\x00\x10\x4a\x46\x49\x46\x00\x01\x01\x00\x00\x01"
    "\x00\x01\x00\x00\xff\xdb\x00\x43\x00\x03\x02\x02\x03\x02\x02\x03"
    "\x03\x03\x03\x04\x03\x03\x04\x05\x08\x05\x05\x04\x04\x05\x0a\x07"
    "\x07\x06\x08\x0c\x0a\x0c\x0c\x0b\x0a\x0b\x0b\x0d\x0e\x12\x10\x0d"
    "\x0e\x11\x0e\x0b\x0b\x10\x16\x10\x11\x13\x14\x15\x15\x15\x0c\x0f"
    "\x17\x18\x16\x14\x18\x12\x14\x15\x14\xff\xdb\x00\x43\x01\x03\x04"
    "\x04\x05\x04\x05\x09\x05\x05\x09\x14\x0d\x0b\x0d\x14\x14\x14\x14"
    "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14"
    "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14"
    "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\xff\xc0"
    "\x00\x11\x08\x61\xa8\x61\xa8\x03\x01\x22\x00\x02\x11\x01\x03\x11"
    //             ^^  ^^  ^^  ^^   image size forced to 25000x25000
    "\x01\xff\xc4\x00\x1f\x00\x00\x01\x05\x01\x01\x01\x01\x01\x01\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09"
    "\x0a\x0b\xff\xc4\x00\xb5\x10\x00\x02\x01\x03\x03\x02\x04\x03\x05"
    "\x05\x04\x04\x00\x00\x01\x7d\x01\x02\x03\x00\x04\x11\x05\x12\x21"
    "\x31\x41\x06\x13\x51\x61\x07\x22\x71\x14\x32\x81\x91\xa1\x08\x23"
    "\x42\xb1\xc1\x15\x52\xd1\xf0\x24\x33\x62\x72\x82\x09\x0a\x16\x17"
    "\x18\x19\x1a\x25\x26\x27\x28\x29\x2a\x34\x35\x36\x37\x38\x39\x3a"
    "\x43\x44\x45\x46\x47\x48\x49\x4a\x53\x54\x55\x56\x57\x58\x59\x5a"
    "\x63\x64\x65\x66\x67\x68\x69\x6a\x73\x74\x75\x76\x77\x78\x79\x7a"
    "\x83\x84\x85\x86\x87\x88\x89\x8a\x92\x93\x94\x95\x96\x97\x98\x99"
    "\x9a\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xb2\xb3\xb4\xb5\xb6\xb7"
    "\xb8\xb9\xba\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xd2\xd3\xd4\xd5"
    "\xd6\xd7\xd8\xd9\xda\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xf1"
    "\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xff\xc4\x00\x1f\x01\x00\x03"
    "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x00\x00\x00\x00\x00\x00\x01"
    "\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\xff\xc4\x00\xb5\x11\x00"
    "\x02\x01\x02\x04\x04\x03\x04\x07\x05\x04\x04\x00\x01\x02\x77\x00"
    "\x01\x02\x03\x11\x04\x05\x21\x31\x06\x12\x41\x51\x07\x61\x71\x13"
    "\x22\x32\x81\x08\x14\x42\x91\xa1\xb1\xc1\x09\x23\x33\x52\xf0\x15"
    "\x62\x72\xd1\x0a\x16\x24\x34\xe1\x25\xf1\x17\x18\x19\x1a\x26\x27"
    "\x28\x29\x2a\x35\x36\x37\x38\x39\x3a\x43\x44\x45\x46\x47\x48\x49"
    "\x4a\x53\x54\x55\x56\x57\x58\x59\x5a\x63\x64\x65\x66\x67\x68\x69"
    "\x6a\x73\x74\x75\x76\x77\x78\x79\x7a\x82\x83\x84\x85\x86\x87\x88"
    "\x89\x8a\x92\x93\x94\x95\x96\x97\x98\x99\x9a\xa2\xa3\xa4\xa5\xa6"
    "\xa7\xa8\xa9\xaa\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xc2\xc3\xc4"
    "\xc5\xc6\xc7\xc8\xc9\xca\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xe2"
    "\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9"
    "\xfa\xff\xda\x00\x0c\x03\x01\x00\x02\x11\x03\x11\x00\x3f\x00\xf9"
    "\xd2\x8a\x28\xaf\xc3\x0f\xf5\x4c\xff\xd9";

// out of 100, this indicates how compressed it will be, this should be changed
// with jpeg equality threshold
// constexpr int kJpegQuality = 75;  // FIXME(brettw)
constexpr int kJpegQuality = 100;

// The threshold of average color differences where we consider two images
// equal. This number was picked to be a little above the observed difference
// using the above quality.
constexpr double kJpegEqualityThreshold = 1.0;

// Computes the average difference between each value in a and b. A and b
// should be the same size. Used to see if two images are approximately equal
// in the presence of compression.
double AveragePixelDelta(base::span<const uint8_t> a,
                         base::span<const uint8_t> b) {
  // If the sizes are different, say the average difference is the maximum.
  if (a.size() != b.size()) {
    return 255.0;
  }
  if (a.empty()) {
    return 0;  // prevent divide by 0 below
  }

  double acc = 0.0;
  for (size_t i = 0; i < a.size(); i++) {
    acc += fabs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
  }

  return acc / static_cast<double>(a.size());
}

std::vector<uint8_t> MakeRGBAImage(int w, int h) {
  std::vector<uint8_t> result;
  result.resize(w * h * 4);

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      size_t base = (y * w + x) * 4;
      result[base] = x * 3;          // r
      result[base + 1] = x * 3 + 1;  // g
      result[base + 2] = x * 3 + 2;  // b
      result[base + 3] = 0xFF;       // a
    }
  }

  return result;
}

}  // namespace

TEST(JPEGCodec, EncodeDecodeRGBA) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // Create an image with known values. It must be opaque because alpha will be
  // lost during compression.
  std::vector<uint8_t> original = MakeRGBAImage(kWidth, kHeight);

  // Encode, making sure it was compressed some.
  SkImageInfo info = SkImageInfo::Make(kWidth, kHeight, kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType);
  SkPixmap src(info, &original[0], kWidth * 4);
  std::optional<std::vector<uint8_t>> encoded =
      JPEGCodec::Encode(src, kJpegQuality);
  ASSERT_TRUE(encoded);
  EXPECT_GT(original.size(), encoded->size());

  // Decode; it should have the same size as the original.
  std::vector<uint8_t> decoded;
  int outw, outh;
  EXPECT_TRUE(JPEGCodec::Decode(encoded->data(), encoded->size(),
                                kRGBA_8888_SkColorType, &decoded, &outw,
                                &outh));
  ASSERT_EQ(kWidth, outw);
  ASSERT_EQ(kHeight, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be approximately equal (compression will have introduced some
  // minor artifacts).
  ASSERT_GE(kJpegEqualityThreshold, AveragePixelDelta(original, decoded));
}

// Test that corrupted data decompression causes failures.
TEST(JPEGCodec, DecodeCorrupted) {
  constexpr int kWidth = 20;
  constexpr int kHeight = 20;

  // Some random data (an uncompressed image).
  std::vector<uint8_t> original = MakeRGBAImage(kWidth, kHeight);

  // Decoding should fail when given non-JPEG compressed data.
  std::vector<unsigned char> output;
  int outw, outh;
  ASSERT_FALSE(JPEGCodec::Decode(&original[0], original.size(),
                                 kRGBA_8888_SkColorType, &output, &outw,
                                 &outh));

  // Make some compressed data.
  SkImageInfo info = SkImageInfo::Make(kWidth, kHeight, kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType);
  SkPixmap src(info, &original[0], kWidth * 4);
  std::optional<std::vector<uint8_t>> compressed =
      JPEGCodec::Encode(src, kJpegQuality);
  ASSERT_TRUE(compressed);

  // Try decompressing a truncated version.
  ASSERT_FALSE(JPEGCodec::Decode(compressed->data(), compressed->size() / 2,
                                 kRGBA_8888_SkColorType, &output, &outw,
                                 &outh));

  // Corrupt it and try decompressing that.
  for (int i = 10; i < 30; i++) {
    compressed.value()[i] = i;
  }
  ASSERT_FALSE(JPEGCodec::Decode(compressed->data(), compressed->size(),
                                 kRGBA_8888_SkColorType, &output, &outw,
                                 &outh));
}

// Test that we can decode JPEG images without invalid-read errors on
// sanitizers. This test decodes a 1x1 JPEG image and writes the decoded RGB (or
// RGBA) pixel to the output buffer without OOB reads.
TEST(JPEGCodec, InvalidRead) {
  std::vector<uint8_t> output;
  int outw, outh;
  ASSERT_TRUE(JPEGCodec::Decode(kTopSitesMigrationTestImage,
                                std::size(kTopSitesMigrationTestImage),
                                kRGBA_8888_SkColorType, &output, &outw, &outh));
}

// Coverage for data races in JPEG encoding when run with TSan.
// Regression test for https://crbug.com/40120159.
TEST(JPEGCodec, ParallelEncoding) {
  constexpr int kImageSize = 32;
  std::vector<uint8_t> image_data = MakeRGBAImage(kImageSize, kImageSize);
  SkImageInfo info = SkImageInfo::Make(
      kImageSize, kImageSize, kRGBA_8888_SkColorType, kOpaque_SkAlphaType);
  SkPixmap src(info, &image_data[0], kImageSize * 4);

  base::test::TaskEnvironment task_environment;
  base::RunLoop encode_loop;

  constexpr int kNumCopies = 100;
  base::RepeatingClosure encode_completion_closure =
      base::BarrierClosure(kNumCopies, encode_loop.QuitClosure());
  for (int i = 0; i < kNumCopies; ++i) {
    base::ThreadPool::PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                                 std::optional<std::vector<uint8_t>> output =
                                     JPEGCodec::Encode(src, kJpegQuality);
                                 ASSERT_TRUE(output);
                                 encode_completion_closure.Run();
                               }));
  }

  encode_loop.Run();
}

TEST(JPEGCodec, ExtremelyLargeImage) {
  std::vector<unsigned char> output;
  int outw, outh;
  bool ok = JPEGCodec::Decode(kExtremelyLargeTestImage,
                              std::size(kExtremelyLargeTestImage),
                              kRGBA_8888_SkColorType, &output, &outw, &outh);
  EXPECT_FALSE(ok);
}

}  // namespace gfx

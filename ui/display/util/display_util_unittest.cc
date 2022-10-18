// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/display_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/display_test_util.h"
#include "ui/display/util/edid_parser.h"

namespace display {

namespace {

// HP z32x monitor.
const unsigned char kHPz32x[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x32\x01\x01\x01\x01"
    "\x1B\x1B\x01\x04\xB5\x46\x27\x78\x3A\x8D\x15\xAC\x51\x32\xB8\x26"
    "\x0B\x50\x54\x21\x08\x00\xD1\xC0\xA9\xC0\x81\xC0\xD1\x00\xB3\x00"
    "\x95\x00\xA9\x40\x81\x80\x4D\xD0\x00\xA0\xF0\x70\x3E\x80\x30\x20"
    "\x35\x00\xB9\x88\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x18\x3C\x1E"
    "\x87\x3C\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x5A\x33\x32\x78\x0A\x20\x20\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x43\x37\x32\x37\x30\x4D\x57\x30\x0A\x20\x20\x01\x46"
    "\x02\x03\x18\xF1\x4B\x10\x1F\x04\x13\x03\x12\x02\x11\x01\x05\x14"
    "\x23\x09\x07\x07\x83\x01\x00\x00\xA3\x66\x00\xA0\xF0\x70\x1F\x80"
    "\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\x56\x5E\x00\xA0\xA0\xA0"
    "\x29\x50\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\xEF\x51\x00\xA0"
    "\xF0\x70\x19\x80\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\xE2\x68"
    "\x00\xA0\xA0\x40\x2E\x60\x20\x30\x63\x00\xB9\x88\x21\x00\x00\x1C"
    "\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20\x36\x00\xB9\x88\x21\x00"
    "\x00\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x3E";

// Chromebook Samus internal display.
const unsigned char kSamus[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x30\xe4\x2e\x04\x00\x00\x00\x00"
    "\x00\x18\x01\x04\xa5\x1b\x12\x96\x02\x4f\xd5\xa2\x59\x52\x93\x26"
    "\x17\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x6d\x6f\x00\x9e\xa0\xa4\x31\x60\x30\x20"
    "\x3a\x00\x10\xb5\x10\x00\x00\x19\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfe\x00\x4c"
    "\x47\x20\x44\x69\x73\x70\x6c\x61\x79\x0a\x20\x20\x00\x00\x00\xfe"
    "\x00\x4c\x50\x31\x32\x39\x51\x45\x32\x2d\x53\x50\x41\x31\x00\x6c";

// Chromebook Eve internal display.
const unsigned char kEve[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4d\x10\x8a\x14\x00\x00\x00\x00"
    "\x16\x1b\x01\x04\xa5\x1a\x11\x78\x06\xde\x50\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xbb\x62\x60\xa0\x90\x40\x2e\x60\x30\x20"
    "\x3a\x00\x03\xad\x10\x00\x00\x18\x00\x00\x00\x10\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfc"
    "\x00\x4c\x51\x31\x32\x33\x50\x31\x4a\x58\x33\x32\x0a\x20\x00\xb6";

// Invalid EDID: too short to contain chromaticity nor gamma information.
const unsigned char kInvalidEdid[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21";

// Partially valid EDID: gamma information is marked as non existent.
const unsigned char kEdidWithNoGamma[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\xFF\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00";

// Chromebook Samsung Galaxy (kohaku) that supports HDR metadata.
constexpr unsigned char kHDR[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x83\x42\x41\x00\x00\x00\x00"
    "\x13\x1d\x01\x04\xb5\x1d\x11\x78\x02\x38\xd1\xae\x51\x3b\xb8\x23"
    "\x0b\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xb9\xd5\x00\x40\xf1\x70\x20\x80\x30\x20"
    "\x88\x00\x26\xa5\x10\x00\x00\x1b\xb9\xd5\x00\x40\xf1\x70\x20\x80"
    "\x30\x20\x88\x00\x26\xa5\x10\x00\x00\x1b\x00\x00\x00\x0f\x00\xff"
    "\x09\x3c\xff\x09\x3c\x2c\x80\x00\x00\x00\x00\x00\x00\x00\x00\xfe"
    "\x00\x41\x54\x4e\x41\x33\x33\x54\x50\x30\x34\x2d\x30\x20\x01\xba"
    "\x02\x03\x0f\x00\xe3\x05\x80\x00\xe6\x06\x05\x01\x73\x6d\x07\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xab";

// EDID collected in the wild: valid but with primaries in the wrong order.
const unsigned char kSST210[] =
    "\xff\x00\xff\xff\xff\xff\x00\xff\x2d\x4c\x42\x52\x32\x31\x50\x43"
    "\x0c\x2b\x03\x01\x33\x80\xa2\x20\x56\x2a\x9c\x4e\x50\x5b\x26\x95"
    "\x50\x23\xbf\x59\x80\xef\x80\x81\x59\x61\x59\x45\x59\x31\x40\x31"
    "\x01\x01\x01\x01\x01\x01\x32\x8c\xa0\x40\xb0\x60\x40\x19\x32\x1e"
    "\x00\x13\x44\x06\x00\x21\x1e\x00\x00\x00\xfd\x00\x38\x00\x1e\x55"
    "\x0f\x51\x0a\x00\x20\x20\x20\x20\x20\x20\x00\x00\xfc\x00\x32\x00"
    "\x30\x31\x20\x54\x69\x44\x69\x67\x61\x74\x0a\x6c\x00\x00\xff\x00"
    "\x48\x00\x4b\x34\x41\x54\x30\x30\x32\x38\x0a\x38\x20\x20\xf8\x00";

// EDID of |kSST210| with the order of the primaries corrected. Still invalid
// because the triangle of primaries is too small.
const unsigned char kSST210Corrected[] =
    "\xff\x00\xff\xff\xff\xff\x00\xff\x2d\x4c\x42\x52\x32\x31\x50\x43"
    "\x0c\x2b\x03\x01\x33\x80\xa2\x20\x56\x2a\x9c\x95\x50\x4e\x50\x5b"
    "\x26\x23\xbf\x59\x80\xef\x80\x81\x59\x61\x59\x45\x59\x31\x40\x31"
    "\x01\x01\x01\x01\x01\x01\x32\x8c\xa0\x40\xb0\x60\x40\x19\x32\x1e"
    "\x00\x13\x44\x06\x00\x21\x1e\x00\x00\x00\xfd\x00\x38\x00\x1e\x55"
    "\x0f\x51\x0a\x00\x20\x20\x20\x20\x20\x20\x00\x00\xfc\x00\x32\x00"
    "\x30\x31\x20\x54\x69\x44\x69\x67\x61\x74\x0a\x6c\x00\x00\xff\x00"
    "\x48\x00\x4b\x34\x41\x54\x30\x30\x32\x38\x0a\x38\x20\x20\xf8\x00";

// This EDID produces blue primary coordinates too far off the expected point,
// which would paint blue colors as purple. See https://crbug.com/809909.
const unsigned char kBrokenBluePrimaries[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x83\x4d\x83\x00\x00\x00\x00"
    "\x00\x19\x01\x04\x95\x1d\x10\x78\x0a\xee\x25\xa3\x54\x4c\x99\x29"
    "\x26\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xd2\x37\x80\xa2\x70\x38\x40\x40\x30\x20"
    "\x25\x00\x25\xa5\x10\x00\x00\x1a\xa6\x2c\x80\xa2\x70\x38\x40\x40"
    "\x30\x20\x25\x00\x25\xa5\x10\x00\x00\x1a\x00\x00\x00\xfe\x00\x56"
    "\x59\x54\x39\x36\x80\x31\x33\x33\x48\x4c\x0a\x20\x00\x00\x00\x00";

// This EDID contains Short Audio Descriptors in the Timing Extension
// Data which indicate LPCM, DTS, and DTS-HD audio.
const unsigned char kDTSAudio[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x59\x3a\x39\x10\x01\x01\x01\x01"
    "\x00\x1f\x01\x03\x80\x5e\x35\x78\x2a\x29\xdd\xa5\x57\x35\x9f\x26"
    "\x0e\x47\x4a\xa5\xce\x00\xd1\xc0\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x08\xe8\x00\x30\xf2\x70\x5a\x80\xb0\x58"
    "\x8a\x00\xad\x11\x32\x00\x00\x1e\x56\x5e\x00\xa0\xa0\xa0\x29\x50"
    "\x30\x20\x35\x00\xad\x11\x32\x00\x00\x1e\x00\x00\x00\xfc\x00\x56"
    "\x34\x33\x35\x2d\x4a\x30\x31\x0a\x20\x20\x20\x20\x00\x00\x00\xfd"
    "\x00\x17\x4c\x0f\x8c\x3c\x00\x0a\x20\x20\x20\x20\x20\x20\x01\xb3"
    "\x02\x03\x75\x70\x57\x61\x66\x5f\x64\x5d\x62\x60\x65\x5e\x63\x10"
    "\x22\x20\x1f\x21\x05\x04\x13\x07\x06\x03\x02\x01\x35\x09\x07\x07"
    "\x15\x07\x50\x57\x04\x03\x3d\x07\xc0\x5f\x7e\x07\x67\x7e\x03\x5f"
    "\x7e\x01\x83\x01\x00\x00\x70\x03\x0c\x00\x20\x00\x38\x44\xa0\x5b"
    "\x5b\x00\x80\x01\x02\x03\x04\x6a\xd8\x5d\xc4\x01\x78\x80\x0b\x02"
    "\x00\x00\xeb\x01\x46\xd0\x00\x4d\x4f\x2a\x96\x38\x1f\xbf\xe5\x01"
    "\x8b\x84\x90\x01\xe2\x00\xff\xe2\x0f\xc3\xe6\x06\x0f\x01\x53\x53"
    "\x0a\xe3\x05\xe0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80";

}  // namespace

TEST(DisplayUtilTest, TestValidDisplaySize) {
  EXPECT_FALSE(IsDisplaySizeValid(gfx::Size(10, 10)));
  EXPECT_FALSE(IsDisplaySizeValid(gfx::Size(40, 30)));
  EXPECT_FALSE(IsDisplaySizeValid(gfx::Size(50, 40)));
  EXPECT_FALSE(IsDisplaySizeValid(gfx::Size(160, 90)));
  EXPECT_FALSE(IsDisplaySizeValid(gfx::Size(160, 100)));

  EXPECT_TRUE(IsDisplaySizeValid(gfx::Size(50, 60)));
  EXPECT_TRUE(IsDisplaySizeValid(gfx::Size(100, 70)));
  EXPECT_TRUE(IsDisplaySizeValid(gfx::Size(272, 181)));
}

TEST(DisplayUtilTest, GetColorSpaceFromEdid) {
  base::HistogramTester histogram_tester;

  // Test with HP z32x monitor.
  constexpr SkColorSpacePrimaries expected_hpz32x_primaries = {
      .fRX = 0.673828f,
      .fRY = 0.316406f,
      .fGX = 0.198242f,
      .fGY = 0.719727f,
      .fBX = 0.148438f,
      .fBY = 0.043945f,
      .fWX = 0.313477f,
      .fWY = 0.329102f};
  skcms_Matrix3x3 expected_hpz32x_toXYZ50_matrix;
  expected_hpz32x_primaries.toXYZD50(&expected_hpz32x_toXYZ50_matrix);
  const std::vector<uint8_t> hpz32x_edid(kHPz32x,
                                         kHPz32x + std::size(kHPz32x) - 1);
  const gfx::ColorSpace expected_hpz32x_color_space =
      gfx::ColorSpace::CreateCustom(
          expected_hpz32x_toXYZ50_matrix,
          skcms_TransferFunction({2.2, 1, 0, 0, 0, 0, 0}));
  EXPECT_EQ(expected_hpz32x_color_space.ToString(),
            GetColorSpaceFromEdid(display::EdidParser(hpz32x_edid)).ToString());
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kSuccess),
      1);

  // Test with Chromebook Samus internal display.
  constexpr SkColorSpacePrimaries expected_samus_primaries = {.fRX = 0.633789f,
                                                              .fRY = 0.347656f,
                                                              .fGX = 0.323242f,
                                                              .fGY = 0.577148f,
                                                              .fBX = 0.151367f,
                                                              .fBY = 0.090820f,
                                                              .fWX = 0.313477f,
                                                              .fWY = 0.329102f};
  skcms_Matrix3x3 expected_samus_toXYZ50_matrix;
  expected_samus_primaries.toXYZD50(&expected_samus_toXYZ50_matrix);
  const std::vector<uint8_t> samus_edid(kSamus, kSamus + std::size(kSamus) - 1);
  const gfx::ColorSpace expected_samus_color_space =
      gfx::ColorSpace::CreateCustom(
          expected_samus_toXYZ50_matrix,
          skcms_TransferFunction({2.5, 1, 0, 0, 0, 0, 0}));
  EXPECT_EQ(expected_samus_color_space.ToString(),
            GetColorSpaceFromEdid(display::EdidParser(samus_edid)).ToString());
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kSuccess),
      2);

  // Test with Chromebook Eve internal display. The SkColorSpacePrimaries:
  // SkColorSpacePrimaries expected_eve_primaries = {.fRX = 0.639648f,
  //                                                 .fRY = 0.329102f,
  //                                                 .fGX = 0.299805f,
  //                                                 .fGY = 0.599609f,
  //                                                 .fBX = 0.149414f,
  //                                                 .fBY = 0.059570f,
  //                                                 .fWX = 0.312500f,
  //                                                 .fWY = 0.328125f};
  // are very close to the BT.709/sRGB ones, so they'll be rounded to those.
  const skcms_TransferFunction eve_transfer({2.2, 1, 0, 0, 0, 0, 0});
  const gfx::ColorSpace expected_eve_color_space(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::CUSTOM,
      gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL,
      /*custom_primary_matrix=*/nullptr, &eve_transfer);
  const std::vector<uint8_t> eve_edid(kEve, kEve + std::size(kEve) - 1);
  EXPECT_EQ(expected_eve_color_space.ToString(),
            GetColorSpaceFromEdid(display::EdidParser(eve_edid)).ToString());
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kSuccess),
      3);

  // Test with a display that supports HDR: Chromebook Samsung Galaxy (kohaku).
  constexpr SkColorSpacePrimaries expected_hdr_primaries = {.fRX = 0.67970f,
                                                            .fRY = 0.31930f,
                                                            .fGX = 0.23240f,
                                                            .fGY = 0.71870f,
                                                            .fBX = 0.13965f,
                                                            .fBY = 0.04400f,
                                                            .fWX = 0.31250f,
                                                            .fWY = 0.32910f};
  skcms_Matrix3x3 expected_hdr_toXYZ50_matrix;
  expected_hdr_primaries.toXYZD50(&expected_hdr_toXYZ50_matrix);
  const std::vector<uint8_t> hdr_edid(kHDR, kHDR + std::size(kHDR) - 1);
  const gfx::ColorSpace expected_hdr_color_space =
      gfx::ColorSpace::CreateCustom(expected_hdr_toXYZ50_matrix,
                                    gfx::ColorSpace::TransferID::PQ);
  EXPECT_TRUE(expected_hdr_color_space.IsHDR());
  EXPECT_EQ(expected_hdr_color_space.ToString(),
            GetColorSpaceFromEdid(display::EdidParser(hdr_edid)).ToString());
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kSuccess),
      4);

  // Test with gamma marked as non-existent.
  const std::vector<uint8_t> no_gamma_edid(
      kEdidWithNoGamma, kEdidWithNoGamma + std::size(kEdidWithNoGamma) - 1);
  const gfx::ColorSpace no_gamma_color_space =
      GetColorSpaceFromEdid(display::EdidParser(no_gamma_edid));
  EXPECT_FALSE(no_gamma_color_space.IsValid());
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kErrorBadGamma),
      1);
  histogram_tester.ExpectTotalCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome", 5);
}

TEST(DisplayUtilTest, GetInvalidColorSpaceFromEdid) {
  base::HistogramTester histogram_tester;
  const std::vector<uint8_t> empty_edid;
  EXPECT_EQ(gfx::ColorSpace(),
            GetColorSpaceFromEdid(display::EdidParser(empty_edid)));
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kErrorPrimariesAreaTooSmall),
      1);

  const std::vector<uint8_t> invalid_edid(
      kInvalidEdid, kInvalidEdid + std::size(kInvalidEdid) - 1);
  const gfx::ColorSpace invalid_color_space =
      GetColorSpaceFromEdid(display::EdidParser(invalid_edid));
  EXPECT_FALSE(invalid_color_space.IsValid());
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kErrorPrimariesAreaTooSmall),
      2);

  const std::vector<uint8_t> sst210_edid(kSST210,
                                         kSST210 + std::size(kSST210) - 1);
  const gfx::ColorSpace sst210_color_space =
      GetColorSpaceFromEdid(display::EdidParser(sst210_edid));
  EXPECT_FALSE(sst210_color_space.IsValid()) << sst210_color_space.ToString();
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kErrorBadCoordinates),
      1);

  const std::vector<uint8_t> sst210_edid_2(
      kSST210Corrected, kSST210Corrected + std::size(kSST210Corrected) - 1);
  const gfx::ColorSpace sst210_color_space_2 =
      GetColorSpaceFromEdid(display::EdidParser(sst210_edid_2));
  EXPECT_FALSE(sst210_color_space_2.IsValid())
      << sst210_color_space_2.ToString();
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kErrorPrimariesAreaTooSmall),
      3);

  const std::vector<uint8_t> broken_blue_edid(
      kBrokenBluePrimaries,
      kBrokenBluePrimaries + std::size(kBrokenBluePrimaries) - 1);
  const gfx::ColorSpace broken_blue_color_space =
      GetColorSpaceFromEdid(display::EdidParser(broken_blue_edid));
  EXPECT_FALSE(broken_blue_color_space.IsValid())
      << broken_blue_color_space.ToString();
  histogram_tester.ExpectBucketCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
      static_cast<base::HistogramBase::Sample>(
          EdidColorSpaceChecksOutcome::kErrorBluePrimaryIsBroken),
      1);
  histogram_tester.ExpectTotalCount(
      "DrmUtil.GetColorSpaceFromEdid.ChecksOutcome", 5);
}

TEST(DisplayUtilTest, GetAudioPassthroughFromEdid) {
  const std::vector<uint8_t> audio_edid(kDTSAudio,
                                        kDTSAudio + std::size(kDTSAudio) - 1);
  EXPECT_EQ(display::EdidParser(audio_edid).audio_formats(),
            display::EdidParser::kAudioBitstreamPcmLinear |
                display::EdidParser::kAudioBitstreamDts |
                display::EdidParser::kAudioBitstreamDtsHd);
}

TEST(DisplayUtilTest, MultipleInternalDisplayIds) {
  // using base::flat_set as base::FixedFlatSet with different N are different
  // types.
  const base::flat_set<int64_t> kTestIdsList[] = {
      base::flat_set<int64_t>({1}),
      base::flat_set<int64_t>({2, 3}),
      base::flat_set<int64_t>({4, 5, 6}),
      base::flat_set<int64_t>(
          {7, 10, 100, std::numeric_limits<int64_t>::max()}),
  };

  int64_t from_last_set = 0;
  for (const auto& id_set : kTestIdsList) {
    SetInternalDisplayIds(id_set);
    for (int64_t id : id_set)
      EXPECT_TRUE(IsInternalDisplayId(id));
    EXPECT_FALSE(IsInternalDisplayId(from_last_set));
    from_last_set = *id_set.rbegin();
  }

  // Reset the internal display.
  SetInternalDisplayIds({});
  for (auto id_set : kTestIdsList) {
    for (int64_t id : id_set)
      EXPECT_FALSE(IsInternalDisplayId(id));
  }
}

TEST(DisplayUtilTest, CompareDisplayIdsWithMultipleDisplays) {
  // Internal display is always first.
  EXPECT_TRUE(CompareDisplayIds(10, 12));
  {
    ScopedSetInternalDisplayIds set_internal(10);
    EXPECT_TRUE(CompareDisplayIds(10, 12));
    EXPECT_TRUE(CompareDisplayIds(10, 9));
    EXPECT_TRUE(CompareDisplayIds(10, 15));
    EXPECT_FALSE(CompareDisplayIds(12, 10));
    EXPECT_FALSE(CompareDisplayIds(12, 9));
    EXPECT_TRUE(CompareDisplayIds(12, 15));
  }
  {
    ScopedSetInternalDisplayIds set_internal(12);
    EXPECT_FALSE(CompareDisplayIds(10, 12));
    EXPECT_FALSE(CompareDisplayIds(10, 9));
    EXPECT_TRUE(CompareDisplayIds(10, 15));
    EXPECT_TRUE(CompareDisplayIds(12, 10));
    EXPECT_TRUE(CompareDisplayIds(12, 9));
    EXPECT_TRUE(CompareDisplayIds(12, 15));
  }
  // Internal displays are always first but compares values between internal
  // displays.
  {
    ScopedSetInternalDisplayIds set_internal({12, 10});
    EXPECT_TRUE(CompareDisplayIds(10, 12));
    EXPECT_TRUE(CompareDisplayIds(10, 9));
    EXPECT_TRUE(CompareDisplayIds(10, 15));
    EXPECT_FALSE(CompareDisplayIds(12, 10));
    EXPECT_TRUE(CompareDisplayIds(12, 9));
    EXPECT_TRUE(CompareDisplayIds(12, 15));
  }
}

}  // namespace display

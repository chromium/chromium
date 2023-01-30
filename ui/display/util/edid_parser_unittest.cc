// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/edid_parser.h"

#include <stdint.h>

#include <memory>

#include "base/containers/flat_set.h"
#include "base/hash/md5.h"
#include "base/numerics/ranges.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size.h"

using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace display {

namespace {

// EDID with non-ascii char in display name.
constexpr unsigned char kBadDisplayName[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x00\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";
constexpr size_t kBadDisplayNameLength = std::size(kBadDisplayName);

// Sample EDID data extracted from real devices.
constexpr unsigned char kNormalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";
constexpr size_t kNormalDisplayLength = std::size(kNormalDisplay);

// Max image display is an optional field and is omitted in this display by
// setting bytes 21-22 to 0x00.
constexpr unsigned char kNoMaxImageSizeDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x00\x00\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";
constexpr size_t kNoMaxImageSizeDisplayLength =
    std::size(kNoMaxImageSizeDisplay);

// Serial number is in bytes 12-15 of Block 0. Serial number descriptor
// (tag: 0xff) is omitted and replaced by a dummy descriptor (tag: 0x10).
constexpr unsigned char kBlockZeroSerialNumberOnlyDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\x10"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x71";
constexpr size_t kBlockZeroSerialNumberOnlyDisplayLength =
    std::size(kBlockZeroSerialNumberOnlyDisplay);

// Serial number is unavailable. Omitted from bytes 12-15 of block zero and SN
// descriptor (tag: 0xff).
constexpr unsigned char kNoSerialNumberDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x00\x00\x00\x00"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\x10"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x71";
constexpr size_t kNoSerialNumberDisplayLength =
    std::size(kNoSerialNumberDisplay);

// Week of manufacture is optional and is omitted in this display
// (0x00 at byte 16).
constexpr unsigned char kNoWeekOfManufactureDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x00\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";
constexpr size_t kNoWeekOfManufactureDisplayLength =
    std::size(kNoWeekOfManufactureDisplay);

// Week of manufacture can be used to signal that year of manufacture is the
// model year by setting byte 16 to 0xff.
constexpr unsigned char kModelYearDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\xff\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";
constexpr size_t kModelYearDisplayLength = std::size(kModelYearDisplay);

constexpr unsigned char kInternalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\xa3\x42\x31\x00\x00\x00\x00"
    "\x00\x15\x01\x03\x80\x1a\x10\x78\x0a\xd3\xe5\x95\x5c\x60\x90\x27"
    "\x19\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x9e\x1b\x00\xa0\x50\x20\x12\x30\x10\x30"
    "\x13\x00\x05\xa3\x10\x00\x00\x19\x00\x00\x00\x0f\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x23\x87\x02\x64\x00\x00\x00\x00\xfe\x00\x53"
    "\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x00\x00\x00\xfe"
    "\x00\x31\x32\x31\x41\x54\x31\x31\x2d\x38\x30\x31\x0a\x20\x00\x45";
constexpr size_t kInternalDisplayLength = std::size(kInternalDisplay);

constexpr unsigned char kOverscanDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x2d\xfe\x08\x00\x00\x00\x00"
    "\x29\x15\x01\x03\x80\x10\x09\x78\x0a\xee\x91\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\xbd\xef\x80\x71\x4f\x81\xc0\x81\x00\x81\x80\x95\x00"
    "\xa9\xc0\xb3\x00\x01\x01\x02\x3a\x80\x18\x71\x38\x2d\x40\x58\x2c"
    "\x45\x00\xa0\x5a\x00\x00\x00\x1e\x66\x21\x56\xaa\x51\x00\x1e\x30"
    "\x46\x8f\x33\x00\xa0\x5a\x00\x00\x00\x1e\x00\x00\x00\xfd\x00\x18"
    "\x4b\x0f\x51\x17\x00\x0a\x20\x20\x20\x20\x20\x20\x00\x00\x00\xfc"
    "\x00\x53\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x01\x1d"
    "\x02\x03\x1f\xf1\x47\x90\x04\x05\x03\x20\x22\x07\x23\x09\x07\x07"
    "\x83\x01\x00\x00\xe2\x00\x0f\x67\x03\x0c\x00\x20\x00\xb8\x2d\x01"
    "\x1d\x80\x18\x71\x1c\x16\x20\x58\x2c\x25\x00\xa0\x5a\x00\x00\x00"
    "\x9e\x01\x1d\x00\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\xa0\x5a\x00"
    "\x00\x00\x1e\x8c\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\xa0"
    "\x5a\x00\x00\x00\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc6";
constexpr size_t kOverscanDisplayLength = std::size(kOverscanDisplay);

// The EDID info misdetecting overscan once. see crbug.com/226318
constexpr unsigned char kMisdetectedDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x10\xac\x64\x40\x4c\x30\x30\x32"
    "\x0c\x15\x01\x03\x80\x40\x28\x78\xea\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\xa5\x4b\x00\x71\x4f\x81\x00\x81\x80\xd1\x00\xa9\x40"
    "\x01\x01\x01\x01\x01\x01\x28\x3c\x80\xa0\x70\xb0\x23\x40\x30\x20"
    "\x36\x00\x81\x91\x21\x00\x00\x1a\x00\x00\x00\xff\x00\x50\x48\x35"
    "\x4e\x59\x31\x33\x4e\x32\x30\x30\x4c\x0a\x00\x00\x00\xfc\x00\x44"
    "\x45\x4c\x4c\x20\x55\x33\x30\x31\x31\x0a\x20\x20\x00\x00\x00\xfd"
    "\x00\x31\x56\x1d\x5e\x12\x00\x0a\x20\x20\x20\x20\x20\x20\x01\x38"
    "\x02\x03\x29\xf1\x50\x90\x05\x04\x03\x02\x07\x16\x01\x06\x11\x12"
    "\x15\x13\x14\x1f\x20\x23\x0d\x7f\x07\x83\x0f\x00\x00\x67\x03\x0c"
    "\x00\x10\x00\x38\x2d\xe3\x05\x03\x01\x02\x3a\x80\x18\x71\x38\x2d"
    "\x40\x58\x2c\x45\x00\x81\x91\x21\x00\x00\x1e\x01\x1d\x80\x18\x71"
    "\x1c\x16\x20\x58\x2c\x25\x00\x81\x91\x21\x00\x00\x9e\x01\x1d\x00"
    "\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\x81\x91\x21\x00\x00\x1e\x8c"
    "\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\x81\x91\x21\x00\x00"
    "\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x94";
constexpr size_t kMisdetectedDisplayLength = std::size(kMisdetectedDisplay);

constexpr unsigned char kLP2565A[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00"
    "\xA9\x40\xB3\x00\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\xA4";
constexpr size_t kLP2565ALength = std::size(kLP2565A);

constexpr unsigned char kLP2565B[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x6E\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x71\x00\xA9\x00\xA9\x40\xA9\x4F"
    "\xB3\x00\xD1\xC0\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x15\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\x45";
constexpr size_t kLP2565BLength = std::size(kLP2565B);

// HP z32x monitor.
constexpr unsigned char kHPz32x[] =
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
constexpr size_t kHPz32xLength = std::size(kHPz32x);

// Chromebook Samus internal display.
constexpr unsigned char kSamus[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x30\xe4\x2e\x04\x00\x00\x00\x00"
    "\x00\x18\x01\x04\xa5\x1b\x12\x96\x02\x4f\xd5\xa2\x59\x52\x93\x26"
    "\x17\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x6d\x6f\x00\x9e\xa0\xa4\x31\x60\x30\x20"
    "\x3a\x00\x10\xb5\x10\x00\x00\x19\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfe\x00\x4c"
    "\x47\x20\x44\x69\x73\x70\x6c\x61\x79\x0a\x20\x20\x00\x00\x00\xfe"
    "\x00\x4c\x50\x31\x32\x39\x51\x45\x32\x2d\x53\x50\x41\x31\x00\x6c";
constexpr size_t kSamusLength = std::size(kSamus);

// Chromebook Eve internal display.
constexpr unsigned char kEve[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4d\x10\x8a\x14\x00\x00\x00\x00"
    "\x16\x1b\x01\x04\xa5\x1a\x11\x78\x06\xde\x50\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xbb\x62\x60\xa0\x90\x40\x2e\x60\x30\x20"
    "\x3a\x00\x03\xad\x10\x00\x00\x18\x00\x00\x00\x10\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfc"
    "\x00\x4c\x51\x31\x32\x33\x50\x31\x4a\x58\x33\x32\x0a\x20\x00\xb6";
constexpr size_t kEveLength = std::size(kEve);

// A Samsung monitor that supports HDR metadata.
constexpr unsigned char kHDRMetadata[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x2d\xf6\x0d\x00\x0e\x00\x01"
    "\x01\x1b\x01\x03\x80\x5f\x36\x78\x0a\x23\xad\xa4\x54\x4d\x99\x26"
    "\x0f\x47\x4a\xbd\xef\x80\x71\x4f\x81\xc0\x81\x00\x81\x80\x95\x00"
    "\xa9\xc0\xb3\x00\x01\x01\x04\x74\x00\x30\xf2\x70\x5a\x80\xb0\x58"
    "\x8a\x00\x50\x1d\x74\x00\x00\x1e\x02\x3a\x80\x18\x71\x38\x2d\x40"
    "\x58\x2c\x45\x00\x50\x1d\x74\x00\x00\x1e\x00\x00\x00\xfd\x00\x18"
    "\x4b\x0f\x51\x1e\x00\x0a\x20\x20\x20\x20\x20\x20\x00\x00\x00\xfc"
    "\x00\x53\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x01\x5a"
    "\x02\x03\x4f\xf0\x53\x5f\x10\x1f\x04\x13\x05\x14\x20\x21\x22\x5d"
    "\x5e\x62\x63\x64\x07\x16\x03\x12\x2c\x09\x07\x07\x15\x07\x50\x3d"
    "\x04\xc0\x57\x07\x00\x83\x01\x00\x00\xe2\x00\x0f\xe3\x05\x83\x01"
    "\x6e\x03\x0c\x00\x30\x00\xb8\x3c\x20\x00\x80\x01\x02\x03\x04\xe6"
    "\x06\x0d\x01\x73\x6d\x07\x61\x65\x66\xe5\x01\x8b\x84\x90\x01\x01"
    "\x1d\x80\xd0\x72\x1c\x16\x20\x10\x2c\x25\x80\x50\x1d\x74\x00\x00"
    "\x9e\x66\x21\x56\xaa\x51\x00\x1e\x30\x46\x8f\x33\x00\x50\x1d\x74"
    "\x00\x00\x1e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xbd";
constexpr size_t kHDRMetadataLength = std::size(kHDRMetadata);

const std::string kNoSerialNumber = "";
const gfx::Size kNoMaxImageSize = gfx::Size(0, 0);
constexpr uint8_t kNoWeekOfManufactureTag = 0x00;
constexpr uint8_t kModelYearTag = 0xff;
// 16843009 == 0x01010101
const std::string kGenericBlockZeroHashedSerialNumber =
    base::MD5String(std::string("16843009"));
const std::string kNormalDisplayHashedDescriptorBlockSerialNumber =
    base::MD5String(std::string("CN4202137Q"));

// Primaries coordinates ({RX, RY, GX, GY, BX, BY, WX, WY}) calculated by hand
// and rounded to 4 decimal places.
constexpr SkColorSpacePrimaries kNormalDisplayPrimaries = {
    0.6777f, 0.3086f, 0.2100f, 0.6924f, 0.1465f, 0.0547f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kInternalDisplayPrimaries = {
    0.5850f, 0.3604f, 0.3750f, 0.5654f, 0.1553f, 0.0996f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kOverscanDisplayPrimaries = {
    0.6396f, 0.3301f, 0.2998f, 0.5996f, 0.1504f, 0.0596f, 0.3125f, 0.3291f};
constexpr SkColorSpacePrimaries kMisdetectedDisplayPrimaries = {
    0.6777f, 0.3086f, 0.2100f, 0.6924f, 0.1465f, 0.0547f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kLP2565APrimaries = {
    0.6396f, 0.3301f, 0.2998f, 0.6084f, 0.1504f, 0.0596f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kLP2565BPrimaries = {
    0.6396f, 0.3301f, 0.2998f, 0.6084f, 0.1504f, 0.0596f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kHPz32xPrimaries = {
    0.6738f, 0.3164f, 0.1982f, 0.7197f, 0.1484f, 0.0439f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kSamusPrimaries = {
    0.6338f, 0.3477f, 0.3232f, 0.5771f, 0.1514f, 0.0908f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kEvePrimaries = {
    0.6396f, 0.3291f, 0.2998f, 0.5996f, 0.1494f, 0.0596f, 0.3125f, 0.3281f};
constexpr SkColorSpacePrimaries kHDRPrimaries = {
    0.6406f, 0.3300f, 0.3007f, 0.6005f, 0.1503f, 0.0605f, 0.2802f, 0.2900f};

// Chromaticity primaries in EDID are specified with 10 bits precision.
constexpr static float kPrimariesPrecision = 1 / 2048.f;

::testing::AssertionResult SkColorSpacePrimariesEquals(
    const char* lhs_expr,
    const char* rhs_expr,
    const SkColorSpacePrimaries& lhs,
    const SkColorSpacePrimaries& rhs) {
  if (!base::IsApproximatelyEqual(lhs.fRX, rhs.fRX, kPrimariesPrecision))
    return AssertionFailure() << "fRX: " << lhs.fRX << " != " << rhs.fRX;
  if (!base::IsApproximatelyEqual(lhs.fRY, rhs.fRY, kPrimariesPrecision))
    return AssertionFailure() << "fRY: " << lhs.fRY << " != " << rhs.fRY;
  if (!base::IsApproximatelyEqual(lhs.fGX, rhs.fGX, kPrimariesPrecision))
    return AssertionFailure() << "fGX: " << lhs.fGX << " != " << rhs.fGX;
  if (!base::IsApproximatelyEqual(lhs.fGY, rhs.fGY, kPrimariesPrecision))
    return AssertionFailure() << "fGY: " << lhs.fGY << " != " << rhs.fGY;
  if (!base::IsApproximatelyEqual(lhs.fBX, rhs.fBX, kPrimariesPrecision))
    return AssertionFailure() << "fBX: " << lhs.fBX << " != " << rhs.fBX;
  if (!base::IsApproximatelyEqual(lhs.fBY, rhs.fBY, kPrimariesPrecision))
    return AssertionFailure() << "fBY: " << lhs.fBY << " != " << rhs.fBY;
  if (!base::IsApproximatelyEqual(lhs.fWX, rhs.fWX, kPrimariesPrecision))
    return AssertionFailure() << "fWX: " << lhs.fWX << " != " << rhs.fWX;
  if (!base::IsApproximatelyEqual(lhs.fWY, rhs.fWY, kPrimariesPrecision))
    return AssertionFailure() << "fWY: " << lhs.fWY << " != " << rhs.fWY;
  return AssertionSuccess();
}

}  // namespace

struct TestParams {
  uint16_t manufacturer_id;
  uint16_t product_id;
  std::string block_zero_serial_number_hash;
  std::string descriptor_block_serial_number_hash;
  gfx::Size max_image_size;
  std::string display_name;
  gfx::Size active_pixel_size;
  int32_t week_of_manufacture;
  int32_t year_of_manufacture;
  bool overscan_flag;
  double gamma;
  int bits_per_channel;
  SkColorSpacePrimaries primaries;

  uint32_t product_code;
  int64_t index_based_display_id_zero;
  int64_t edid_based_display_id;

  std::string manufacturer_id_string;
  std::string product_id_string;

  base::flat_set<gfx::ColorSpace::PrimaryID> supported_color_primary_ids_;
  base::flat_set<gfx::ColorSpace::TransferID> supported_color_transfer_ids_;
  absl::optional<gfx::HDRStaticMetadata> hdr_static_metadata_;
  absl::optional<gfx::Range> vertical_display_range_limits_;

  const unsigned char* edid_blob;
  size_t edid_blob_length;
} kTestCases[] = {
    {0x22f0u,
     0x6c28u,
     kGenericBlockZeroHashedSerialNumber,
     kNormalDisplayHashedDescriptorBlockSerialNumber,
     gfx::Size(64, 40),
     "HP Z 30w",  // non-ascii char in display name.
     gfx::Size(2560, 1600),
     2,
     2012,
     false,
     2.2,
     10,
     kNormalDisplayPrimaries,
     586181672,
     9834990092472576,
     1713305697,
     "HWP",
     "286C",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kBadDisplayName,
     kBadDisplayNameLength},
    {0x22f0u,
     0x6c28u,
     kGenericBlockZeroHashedSerialNumber,
     kNormalDisplayHashedDescriptorBlockSerialNumber,
     gfx::Size(64, 40),
     "HP ZR30w",
     gfx::Size(2560, 1600),
     2,
     2012,
     false,
     2.2,
     10,
     kNormalDisplayPrimaries,
     586181672,
     9834734971736576,
     51468448,
     "HWP",
     "286C",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kNormalDisplay,
     kNormalDisplayLength},
    {0x22f0u,
     0x6c28u,
     kGenericBlockZeroHashedSerialNumber,
     kNormalDisplayHashedDescriptorBlockSerialNumber,
     kNoMaxImageSize,
     "HP ZR30w",
     gfx::Size(2560, 1600),
     2,
     2012,
     false,
     2.2,
     10,
     kNormalDisplayPrimaries,
     586181672,
     9834734971736576,
     403808854,
     "HWP",
     "286C",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kNoMaxImageSizeDisplay,
     kNoMaxImageSizeDisplayLength},
    {0x22f0u,
     0x6c28u,
     kGenericBlockZeroHashedSerialNumber,
     kNoSerialNumber,
     gfx::Size(64, 40),
     "HP ZR30w",
     gfx::Size(2560, 1600),
     2,
     2012,
     false,
     2.2,
     10,
     kNormalDisplayPrimaries,
     586181672,
     9834734971736576,
     3094128629,
     "HWP",
     "286C",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kBlockZeroSerialNumberOnlyDisplay,
     kBlockZeroSerialNumberOnlyDisplayLength},
    {0x22f0u,
     0x6c28u,
     kNoSerialNumber,
     kNoSerialNumber,
     gfx::Size(64, 40),
     "HP ZR30w",
     gfx::Size(2560, 1600),
     2,
     2012,
     false,
     2.2,
     10,
     kNormalDisplayPrimaries,
     586181672,
     9834734971736576,
     2769865770,
     "HWP",
     "286C",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kNoSerialNumberDisplay,
     kNoSerialNumberDisplayLength},
    {0x22f0u,
     0x6c28u,
     kGenericBlockZeroHashedSerialNumber,
     kNormalDisplayHashedDescriptorBlockSerialNumber,
     gfx::Size(64, 40),
     "HP ZR30w",
     gfx::Size(2560, 1600),
     kNoWeekOfManufactureTag,
     2012,
     false,
     2.2,
     10,
     kNormalDisplayPrimaries,
     586181672,
     9834734971736576,
     4082014303,
     "HWP",
     "286C",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kNoWeekOfManufactureDisplay,
     kNoWeekOfManufactureDisplayLength},
    {0x22f0u,
     0x6c28u,
     kGenericBlockZeroHashedSerialNumber,
     kNormalDisplayHashedDescriptorBlockSerialNumber,
     gfx::Size(64, 40),
     "HP ZR30w",
     gfx::Size(2560, 1600),
     kModelYearTag,
     2012,
     false,
     2.2,
     10,
     kNormalDisplayPrimaries,
     586181672,
     9834734971736576,
     1070357245,
     "HWP",
     "286C",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kModelYearDisplay,
     kModelYearDisplayLength},
    {0x4ca3u,
     0x4231u,
     kNoSerialNumber,
     kNoSerialNumber,
     gfx::Size(26, 16),
     "",
     gfx::Size(1280, 800),
     kNoWeekOfManufactureTag,
     2011,
     false,
     2.2,
     -1,
     kInternalDisplayPrimaries,
     1285767729,
     21571318625337344,
     1646280528,
     "SEC",
     "3142",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kInternalDisplay,
     kInternalDisplayLength},
    {0x4c2du,
     0xfe08u,
     kNoSerialNumber,
     kNoSerialNumber,
     gfx::Size(16, 9),
     "SAMSUNG",
     gfx::Size(1920, 1080),
     41,
     2011,
     true,
     2.2,
     -1,
     kOverscanDisplayPrimaries,
     1278082568,
     21442559853606400,
     3766836601,
     "SAM",
     "08FE",
     {},
     {},
     absl::nullopt,
     gfx::Range(24, 75),
     kOverscanDisplay,
     kOverscanDisplayLength},
    {0x10ACu,
     0x6440u,
     base::MD5String("842018892"),  // == LSB of 0x4c, 0x30, 0x30, 0x32
     base::MD5String("PH5NY13N200L"),
     gfx::Size(64, 40),
     "DELL U3011",
     gfx::Size(1920, 1200),
     12,
     2011,
     false,
     2.2,
     -1,
     kMisdetectedDisplayPrimaries,
     279733312,
     4692848143772416,
     1487444765,
     "DEL",
     "4064",
     {gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::PrimaryID::SMPTE170M},
     {},
     absl::nullopt,
     gfx::Range(49, 86),
     kMisdetectedDisplay,
     kMisdetectedDisplayLength},
    {0x22f0u,
     0x7626u,
     kGenericBlockZeroHashedSerialNumber,
     base::MD5String("CNK80204HM"),
     gfx::Size(52, 33),
     "HP LP2465",
     gfx::Size(1920, 1200),
     2,
     2008,
     false,
     2.2,
     -1,
     kLP2565APrimaries,
     586184230,
     9834630174887424,
     1695949480,
     "HWP",
     "2676",
     {},
     {},
     absl::nullopt,
     gfx::Range(48, 85),
     kLP2565A,
     kLP2565ALength},
    {0x22f0u,
     0x7526u,
     kGenericBlockZeroHashedSerialNumber,
     base::MD5String("CNK80204HM"),
     gfx::Size(52, 33),
     "HP LP2465",
     gfx::Size(1920, 1200),
     2,
     2008,
     false,
     2.2,
     -1,
     kLP2565BPrimaries,
     586183974,
     9834630174887424,
     3357789438,
     "HWP",
     "2675",
     {},
     {},
     absl::nullopt,
     gfx::Range(48, 85),
     kLP2565B,
     kLP2565BLength},
    {0x22f0u,
     0x7532u,
     kGenericBlockZeroHashedSerialNumber,
     base::MD5String("CNC7270MW0"),
     gfx::Size(70, 39),
     "HP Z32x",
     gfx::Size(3840, 2160),
     27,
     2017,
     false,
     2.2,
     10,
     kHPz32xPrimaries,
     586183986,
     9834799315992832,
     129207725,
     "HWP",
     "3275",
     {},
     {},
     absl::nullopt,
     gfx::Range(24, 60),
     kHPz32x,
     kHPz32xLength},
    {0x30E4u,
     0x2E04u,
     kNoSerialNumber,
     kNoSerialNumber,
     gfx::Size(27, 18),
     "",
     gfx::Size(2560, 1700),
     kNoWeekOfManufactureTag,
     2014,
     false,
     2.5,
     8,
     kSamusPrimaries,
     820260356,
     13761487533244416,
     2825178591,
     "LGD",
     "042E",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kSamus,
     kSamusLength},
    {0x4D10u,
     0x8A14u,
     kNoSerialNumber,
     kNoSerialNumber,
     gfx::Size(26, 17),
     "LQ123P1JX32",
     gfx::Size(2400, 1600),
     22,
     2017,
     false,
     2.2,
     8,
     kEvePrimaries,
     1292929556,
     21692109949126656,
     2755351929,
     "SHP",
     "148A",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     kEve,
     kEveLength},
    {19501u,
     62989u,
     base::MD5String("16780800"),  // == LSB of 0x00, 0x0e, 0x00 0x01
     kNoSerialNumber,
     gfx::Size(95, 54),
     "SAMSUNG",
     gfx::Size(3840, 2160),
     1,
     2017,
     true,
     2.2,
     -1,
     kHDRPrimaries,
     1278080525,
     21442559853606400,
     755395064,
     "SAM",
     "0DF6",
     {gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::PrimaryID::SMPTE170M,
      gfx::ColorSpace::PrimaryID::BT2020},
     {gfx::ColorSpace::TransferID::BT709, gfx::ColorSpace::TransferID::PQ,
      gfx::ColorSpace::TransferID::HLG},
     absl::make_optional<gfx::HDRStaticMetadata>(603.666, 530.095, 0.00454),
     gfx::Range(24, 75),
     kHDRMetadata,
     kHDRMetadataLength},

    // Empty Edid, which is tantamount to error.
    {0,
     0,
     kNoSerialNumber,
     kNoSerialNumber,
     gfx::Size(0, 0),
     "",
     gfx::Size(0, 0),
     kNoWeekOfManufactureTag,
     display::kInvalidYearOfManufacture,
     false,
     0.0,
     -1,
     SkColorSpacePrimaries(),
     0,
     0,
     // Not zero because we're still hashing some string of zero/empty values.
     710538554,
     "@@@",
     "0000",
     {},
     {},
     absl::nullopt,
     absl::nullopt,
     nullptr,
     0u},
};

class EDIDParserTest : public TestWithParam<TestParams> {
 public:
  EDIDParserTest()
      : parser_(std::vector<uint8_t>(
            GetParam().edid_blob,
            GetParam().edid_blob + GetParam().edid_blob_length)) {}

  EDIDParserTest(const EDIDParserTest&) = delete;
  EDIDParserTest& operator=(const EDIDParserTest&) = delete;

  const EdidParser parser_;
};

TEST_P(EDIDParserTest, ParseEdids) {
  EXPECT_EQ(parser_.manufacturer_id(), GetParam().manufacturer_id);
  EXPECT_EQ(parser_.product_id(), GetParam().product_id);
  EXPECT_EQ(parser_.block_zero_serial_number_hash(),
            GetParam().block_zero_serial_number_hash);
  EXPECT_EQ(parser_.descriptor_block_serial_number_hash(),
            GetParam().descriptor_block_serial_number_hash);
  EXPECT_EQ(parser_.max_image_size(), GetParam().max_image_size);
  EXPECT_EQ(parser_.display_name(), GetParam().display_name);
  EXPECT_EQ(parser_.active_pixel_size(), GetParam().active_pixel_size);
  EXPECT_EQ(parser_.week_of_manufacture(), GetParam().week_of_manufacture);
  EXPECT_EQ(parser_.year_of_manufacture(), GetParam().year_of_manufacture);
  EXPECT_EQ(parser_.has_overscan_flag(), GetParam().overscan_flag);
  if (parser_.has_overscan_flag())
    EXPECT_EQ(parser_.overscan_flag(), GetParam().overscan_flag);
  EXPECT_DOUBLE_EQ(parser_.gamma(), GetParam().gamma);
  EXPECT_EQ(parser_.bits_per_channel(), GetParam().bits_per_channel);
  EXPECT_PRED_FORMAT2(SkColorSpacePrimariesEquals, parser_.primaries(),
                      GetParam().primaries);

  EXPECT_EQ(parser_.GetProductCode(), GetParam().product_code);
  EXPECT_EQ(parser_.GetIndexBasedDisplayId(0 /* product_index */),
            GetParam().index_based_display_id_zero);
  EXPECT_EQ(parser_.GetEdidBasedDisplayId(), GetParam().edid_based_display_id);

  EXPECT_EQ(EdidParser::ManufacturerIdToString(parser_.manufacturer_id()),
            GetParam().manufacturer_id_string);
  EXPECT_EQ(EdidParser::ProductIdToString(parser_.product_id()),
            GetParam().product_id_string);

  EXPECT_EQ(GetParam().supported_color_primary_ids_,
            parser_.supported_color_primary_ids());
  EXPECT_EQ(GetParam().supported_color_transfer_ids_,
            parser_.supported_color_transfer_ids());

  const absl::optional<gfx::HDRStaticMetadata> hdr_static_metadata =
      parser_.hdr_static_metadata();
  EXPECT_EQ(GetParam().hdr_static_metadata_.has_value(),
            hdr_static_metadata.has_value());
  if (GetParam().hdr_static_metadata_.has_value() &&
      hdr_static_metadata.has_value()) {
    constexpr double epsilon = 0.001;
    EXPECT_NEAR(GetParam().hdr_static_metadata_->max, hdr_static_metadata->max,
                epsilon);
    EXPECT_NEAR(GetParam().hdr_static_metadata_->max_avg,
                hdr_static_metadata->max_avg, epsilon);
    EXPECT_NEAR(GetParam().hdr_static_metadata_->min, hdr_static_metadata->min,
                epsilon);
  }

  const absl::optional<gfx::Range> vertical_display_range_limits =
      parser_.vertical_display_range_limits();
  EXPECT_EQ(GetParam().vertical_display_range_limits_.has_value(),
            vertical_display_range_limits.has_value());
  if (GetParam().vertical_display_range_limits_.has_value() &&
      vertical_display_range_limits.has_value()) {
    EXPECT_EQ(vertical_display_range_limits->start(),
              GetParam().vertical_display_range_limits_->start());
    EXPECT_EQ(vertical_display_range_limits->end(),
              GetParam().vertical_display_range_limits_->end());
  }
}

INSTANTIATE_TEST_SUITE_P(All, EDIDParserTest, ValuesIn(kTestCases));

}  // namespace display

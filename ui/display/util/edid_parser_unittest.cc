// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

// EDID for Dell UP3218K, a 2x1 8K tiled display.
constexpr unsigned char kTiledDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x10\xac\x47\x41\x4c\x34\x37\x41"
    "\x0b\x21\x01\x04\xb5\x46\x27\x78\x3a\x76\x45\xae\x51\x33\xba\x26"
    "\x0d\x50\x54\xa5\x4b\x00\x81\x00\xb3\x00\xd1\x00\xa9\x40\x81\x80"
    "\xd1\xc0\x01\x01\x01\x01\x4d\xd0\x00\xa0\xf0\x70\x3e\x80\x30\x20"
    "\x35\x00\xba\x89\x21\x00\x00\x1a\x00\x00\x00\xff\x00\x4a\x48\x4e"
    "\x34\x4a\x33\x33\x47\x41\x37\x34\x4c\x0a\x00\x00\x00\xfc\x00\x44"
    "\x45\x4c\x4c\x20\x55\x50\x33\x32\x31\x38\x4b\x0a\x00\x00\x00\xfd"
    "\x00\x18\x4b\x1e\xb4\x6c\x01\x0a\x20\x20\x20\x20\x20\x20\x02\x79"
    "\x02\x03\x1d\xf1\x50\x10\x1f\x20\x05\x14\x04\x13\x12\x11\x03\x02"
    "\x16\x15\x07\x06\x01\x23\x09\x1f\x07\x83\x01\x00\x00\xa3\x66\x00"
    "\xa0\xf0\x70\x1f\x80\x30\x20\x35\x00\xba\x89\x21\x00\x00\x1a\x56"
    "\x5e\x00\xa0\xa0\xa0\x29\x50\x30\x20\x35\x00\xba\x89\x21\x00\x00"
    "\x1a\x7c\x39\x00\xa0\x80\x38\x1f\x40\x30\x20\x3a\x00\xba\x89\x21"
    "\x00\x00\x1a\xa8\x16\x00\xa0\x80\x38\x13\x40\x30\x20\x3a\x00\xba"
    "\x89\x21\x00\x00\x1a\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x47"
    "\x70\x12\x79\x00\x00\x12\x00\x16\x82\x10\x10\x00\xff\x0e\xdf\x10"
    "\x00\x00\x00\x00\x00\x44\x45\x4c\x47\x41\x4c\x34\x37\x41\x03\x01"
    "\x50\x70\x92\x01\x84\xff\x1d\xc7\x00\x1d\x80\x09\x00\xdf\x10\x2f"
    "\x00\x02\x00\x04\x00\xc1\x42\x01\x84\xff\x1d\xc7\x00\x2f\x80\x1f"
    "\x00\xdf\x10\x30\x00\x02\x00\x04\x00\xa8\x4e\x01\x04\xff\x0e\xc7"
    "\x00\x2f\x80\x1f\x00\xdf\x10\x61\x00\x02\x00\x09\x00\x97\x9d\x01"
    "\x04\xff\x0e\xc7\x00\x2f\x80\x1f\x00\xdf\x10\x2f\x00\x02\x00\x09"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x78\x90";
constexpr size_t kTiledDisplayLength = std::size(kTiledDisplay);

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
constexpr SkColorSpacePrimaries kDellTiledPrimaries = {
    0.6807f, 0.3193f, 0.2002f, 0.7285f, 0.1494f, 0.0508f, 0.3134f, 0.3291f};

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
  std::string test_name;
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

  base::flat_set<EdidParser::PrimaryMatrixPair>
      supported_color_primary_matrix_ids;
  base::flat_set<gfx::ColorSpace::TransferID> supported_color_transfer_ids;
  std::optional<gfx::HDRStaticMetadata> hdr_static_metadata;
  std::optional<uint16_t> vsync_rate_min;
  bool tile_scale_to_fit;

  const unsigned char* edid_blob;
  size_t edid_blob_length;
} kTestCases[] = {
    {.test_name = "BadDisplayName",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x6c28u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash =
         kNormalDisplayHashedDescriptorBlockSerialNumber,
     .max_image_size = gfx::Size(64, 40),
     .display_name = "HP Z 30w",  // non-ascii char in display name.
     .active_pixel_size = gfx::Size(2560, 1600),
     .week_of_manufacture = 2,
     .year_of_manufacture = 2012,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kNormalDisplayPrimaries,
     .product_code = 586181672,
     .index_based_display_id_zero = 9834990092472576,
     .edid_based_display_id = 1713305697,
     .manufacturer_id_string = "HWP",
     .product_id_string = "286C",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kBadDisplayName,
     .edid_blob_length = kBadDisplayNameLength},
    {.test_name = "NormalDisplay",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x6c28u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash =
         kNormalDisplayHashedDescriptorBlockSerialNumber,
     .max_image_size = gfx::Size(64, 40),
     .display_name = "HP ZR30w",
     .active_pixel_size = gfx::Size(2560, 1600),
     .week_of_manufacture = 2,
     .year_of_manufacture = 2012,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kNormalDisplayPrimaries,
     .product_code = 586181672,
     .index_based_display_id_zero = 9834734971736576,
     .edid_based_display_id = 51468448,
     .manufacturer_id_string = "HWP",
     .product_id_string = "286C",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kNormalDisplay,
     .edid_blob_length = kNormalDisplayLength},
    {.test_name = "NoMaxImageSizeDisplay",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x6c28u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash =
         kNormalDisplayHashedDescriptorBlockSerialNumber,
     .max_image_size = kNoMaxImageSize,
     .display_name = "HP ZR30w",
     .active_pixel_size = gfx::Size(2560, 1600),
     .week_of_manufacture = 2,
     .year_of_manufacture = 2012,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kNormalDisplayPrimaries,
     .product_code = 586181672,
     .index_based_display_id_zero = 9834734971736576,
     .edid_based_display_id = 403808854,
     .manufacturer_id_string = "HWP",
     .product_id_string = "286C",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kNoMaxImageSizeDisplay,
     .edid_blob_length = kNoMaxImageSizeDisplayLength},
    {.test_name = "BlockZeroSerialNumberOnlyDisplay",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x6c28u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(64, 40),
     .display_name = "HP ZR30w",
     .active_pixel_size = gfx::Size(2560, 1600),
     .week_of_manufacture = 2,
     .year_of_manufacture = 2012,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kNormalDisplayPrimaries,
     .product_code = 586181672,
     .index_based_display_id_zero = 9834734971736576,
     .edid_based_display_id = 3094128629,
     .manufacturer_id_string = "HWP",
     .product_id_string = "286C",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kBlockZeroSerialNumberOnlyDisplay,
     .edid_blob_length = kBlockZeroSerialNumberOnlyDisplayLength},
    {.test_name = "NoSerialNumberDisplay",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x6c28u,
     .block_zero_serial_number_hash = kNoSerialNumber,
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(64, 40),
     .display_name = "HP ZR30w",
     .active_pixel_size = gfx::Size(2560, 1600),
     .week_of_manufacture = 2,
     .year_of_manufacture = 2012,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kNormalDisplayPrimaries,
     .product_code = 586181672,
     .index_based_display_id_zero = 9834734971736576,
     .edid_based_display_id = 2769865770,
     .manufacturer_id_string = "HWP",
     .product_id_string = "286C",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kNoSerialNumberDisplay,
     .edid_blob_length = kNoSerialNumberDisplayLength},
    {.test_name = "NoWeekOfManufactureDisplay",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x6c28u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash =
         kNormalDisplayHashedDescriptorBlockSerialNumber,
     .max_image_size = gfx::Size(64, 40),
     .display_name = "HP ZR30w",
     .active_pixel_size = gfx::Size(2560, 1600),
     .week_of_manufacture = kNoWeekOfManufactureTag,
     .year_of_manufacture = 2012,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kNormalDisplayPrimaries,
     .product_code = 586181672,
     .index_based_display_id_zero = 9834734971736576,
     .edid_based_display_id = 4082014303,
     .manufacturer_id_string = "HWP",
     .product_id_string = "286C",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kNoWeekOfManufactureDisplay,
     .edid_blob_length = kNoWeekOfManufactureDisplayLength},
    {.test_name = "ModelYearDisplay",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x6c28u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash =
         kNormalDisplayHashedDescriptorBlockSerialNumber,
     .max_image_size = gfx::Size(64, 40),
     .display_name = "HP ZR30w",
     .active_pixel_size = gfx::Size(2560, 1600),
     .week_of_manufacture = kModelYearTag,
     .year_of_manufacture = 2012,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kNormalDisplayPrimaries,
     .product_code = 586181672,
     .index_based_display_id_zero = 9834734971736576,
     .edid_based_display_id = 1070357245,
     .manufacturer_id_string = "HWP",
     .product_id_string = "286C",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kModelYearDisplay,
     .edid_blob_length = kModelYearDisplayLength},
    {.test_name = "InternalDisplay",
     .manufacturer_id = 0x4ca3u,
     .product_id = 0x4231u,
     .block_zero_serial_number_hash = kNoSerialNumber,
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(26, 16),
     .display_name = "",
     .active_pixel_size = gfx::Size(1280, 800),
     .week_of_manufacture = kNoWeekOfManufactureTag,
     .year_of_manufacture = 2011,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = -1,
     .primaries = kInternalDisplayPrimaries,
     .product_code = 1285767729,
     .index_based_display_id_zero = 21571318625337344,
     .edid_based_display_id = 1646280528,
     .manufacturer_id_string = "SEC",
     .product_id_string = "3142",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kInternalDisplay,
     .edid_blob_length = kInternalDisplayLength},
    {.test_name = "OverscanDisplay",
     .manufacturer_id = 0x4c2du,
     .product_id = 0xfe08u,
     .block_zero_serial_number_hash = kNoSerialNumber,
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(16, 9),
     .display_name = "SAMSUNG",
     .active_pixel_size = gfx::Size(1920, 1080),
     .week_of_manufacture = 41,
     .year_of_manufacture = 2011,
     .overscan_flag = true,
     .gamma = 2.2,
     .bits_per_channel = -1,
     .primaries = kOverscanDisplayPrimaries,
     .product_code = 1278082568,
     .index_based_display_id_zero = 21442559853606400,
     .edid_based_display_id = 3766836601,
     .manufacturer_id_string = "SAM",
     .product_id_string = "08FE",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = 24,
     .tile_scale_to_fit = false,
     .edid_blob = kOverscanDisplay,
     .edid_blob_length = kOverscanDisplayLength},
    {.test_name = "MisdetectedDisplay",
     .manufacturer_id = 0x10ACu,
     .product_id = 0x6440u,
     .block_zero_serial_number_hash =
         base::MD5String("842018892"),  // == LSB of 0x4c, 0x30, 0x30, 0x32
     .descriptor_block_serial_number_hash = base::MD5String("PH5NY13N200L"),
     .max_image_size = gfx::Size(64, 40),
     .display_name = "DELL U3011",
     .active_pixel_size = gfx::Size(1920, 1200),
     .week_of_manufacture = 12,
     .year_of_manufacture = 2011,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = -1,
     .primaries = kMisdetectedDisplayPrimaries,
     .product_code = 279733312,
     .index_based_display_id_zero = 4692848143772416,
     .edid_based_display_id = 1487444765,
     .manufacturer_id_string = "DEL",
     .product_id_string = "4064",
     .supported_color_primary_matrix_ids =
         {{gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::MatrixID::BT709},
          {gfx::ColorSpace::PrimaryID::SMPTE170M,
           gfx::ColorSpace::MatrixID::SMPTE170M}},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = 49,
     .tile_scale_to_fit = false,
     .edid_blob = kMisdetectedDisplay,
     .edid_blob_length = kMisdetectedDisplayLength},
    {.test_name = "LP2565A",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x7626u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash = base::MD5String("CNK80204HM"),
     .max_image_size = gfx::Size(52, 33),
     .display_name = "HP LP2465",
     .active_pixel_size = gfx::Size(1920, 1200),
     .week_of_manufacture = 2,
     .year_of_manufacture = 2008,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = -1,
     .primaries = kLP2565APrimaries,
     .product_code = 586184230,
     .index_based_display_id_zero = 9834630174887424,
     .edid_based_display_id = 1695949480,
     .manufacturer_id_string = "HWP",
     .product_id_string = "2676",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = 48,
     .tile_scale_to_fit = false,
     .edid_blob = kLP2565A,
     .edid_blob_length = kLP2565ALength},
    {.test_name = "LP2565B",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x7526u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash = base::MD5String("CNK80204HM"),
     .max_image_size = gfx::Size(52, 33),
     .display_name = "HP LP2465",
     .active_pixel_size = gfx::Size(1920, 1200),
     .week_of_manufacture = 2,
     .year_of_manufacture = 2008,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = -1,
     .primaries = kLP2565BPrimaries,
     .product_code = 586183974,
     .index_based_display_id_zero = 9834630174887424,
     .edid_based_display_id = 3357789438,
     .manufacturer_id_string = "HWP",
     .product_id_string = "2675",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = 48,
     .tile_scale_to_fit = false,
     .edid_blob = kLP2565B,
     .edid_blob_length = kLP2565BLength},
    {.test_name = "HPz32x",
     .manufacturer_id = 0x22f0u,
     .product_id = 0x7532u,
     .block_zero_serial_number_hash = kGenericBlockZeroHashedSerialNumber,
     .descriptor_block_serial_number_hash = base::MD5String("CNC7270MW0"),
     .max_image_size = gfx::Size(70, 39),
     .display_name = "HP Z32x",
     .active_pixel_size = gfx::Size(3840, 2160),
     .week_of_manufacture = 27,
     .year_of_manufacture = 2017,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kHPz32xPrimaries,
     .product_code = 586183986,
     .index_based_display_id_zero = 9834799315992832,
     .edid_based_display_id = 129207725,
     .manufacturer_id_string = "HWP",
     .product_id_string = "3275",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = 24,
     .tile_scale_to_fit = false,
     .edid_blob = kHPz32x,
     .edid_blob_length = kHPz32xLength},
    {.test_name = "Samus",
     .manufacturer_id = 0x30E4u,
     .product_id = 0x2E04u,
     .block_zero_serial_number_hash = kNoSerialNumber,
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(27, 18),
     .display_name = "",
     .active_pixel_size = gfx::Size(2560, 1700),
     .week_of_manufacture = kNoWeekOfManufactureTag,
     .year_of_manufacture = 2014,
     .overscan_flag = false,
     .gamma = 2.5,
     .bits_per_channel = 8,
     .primaries = kSamusPrimaries,
     .product_code = 820260356,
     .index_based_display_id_zero = 13761487533244416,
     .edid_based_display_id = 2825178591,
     .manufacturer_id_string = "LGD",
     .product_id_string = "042E",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kSamus,
     .edid_blob_length = kSamusLength},
    {.test_name = "Eve",
     .manufacturer_id = 0x4D10u,
     .product_id = 0x8A14u,
     .block_zero_serial_number_hash = kNoSerialNumber,
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(26, 17),
     .display_name = "LQ123P1JX32",
     .active_pixel_size = gfx::Size(2400, 1600),
     .week_of_manufacture = 22,
     .year_of_manufacture = 2017,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 8,
     .primaries = kEvePrimaries,
     .product_code = 1292929556,
     .index_based_display_id_zero = 21692109949126656,
     .edid_based_display_id = 2755351929,
     .manufacturer_id_string = "SHP",
     .product_id_string = "148A",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = kEve,
     .edid_blob_length = kEveLength},
    {.test_name = "HDRMetadata",
     .manufacturer_id = 19501u,
     .product_id = 62989u,
     .block_zero_serial_number_hash =
         base::MD5String("16780800"),  // == LSB of 0x00, 0x0e, 0x00 0x01
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(95, 54),
     .display_name = "SAMSUNG",
     .active_pixel_size = gfx::Size(3840, 2160),
     .week_of_manufacture = 1,
     .year_of_manufacture = 2017,
     .overscan_flag = true,
     .gamma = 2.2,
     .bits_per_channel = -1,
     .primaries = kHDRPrimaries,
     .product_code = 1278080525,
     .index_based_display_id_zero = 21442559853606400,
     .edid_based_display_id = 755395064,
     .manufacturer_id_string = "SAM",
     .product_id_string = "0DF6",
     .supported_color_primary_matrix_ids =
         {{gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::MatrixID::BT709},
          {gfx::ColorSpace::PrimaryID::SMPTE170M,
           gfx::ColorSpace::MatrixID::SMPTE170M},
          {gfx::ColorSpace::PrimaryID::BT2020, gfx::ColorSpace::MatrixID::RGB}},
     .supported_color_transfer_ids = {gfx::ColorSpace::TransferID::BT709,
                                      gfx::ColorSpace::TransferID::PQ,
                                      gfx::ColorSpace::TransferID::HLG},
     .hdr_static_metadata = std::make_optional<gfx::HDRStaticMetadata>(
         603.666,
         530.095,
         0.00454,
         gfx::HDRStaticMetadata::EotfMask({
             gfx::HDRStaticMetadata::Eotf::kGammaSdrRange,
             gfx::HDRStaticMetadata::Eotf::kPq,
         })),
     .vsync_rate_min = 24,
     .tile_scale_to_fit = false,
     .edid_blob = kHDRMetadata,
     .edid_blob_length = kHDRMetadataLength},
    {.test_name = "TiledDisplay",
     .manufacturer_id = 0x10ac,
     .product_id = 0x4741,
     .block_zero_serial_number_hash = "d79ced90548d0c97fee4406b172c6fb9",
     .descriptor_block_serial_number_hash = "684333ab7cf4f22ae964878f967d7a13",
     .max_image_size = gfx::Size(70, 39),
     .display_name = "DELL UP3218K",
     .active_pixel_size = gfx::Size(3840, 2160),
     .week_of_manufacture = 11,
     .year_of_manufacture = 2023,
     .overscan_flag = false,
     .gamma = 2.2,
     .bits_per_channel = 10,
     .primaries = kDellTiledPrimaries,
     .product_code = 279725889,
     .index_based_display_id_zero = 4693236086999552,
     .edid_based_display_id = 4170208605,
     .manufacturer_id_string = "DEL",
     .product_id_string = "4147",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = 24,
     .tile_scale_to_fit = true,
     .edid_blob = kTiledDisplay,
     .edid_blob_length = kTiledDisplayLength},
    // Empty Edid, which is tantamount to error.
    {.test_name = "EmptyEdid",
     .manufacturer_id = 0,
     .product_id = 0,
     .block_zero_serial_number_hash = kNoSerialNumber,
     .descriptor_block_serial_number_hash = kNoSerialNumber,
     .max_image_size = gfx::Size(0, 0),
     .display_name = "",
     .active_pixel_size = gfx::Size(0, 0),
     .week_of_manufacture = kNoWeekOfManufactureTag,
     .year_of_manufacture = display::kInvalidYearOfManufacture,
     .overscan_flag = false,
     .gamma = 0.0,
     .bits_per_channel = -1,
     .primaries = SkColorSpacePrimaries(),
     .product_code = 0,
     .index_based_display_id_zero = 0,
     // Not zero because we're still hashing some string of zero/empty values.
     .edid_based_display_id = 710538554,
     .manufacturer_id_string = "@@@",
     .product_id_string = "0000",
     .supported_color_primary_matrix_ids = {},
     .supported_color_transfer_ids = {},
     .hdr_static_metadata = std::nullopt,
     .vsync_rate_min = std::nullopt,
     .tile_scale_to_fit = false,
     .edid_blob = nullptr,
     .edid_blob_length = 0u},
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
  std::vector<uint8_t> expected_edid(
      GetParam().edid_blob, GetParam().edid_blob + GetParam().edid_blob_length);
  EXPECT_EQ(parser_.edid_blob(), expected_edid);

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

  EXPECT_EQ(GetParam().supported_color_primary_matrix_ids,
            parser_.supported_color_primary_matrix_ids());
  EXPECT_EQ(GetParam().supported_color_transfer_ids,
            parser_.supported_color_transfer_ids());

  const std::optional<gfx::HDRStaticMetadata> hdr_static_metadata =
      parser_.hdr_static_metadata();
  EXPECT_EQ(GetParam().hdr_static_metadata.has_value(),
            hdr_static_metadata.has_value());
  if (GetParam().hdr_static_metadata.has_value() &&
      hdr_static_metadata.has_value()) {
    constexpr double epsilon = 0.001;
    EXPECT_NEAR(GetParam().hdr_static_metadata->max, hdr_static_metadata->max,
                epsilon);
    EXPECT_NEAR(GetParam().hdr_static_metadata->max_avg,
                hdr_static_metadata->max_avg, epsilon);
    EXPECT_NEAR(GetParam().hdr_static_metadata->min, hdr_static_metadata->min,
                epsilon);
  }

  const std::optional<uint16_t> vsync_rate_min = parser_.vsync_rate_min();
  EXPECT_EQ(GetParam().vsync_rate_min.has_value(), vsync_rate_min.has_value());
  if (GetParam().vsync_rate_min.has_value() && vsync_rate_min.has_value()) {
    EXPECT_EQ(vsync_rate_min.value(), GetParam().vsync_rate_min.value());
  }

  EXPECT_EQ(parser_.TileCanScaleToFit(), GetParam().tile_scale_to_fit);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EDIDParserTest,
    ValuesIn(kTestCases),
    [](const testing::TestParamInfo<EDIDParserTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace display

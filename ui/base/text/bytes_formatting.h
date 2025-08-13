// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEXT_BYTES_FORMATTING_H_
#define UI_BASE_TEXT_BYTES_FORMATTING_H_

#include <string>

#include "base/byte_count.h"
#include "base/component_export.h"

namespace ui {

// Simple API ------------------------------------------------------------------

// Simple call to return a byte quantity as a string in human-readable format.
//
// For example:
//   FormatBytes(base::ByteCount(512)) => "512 B"
//   FormatBytes(base::ByteCount(101479)) => "99.1 kB"
COMPONENT_EXPORT(UI_BASE) std::u16string FormatBytes(base::ByteCount bytes);

// Simple call to return a speed as a string in human-readable format.
//
// For example:
//   FormatSpeed(base::ByteCount(512)) => "512 B/s"
//   FormatSpeed(base::ByteCount(101479)) => "99.1 kB/s"
COMPONENT_EXPORT(UI_BASE) std::u16string FormatSpeed(base::ByteCount bytes);

// Less-Simple API -------------------------------------------------------------

enum class DataUnits {
  kByte = 0,
  kKibibyte,
  kMebibyte,
  kGibibyte,
  kTebibyte,
  kPebibyte
};

// Return the unit type that is appropriate for displaying the amount of bytes
// passed in. Most of the time, an explicit call to this isn't necessary; just
// use FormatBytes()/FormatSpeed() above.
COMPONENT_EXPORT(UI_BASE) DataUnits GetByteDisplayUnits(base::ByteCount bytes);

// Return a byte quantity as a string in human-readable format with an optional
// unit suffix. Specify in the `units` argument the units to be used.
//
// For example:
//   FormatBytes(base::ByteCount(512), DataUnits::kKibibyte, true) => "0.5 kB"
//   FormatBytes(base::KiB(10), DataUnits::kMebibyte, false) => "0.1"
COMPONENT_EXPORT(UI_BASE)
std::u16string FormatBytesWithUnits(base::ByteCount bytes,
                                    DataUnits units,
                                    bool show_units);

// As above, but with "/s" units for speed values.
//
// For example:
//   FormatSpeed(base::ByteCount(512), DataUnits::kKibibyte, true) => "0.5 kB/s"
//   FormatSpeed(base::KiB(10), DataUnits::kMebibyte, false) => "0.1"
std::u16string FormatSpeedWithUnits(base::ByteCount bytes,
                                    DataUnits units,
                                    bool show_units);

}  // namespace ui

#endif  // UI_BASE_TEXT_BYTES_FORMATTING_H_

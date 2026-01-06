// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/text/bytes_formatting.h"

#include <array>

#include "base/byte_size.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/i18n/number_formatting.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

// Byte suffix string constants. These both must match the DataUnits enum.
constexpr int kByteStrings[] = {IDS_APP_BYTES,     IDS_APP_KIBIBYTES,
                                IDS_APP_MEBIBYTES, IDS_APP_GIBIBYTES,
                                IDS_APP_TEBIBYTES, IDS_APP_PEBIBYTES};

constexpr int kSpeedStrings[] = {
    IDS_APP_BYTES_PER_SECOND,     IDS_APP_KIBIBYTES_PER_SECOND,
    IDS_APP_MEBIBYTES_PER_SECOND, IDS_APP_GIBIBYTES_PER_SECOND,
    IDS_APP_TEBIBYTES_PER_SECOND, IDS_APP_PEBIBYTES_PER_SECOND};

std::u16string FormatBytesInternal(base::ByteSize bytes,
                                   DataUnits units,
                                   bool show_units,
                                   const base::span<const int> suffix) {
  DCHECK(units >= DataUnits::kByte && units <= DataUnits::kPebibyte);

  // Put the quantity in the right units.
  double unit_amount = bytes.InBytesF();
  for (int i = 0; i < static_cast<int>(units); ++i) {
    unit_amount /= 1024.0;
  }

  int fractional_digits = 0;
  if (!bytes.is_zero() && units != DataUnits::kByte && unit_amount < 100) {
    fractional_digits = 1;
  }

  std::u16string result = base::FormatDouble(unit_amount, fractional_digits);

  if (show_units) {
    result = l10n_util::GetStringFUTF16(
        suffix[static_cast<unsigned int>(units)], result);
  }

  return result;
}

}  // namespace

DataUnits GetByteDisplayUnits(base::ByteSize bytes) {
  // The byte thresholds at which we display amounts. A byte count is displayed
  // in unit U when kUnitThresholds[U] <= bytes < kUnitThresholds[U+1].
  // This must match the DataUnits enum.
  static constexpr auto kUnitThresholds = std::to_array<base::ByteSize>({
      base::ByteSize(0),  // DataUnits::kByte,
      base::KiBU(3),      // DataUnits::kKibibyte,
      base::MiBU(2),      // DataUnits::kMebibyte,
      base::GiBU(1),      // DataUnits::kGibibyte,
      base::TiBU(1),      // DataUnits::kTebibyte,
      base::PiBU(1)       // DataUnits::kPebibyte,
  });
  static constexpr auto kUnitThresholdsSpan = base::span(kUnitThresholds);

  size_t unit_index = kUnitThresholdsSpan.size();
  while (--unit_index > 0) {
    if (bytes >= kUnitThresholdsSpan[unit_index]) {
      break;
    }
  }

  DataUnits units = static_cast<DataUnits>(unit_index);
  DCHECK(units >= DataUnits::kByte && units <= DataUnits::kPebibyte);
  return units;
}

std::u16string FormatBytesWithUnits(base::ByteSize bytes,
                                    DataUnits units,
                                    bool show_units) {
  return FormatBytesInternal(bytes, units, show_units, kByteStrings);
}

std::u16string FormatSpeedWithUnits(base::ByteSize bytes,
                                    DataUnits units,
                                    bool show_units) {
  return FormatBytesInternal(bytes, units, show_units, kSpeedStrings);
}

std::u16string FormatBytes(base::ByteSize bytes) {
  return FormatBytesWithUnits(bytes, GetByteDisplayUnits(bytes),
                              /*show_units=*/true);
}

std::u16string FormatSpeed(base::ByteSize bytes) {
  return FormatSpeedWithUnits(bytes, GetByteDisplayUnits(bytes),
                              /*show_units=*/true);
}

}  // namespace ui

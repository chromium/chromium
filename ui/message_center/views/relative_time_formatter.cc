// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/message_center/views/relative_time_formatter.h"

#include "base/numerics/safe_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"


namespace message_center {

namespace {

// Holds the UI string ids for a given time |range|.
struct RelativeTimeFormat {
  // The time range for these UI string ids.
  base::TimeDelta range;
  // UI string id for times in the past for this range.
  int past;
  // UI string id for times in the future for this range.
  int future;
};

// Gets the relative time format closest but greater than |delta|.
const RelativeTimeFormat& GetRelativeTimeFormat(base::TimeDelta delta) {
  // All relative time formats must be sorted by their |range|.
  static constexpr RelativeTimeFormat kTimeFormats[] = {
      {base::TimeDelta(), IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST},
      {base::Minutes(1), IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE},
      {base::Hours(1), IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE},
      {base::Days(1), IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST_FUTURE},
      {base::Days(364), IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE},
  };
  constexpr size_t kTimeFormatsCount = std::size(kTimeFormats);
  static_assert(kTimeFormatsCount > 0, "kTimeFormats must not be empty");

  for (size_t i = 0; i < kTimeFormatsCount - 1; ++i) {
    if (delta < kTimeFormats[i + 1].range)
      return kTimeFormats[i];
  }
  return kTimeFormats[kTimeFormatsCount - 1];
}

}  // namespace

void GetRelativeTimeStringAndNextUpdateTime(base::TimeDelta delta,
                                            std::u16string* relative_time,
                                            base::TimeDelta* next_update) {
  bool past = delta.is_negative();
  base::TimeDelta absolute = past ? -delta : delta;
  const RelativeTimeFormat& format = GetRelativeTimeFormat(absolute);

  // Handle "now" case without a count.
  if (format.range.is_zero()) {
    *relative_time = l10n_util::GetStringUTF16(format.past);
    *next_update = delta + base::Minutes(1);
    return;
  }

  int string_id = past ? format.past : format.future;
  int count = base::ClampFloor(absolute / format.range);
  base::TimeDelta delay = past ? format.range * (count + 1)
                               : base::Milliseconds(1) - format.range * count;

  *relative_time = l10n_util::GetPluralStringFUTF16(string_id, count);
  *next_update = delta + delay;
}

}  // namespace message_center

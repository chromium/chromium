// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/relative_time_formatter.h"

#include "base/stl_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using base::TimeDelta;

namespace message_center {

namespace {

// Holds the UI string ids for a given time |range|.
struct RelativeTimeFormat {
  // The time range for these UI string ids.
  TimeDelta range;
  // UI string id for times in the past for this range.
  int past;
  // UI string id for times in the future for this range.
  int future;
};

// Gets the relative time format closest but greater than |delta|.
const RelativeTimeFormat& GetRelativeTimeFormat(TimeDelta delta) {
  // All relative time formats must be sorted by their |range|.
  static constexpr RelativeTimeFormat kTimeFormats[] = {
      {TimeDelta(), IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST},
      {TimeDelta::FromMinutes(1),
       IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE},
      {TimeDelta::FromHours(1),
       IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE},
      {TimeDelta::FromDays(1), IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST_FUTURE},
      {TimeDelta::FromDays(364),
       IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST,
       IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE},
  };
  constexpr size_t kTimeFormatsCount = base::size(kTimeFormats);
  static_assert(kTimeFormatsCount > 0, "kTimeFormats must not be empty");

  for (size_t i = 0; i < kTimeFormatsCount - 1; ++i) {
    if (delta < kTimeFormats[i + 1].range)
      return kTimeFormats[i];
  }
  return kTimeFormats[kTimeFormatsCount - 1];
}

}  // namespace

void GetRelativeTimeStringAndNextUpdateTime(TimeDelta delta,
                                            base::string16* relative_time,
                                            TimeDelta* next_update) {
  bool past = delta < TimeDelta();
  TimeDelta absolute = past ? -delta : delta;
  const RelativeTimeFormat& format = GetRelativeTimeFormat(absolute);

  // Handle "now" case without a count.
  if (format.range.is_zero()) {
    *relative_time = l10n_util::GetStringUTF16(format.past);
    *next_update = delta + TimeDelta::FromMinutes(1);
    return;
  }

  int string_id = past ? format.past : format.future;
  int count = static_cast<int>(absolute / format.range);
  TimeDelta delay = past
                        ? format.range * (count + 1)
                        : TimeDelta::FromMilliseconds(1) - format.range * count;

  *relative_time = l10n_util::GetPluralStringFUTF16(string_id, count);
  *next_update = delta + delay;
}

}  // namespace message_center

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_RELATIVE_TIME_FORMATTER_H_
#define UI_MESSAGE_CENTER_VIEWS_RELATIVE_TIME_FORMATTER_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

// Translates the given relative time in |delta| and writes the resulting UI
// string into |relative_time|. That string will be valid for the duration
// written to |next_update|.
MESSAGE_CENTER_EXPORT void GetRelativeTimeStringAndNextUpdateTime(
    base::TimeDelta delta,
    base::string16* relative_time,
    base::TimeDelta* next_update);

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_RELATIVE_TIME_FORMATTER_H_

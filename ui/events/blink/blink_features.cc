// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_features.h"

namespace features {

BASE_FEATURE(kSendMouseLeaveEvents,
             "SendMouseLeaveEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDontSendKeyEventsToJavascript,
             "DontSendKeyEventsToJavascript",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

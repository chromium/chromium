// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/ui_android_features.h"

#include "base/feature_list.h"

namespace features {
BASE_FEATURE(kConvertTrackpadEventsToMouse,
             "ConvertTrackpadEventsToMouse",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

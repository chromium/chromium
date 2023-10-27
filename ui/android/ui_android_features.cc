// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/ui_android_features.h"

namespace ui {

BASE_FEATURE(kAndroidHDR, "AndroidHDR", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kConvertTrackpadEventsToMouse,
             "ConvertTrackpadEventsToMouse",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeprecatedExternalPickerFunction,
             "DeprecatedExternalPickerFunction",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace ui

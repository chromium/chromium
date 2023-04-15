// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_UI_ANDROID_FEATURES_H_
#define UI_ANDROID_UI_ANDROID_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace features {

// Keep sorted!

// Enables converting trackpad click gestures to mouse events in
// order for them to be interpreted similar to a desktop
// experience (i.e. double-click to select word.)
COMPONENT_EXPORT(UI_ANDROID_FEATURES)
BASE_DECLARE_FEATURE(kConvertTrackpadEventsToMouse);
}  // namespace features

#endif  // UI_ANDROID_UI_ANDROID_FEATURES_H_

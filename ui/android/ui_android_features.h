// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_UI_ANDROID_FEATURES_H_
#define UI_ANDROID_UI_ANDROID_FEATURES_H_

#include "base/feature_list.h"
#include "ui/android/ui_android_export.h"

namespace ui {

// Keep sorted!

// Feature controlling whether or not HDR is enabled on Android.
// TODO(https://crbug.com/1430768): Leave this as a kill switch until Android U
// ships.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kAndroidHDR);

// Enables converting trackpad click gestures to mouse events in
// order for them to be interpreted similar to a desktop
// experience (i.e. double-click to select word.)
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kConvertTrackpadEventsToMouse);

// Use the old-style opening of an External Picker when uploading files.
UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kDeprecatedExternalPickerFunction);

}  // namespace ui

#endif  // UI_ANDROID_UI_ANDROID_FEATURES_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_BROWSER_CONTROLS_OFFSET_TAG_DEFINITIONS_H_
#define UI_ANDROID_BROWSER_CONTROLS_OFFSET_TAG_DEFINITIONS_H_

#include "base/android/scoped_java_ref.h"
#include "cc/input/browser_controls_offset_tags.h"
#include "ui/android/browser_controls_offset_tag_constraints.h"
#include "ui/android/ui_android_export.h"

namespace ui {

// See cc/input/browser_controls_offset_tags.h and
// ui/android/browser_controls_offset_tag_constraints.h for more details.
struct UI_ANDROID_EXPORT BrowserControlsOffsetTagDefinitions {
  cc::BrowserControlsOffsetTags tags;
  BrowserControlsOffsetTagConstraints constraints;
};

UI_ANDROID_EXPORT BrowserControlsOffsetTagDefinitions
FromJavaBrowserControlsOffsetTagDefinitions(
    JNIEnv* env,
    const base::android::JavaRef<jobject>&
        jbrowser_controls_offset_tag_definitions);

}  // namespace ui

#endif  // UI_ANDROID_BROWSER_CONTROLS_OFFSET_TAG_DEFINITIONS_H_

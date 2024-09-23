// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_PROGRESS_BAR_CONFIG_H_
#define UI_ANDROID_PROGRESS_BAR_CONFIG_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/ui_android_export.h"

namespace ui {

// Provides drawing information about the progress bar drawn in java UI.
// See WindowAndroid.ProgressBarConfig.
struct UI_ANDROID_EXPORT ProgressBarConfig {
  SkColor4f background_color;
  int height_physical = 0;
  SkColor4f color;
  int hairline_height_physical = 0;
  SkColor4f hairline_color;

  bool ShouldDisplay() const { return height_physical != 0; }
};

}  // namespace ui

#endif  // UI_ANDROID_PROGRESS_BAR_CONFIG_H_

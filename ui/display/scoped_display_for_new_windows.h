// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_SCOPED_DISPLAY_FOR_NEW_WINDOWS_H_
#define UI_DISPLAY_SCOPED_DISPLAY_FOR_NEW_WINDOWS_H_

#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"

namespace display {

// Constructing a ScopedDisplayForNewWindows allows temporarily switching
// display for new windows during the lifetime of this object.
class DISPLAY_EXPORT ScopedDisplayForNewWindows {
 public:
  explicit ScopedDisplayForNewWindows(int64_t new_display);
  explicit ScopedDisplayForNewWindows(gfx::NativeView view);
  ~ScopedDisplayForNewWindows();
  ScopedDisplayForNewWindows(const ScopedDisplayForNewWindows&) = delete;
  ScopedDisplayForNewWindows& operator=(const ScopedDisplayForNewWindows&) =
      delete;
};

}  // namespace display

#endif  // UI_DISPLAY_SCOPED_DISPLAY_FOR_NEW_WINDOWS_H_

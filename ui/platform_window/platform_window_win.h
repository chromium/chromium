// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_WIN_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_WIN_H_

#include "base/component_export.h"
#include "ui/platform_window/platform_window_base.h"

namespace ui {

// Windows extensions to the PlatformWindowBase.
class COMPONENT_EXPORT(PLATFORM_WINDOW) PlatformWindowWin
    : public PlatformWindowBase {
 public:
  PlatformWindowWin() = default;
  ~PlatformWindowWin() override = default;

  // Enables or disables platform provided animations of the PlatformWindow.
  // If |enabled| is set to false, animations must be disabled.
  virtual void SetVisibilityChangedAnimationsEnabled(bool enabled) = 0;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_WIN_H_

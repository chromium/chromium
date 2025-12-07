// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_H_
#define UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_H_

#include <memory>

#include "base/component_export.h"
#include "ui/gfx/native_ui_types.h"

namespace ui {

// An observer which detects when a gfx::NativeWindow is closed.
class COMPONENT_EXPORT(UI_NATIVE_WINDOW_TRACKER) NativeWindowTracker {
 public:
  virtual ~NativeWindowTracker() = default;

  static std::unique_ptr<NativeWindowTracker> Create(gfx::NativeWindow window);

  // Returns true if the native window passed to Create() has been closed.
  virtual bool WasNativeWindowDestroyed() const = 0;
};

}  // namespace ui

#endif  // UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_H_

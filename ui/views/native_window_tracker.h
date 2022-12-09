// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_NATIVE_WINDOW_TRACKER_H_
#define UI_VIEWS_NATIVE_WINDOW_TRACKER_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace views {

// An observer which detects when a gfx::NativeWindow is closed.
class VIEWS_EXPORT NativeWindowTracker {
 public:
  virtual ~NativeWindowTracker() = default;

  static std::unique_ptr<NativeWindowTracker> Create(gfx::NativeWindow window);

  // Returns true if the native window passed to Create() has been closed.
  virtual bool WasNativeWindowDestroyed() const = 0;
};

}  // namespace views

#endif  // UI_VIEWS_NATIVE_WINDOW_TRACKER_H_

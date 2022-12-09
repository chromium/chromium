// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_NATIVE_WINDOW_TRACKER_COCOA_H_
#define UI_VIEWS_COCOA_NATIVE_WINDOW_TRACKER_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/views_export.h"

@class BridgedNativeWindowTracker;

namespace views {

class VIEWS_EXPORT NativeWindowTrackerCocoa : public NativeWindowTracker {
 public:
  explicit NativeWindowTrackerCocoa(gfx::NativeWindow window);

  NativeWindowTrackerCocoa(const NativeWindowTrackerCocoa&) = delete;
  NativeWindowTrackerCocoa& operator=(const NativeWindowTrackerCocoa&) = delete;

  ~NativeWindowTrackerCocoa() override;

  // NativeWindowTracker:
  bool WasNativeWindowDestroyed() const override;

 private:
  base::scoped_nsobject<BridgedNativeWindowTracker> bridge_;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_NATIVE_WINDOW_TRACKER_COCOA_H_

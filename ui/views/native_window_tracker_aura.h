// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_NATIVE_WINDOW_TRACKER_AURA_H_
#define UI_VIEWS_NATIVE_WINDOW_TRACKER_AURA_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT NativeWindowTrackerAura : public NativeWindowTracker,
                                             public aura::WindowObserver {
 public:
  explicit NativeWindowTrackerAura(gfx::NativeWindow window);

  NativeWindowTrackerAura(const NativeWindowTrackerAura&) = delete;
  NativeWindowTrackerAura& operator=(const NativeWindowTrackerAura&) = delete;

  ~NativeWindowTrackerAura() override;

  // NativeWindowTracker:
  bool WasNativeWindowDestroyed() const override;

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  raw_ptr<aura::Window> window_;
};

}  // namespace views

#endif  // UI_VIEWS_NATIVE_WINDOW_TRACKER_AURA_H_

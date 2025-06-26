// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_AURA_H_
#define UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_AURA_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/native_window_tracker/native_window_tracker.h"

namespace ui {

class COMPONENT_EXPORT(UI_NATIVE_WINDOW_TRACKER) NativeWindowTrackerAura
    : public NativeWindowTracker,
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

}  // namespace ui

#endif  // UI_NATIVE_WINDOW_TRACKER_NATIVE_WINDOW_TRACKER_AURA_H_

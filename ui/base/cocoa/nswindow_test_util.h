// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_NSWINDOW_TEST_UTIL_H_
#define UI_BASE_COCOA_NSWINDOW_TEST_UTIL_H_

#include <memory>

#include "base/component_export.h"
#include "base/run_loop.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE) NSWindowFullscreenNotificationWaiter {
 public:
  explicit NSWindowFullscreenNotificationWaiter(gfx::NativeWindow window);
  ~NSWindowFullscreenNotificationWaiter();

  void WaitForEnterAndExitCount(int target_enter_count, int target_exit_count);
  int enter_count() const { return enter_count_; }
  int exit_count() const { return exit_count_; }

  static void NotifyFullscreenTransitionComplete(gfx::NativeWindow window,
                                                 bool fullscreen);

 private:
  void NotifyFullscreenTransitionComplete(bool fullscreen);

  std::unique_ptr<base::RunLoop> run_loop_;
  const gfx::NativeWindow window_;

  int enter_count_ = 0;
  int exit_count_ = 0;
  int target_enter_count_ = 0;
  int target_exit_count_ = 0;

  static NSWindowFullscreenNotificationWaiter* instance_;
};

class COMPONENT_EXPORT(UI_BASE) NSWindowFakedForTesting {
 public:
  // This is set by tests that are not written robustly to asynchronous
  // transitions to fullscreen (e.g, by using ScopedFakeNSWindowFullscreen on
  // macOS).
  static void SetEnabled(bool);
  static bool IsEnabled();

 private:
  static bool enabled_;
};

// static void SetFullscreenFakedForTesting(bool);
// static bool IsFullscreenFakedForTesting();

}  // namespace ui

#endif  // UI_BASE_COCOA_NSWINDOW_TEST_UTIL_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SCOPED_ANIMATION_DISABLER_H_
#define UI_WM_CORE_SCOPED_ANIMATION_DISABLER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}

namespace wm {

// Helper class to perform window state changes without animations. Used to hide
// /show/minimize windows without having their animation interfere with the ones
// this class is in charge of.
class COMPONENT_EXPORT(UI_WM) ScopedAnimationDisabler {
 public:
  explicit ScopedAnimationDisabler(aura::Window* window);
  ScopedAnimationDisabler(const ScopedAnimationDisabler&) = delete;
  ScopedAnimationDisabler& operator=(const ScopedAnimationDisabler&) = delete;
  ~ScopedAnimationDisabler();

 private:
  const raw_ptr<aura::Window> window_;
  bool was_animation_enabled_ = false;
};

}  // namespace wm

#endif  // UI_WM_CORE_SCOPED_ANIMATION_DISABLER_H_

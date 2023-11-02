// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_ANIMATION_HOST_H_
#define UI_WM_PUBLIC_ANIMATION_HOST_H_

#include "ui/wm/public/wm_public_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Vector2d;
}

namespace wm {

// Interface for top level window host of animation. Communicates additional
// bounds required for animation as well as animation completion for deferring
// window closes on hide.
class WM_PUBLIC_EXPORT AnimationHost {
 public:
  // Ensure the host window is at least this large so that transitions have
  // sufficient space.
  // The |top_left_delta| parameter contains the offset to be subtracted from
  // the window bounds for the top left corner.
  // The |bottom_right_delta| parameter contains the offset to be added to the
  // window bounds for the bottom right.
  virtual void SetHostTransitionOffsets(
      const gfx::Vector2d& top_left_delta,
      const gfx::Vector2d& bottom_right_delta) = 0;

  // Called after the window has faded out on a hide.
  virtual void OnWindowHidingAnimationCompleted() = 0;

 protected:
  virtual ~AnimationHost() {}
};

WM_PUBLIC_EXPORT void SetAnimationHost(aura::Window* window,
                                       AnimationHost* animation_host);
WM_PUBLIC_EXPORT AnimationHost* GetAnimationHost(aura::Window* window);

}  // namespace wm

#endif  // UI_WM_PUBLIC_ANIMATION_HOST_H_

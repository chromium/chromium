// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_TEST_ANIMATION_DELEGATE_H_
#define UI_GFX_ANIMATION_TEST_ANIMATION_DELEGATE_H_

#include "base/run_loop.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {

// Trivial AnimationDelegate implementation. AnimationEnded/Canceled quit the
// message loop.
class TestAnimationDelegate : public AnimationDelegate {
 public:
  TestAnimationDelegate() = default;

  TestAnimationDelegate(const TestAnimationDelegate&) = delete;
  TestAnimationDelegate& operator=(const TestAnimationDelegate&) = delete;

  virtual void AnimationEnded(const Animation* animation) {
    finished_ = true;
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  virtual void AnimationCanceled(const Animation* animation) {
    canceled_ = true;
    AnimationEnded(animation);
  }

  bool finished() const { return finished_; }
  bool canceled() const { return canceled_; }

 private:
  bool canceled_ = false;
  bool finished_ = false;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_TEST_ANIMATION_DELEGATE_H_

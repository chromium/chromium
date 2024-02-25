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
  TestAnimationDelegate(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  TestAnimationDelegate(const TestAnimationDelegate&) = delete;
  TestAnimationDelegate& operator=(const TestAnimationDelegate&) = delete;

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }
  virtual void AnimationEnded(const Animation* animation) {
    finished_ = true;
    QuitRunLoop();
  }

  void QuitRunLoop() { std::move(quit_closure_).Run(); }

  virtual void AnimationCanceled(const Animation* animation) {
    canceled_ = true;
    AnimationEnded(animation);
  }

  bool finished() const { return finished_; }
  bool canceled() const { return canceled_; }

 private:
  bool canceled_ = false;
  bool finished_ = false;
  base::OnceClosure quit_closure_ = base::DoNothing();
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_TEST_ANIMATION_DELEGATE_H_

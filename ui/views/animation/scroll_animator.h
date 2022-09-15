// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_SCROLL_ANIMATOR_H_
#define UI_VIEWS_ANIMATION_SCROLL_ANIMATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/views_export.h"

namespace gfx {
class SlideAnimation;
}

namespace views {

class VIEWS_EXPORT ScrollDelegate {
 public:
  // Returns true if the content was actually scrolled, false otherwise.
  virtual bool OnScroll(float dx, float dy) = 0;

  // Called when the contents scrolled by the fling event ended.
  virtual void OnFlingScrollEnded() {}

 protected:
  ~ScrollDelegate() = default;
};

class VIEWS_EXPORT ScrollAnimator : public gfx::AnimationDelegate {
 public:
  // The ScrollAnimator does not own the delegate. Uses default acceleration.
  explicit ScrollAnimator(ScrollDelegate* delegate);

  ScrollAnimator(const ScrollAnimator&) = delete;
  ScrollAnimator& operator=(const ScrollAnimator&) = delete;

  ~ScrollAnimator() override;

  // Use this if you would prefer different acceleration than the default.
  void set_acceleration(float acceleration) { acceleration_ = acceleration; }

  // Use this if you would prefer different velocity than the default.
  void set_velocity_multiplier(float velocity_multiplier) {
    velocity_multiplier_ = velocity_multiplier;
  }

  void Start(float velocity_x, float velocity_y);
  void Stop();

  bool is_scrolling() const { return !!animation_.get(); }

 private:
  // Implementation of gfx::AnimationDelegate.
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  raw_ptr<ScrollDelegate> delegate_;

  float velocity_x_{0.f};
  float velocity_y_{0.f};
  float velocity_multiplier_{1.f};
  float last_t_{0.f};
  float duration_{0.f};
  float acceleration_;

  std::unique_ptr<gfx::SlideAnimation> animation_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_SCROLL_ANIMATOR_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_SCROLLBAR_ANIMATOR_MAC_H_
#define UI_NATIVE_THEME_SCROLLBAR_ANIMATOR_MAC_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/native_theme/native_theme_export.h"

namespace ui {

// Timer used for animating scrollbar effects.
// TODO(crbug.com/40626921): Change this to be driven by the client
// (Blink or Views) animation system.
class NATIVE_THEME_EXPORT ScrollbarAnimationTimerMac {
 public:
  ScrollbarAnimationTimerMac(
      base::RepeatingCallback<void(double)> callback,
      double duration,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~ScrollbarAnimationTimerMac();

  void Start();
  void Stop();
  void SetDuration(double duration);

 private:
  void TimerFired();

  base::RepeatingTimer timer_;
  double start_time_;  // In seconds.
  double duration_;    // In seconds.
  base::RepeatingCallback<void(double)> callback_;
  std::unique_ptr<gfx::CubicBezierTimingFunction> timing_function_;
};

class NATIVE_THEME_EXPORT OverlayScrollbarAnimatorMac {
 public:
  class Client {
   public:
    virtual ~Client() {}
    // Return true if the mouse is currently in the tracks' frame. This is used
    // during an initial scroll, because MouseDidEnter and MouseDidExit are not
    // while the scrollbar is hidden.
    virtual bool IsMouseInScrollbarFrameRect() const = 0;

    // Set whether all of the scrollbar is hidden.
    virtual void SetHidden(bool) = 0;

    // Called whenever the value of `GetThumbAlpha` or `GetThumbWidth` may
    // have changed.
    virtual void SetThumbNeedsDisplay() = 0;

    // Called whenever the value of `GetTrackAlpha` may have changed.
    virtual void SetTrackNeedsDisplay() = 0;
  };

  OverlayScrollbarAnimatorMac(
      Client* client,
      int thumb_width_expanded,
      int thumb_width_unexpanded,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~OverlayScrollbarAnimatorMac();

  // Called when the mouse moves to be over the scrollbar's area.
  void MouseDidEnter();

  // Called when the mouse leave the scrollbar's area.
  void MouseDidExit();

  // Called in response to a scroll in the direction of this scrollbar.
  void DidScroll();

  // Retrieve the rendering properties of the scrollbar.
  float GetThumbAlpha() const { return thumb_alpha_; }
  float GetTrackAlpha() const { return track_alpha_; }
  int GetThumbWidth() const { return thumb_width_; }

 protected:
  void ExpandThumbAnimationStart();
  void ExpandThumbAnimationTicked(double progress);

  void FadeInTrackAnimationStart();
  void FadeInTrackAnimationTicked(double progress);

  void FadeOutTimerUpdate();
  void FadeOutAnimationStart();
  void FadeOutAnimationTicked(double progress);
  void FadeOutAnimationCancel();

  const raw_ptr<Client> client_;  // Weak, owns `this`.
  const int thumb_width_expanded_;
  const int thumb_width_unexpanded_;

  int thumb_width_ = 0;
  float thumb_alpha_ = 0;
  float track_alpha_ = 0;
  bool mouse_in_track_ = false;

  static const float kAnimationDurationSeconds;
  static const base::TimeDelta kFadeOutDelay;

  // Animator for expanding `thumb_width_` when the mouse enters the
  // scrollbar area.
  std::unique_ptr<ScrollbarAnimationTimerMac> expand_thumb_animation_;

  // Animator for fading in `track_alpha_` when the mouse enters the scrollbar
  // area. Note that `thumb_alpha_` never fades in (it instantaneously appears).
  std::unique_ptr<ScrollbarAnimationTimerMac> fade_in_track_animation_;

  // Animator for fading out `track_alpha_` and `thumb_alpha_` together after
  // inactivity.
  std::unique_ptr<ScrollbarAnimationTimerMac> fade_out_animation_;

  // Timer to trigger the `fade_out_animation_`.
  std::unique_ptr<base::RetainingOneShotTimer> start_scrollbar_fade_out_timer_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<OverlayScrollbarAnimatorMac> weak_factory_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_SCROLLBAR_ANIMATOR_MAC_H_

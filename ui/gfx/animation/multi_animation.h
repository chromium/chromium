// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_MULTI_ANIMATION_H_
#define UI_GFX_ANIMATION_MULTI_ANIMATION_H_

#include <stddef.h>

#include <vector>

#include "base/time/time.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"

namespace gfx {

// MultiAnimation is an animation that consists of a number of sub animations.
// To create a MultiAnimation pass in the parts, invoke Start() and the delegate
// is notified as the animation progresses. By default MultiAnimation runs until
// Stop is invoked, see |set_continuous()| for details.
class ANIMATION_EXPORT MultiAnimation : public Animation {
 public:
  // Defines part of the animation. Each part consists of the following:
  //
  // part_length: the length of time the part runs.
  // part_start: the amount of time to offset this part by when calculating the
  // initial percentage.
  // total_length: the total length used to calculate the percentange completed.
  // start_value: the animation value at the beginning of this part of the
  // animation. Defaults to 0.
  // end_value: the animation value at the end of this part of the animation.
  // Defaults to 1.
  //
  // In most cases |part_start| is empty and |total_length| = |part_length|. But
  // you can adjust the start/total for different effects. For example, to run a
  // part for 200ms with a % between .25 and .75 use the following three values:
  // part_length = 200, part_start = 100, total_length = 400.
  //
  // |start_value| and |end_value| can be used to chain multiple animations into
  // a single function. A common use case is a MultiAnimation that consists of
  // these parts: 0->1 (fade-in), 1->1 (hold) and 1->0 (fade out).
  struct Part {
    Part(base::TimeDelta length,
         Tween::Type type,
         double start_value = 0.0,
         double end_value = 1.0)
        : length(length),
          type(type),
          start_value(start_value),
          end_value(end_value) {}

    base::TimeDelta length;
    Tween::Type type;
    double start_value;
    double end_value;
  };
  using Parts = std::vector<Part>;

  static constexpr auto kDefaultTimerInterval = base::Milliseconds(20);

  explicit MultiAnimation(
      const Parts& parts,
      base::TimeDelta timer_interval = kDefaultTimerInterval);

  MultiAnimation(const MultiAnimation&) = delete;
  MultiAnimation& operator=(const MultiAnimation&) = delete;

  ~MultiAnimation() override;

  // Sets whether the animation continues after it reaches the end. If true, the
  // animation runs until explicitly stopped. The default is true.
  void set_continuous(bool continuous) { continuous_ = continuous; }

  // Returns the current value. The current value for a MultiAnimation is
  // determined from the tween type of the current part.
  double GetCurrentValue() const override;

  // Returns the index of the current part.
  size_t current_part_index() const { return current_part_index_; }

 protected:
  // Animation overrides.
  void Step(base::TimeTicks time_now) override;
  void SetStartTime(base::TimeTicks start_time) override;

 private:
  // Returns the part containing the specified time. |time| is reset to be
  // relative to the part containing the time and |part_index| the index of the
  // part.
  const Part& GetPart(base::TimeDelta* time, size_t* part_index);

  // The parts that make up the animation.
  const Parts parts_;

  // Total time of all the parts.
  const base::TimeDelta cycle_time_;

  // Animation state for the current part.
  double current_part_state_ = 0.0;

  // Index of the current part.
  size_t current_part_index_ = 0;

  // See description above setter.
  bool continuous_ = true;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_MULTI_ANIMATION_H_

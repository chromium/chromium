// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_MODEL_H_
#define UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_MODEL_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_animation_export.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace gfx {

class GFX_KEYFRAME_ANIMATION_EXPORT KeyframeModel {
 public:
  // KeyframeModels begin in the 'WAITING_FOR_TARGET_AVAILABILITY' state. A
  // KeyframeModel waiting for target availability will run as soon as its
  // target property is free (and all the KeyframeModels animating with it are
  // also able to run). When this time arrives, the controller will move the
  // keyframe model into the STARTING state, and then into the RUNNING state.
  // RUNNING KeyframeModels may toggle between RUNNING and PAUSED, and may be
  // stopped by moving into either the ABORTED or FINISHED states. A FINISHED
  // keyframe model was allowed to run to completion, but an ABORTED keyframe
  // model was not. An animation in the state ABORTED_BUT_NEEDS_COMPLETION is a
  // keyframe model that was aborted for some reason, but needs to be finished.
  // Currently this is for impl-only scroll offset KeyframeModels that need to
  // be completed on the main thread.
  enum RunState {
    WAITING_FOR_TARGET_AVAILABILITY = 0,
    WAITING_FOR_DELETION,
    STARTING,
    RUNNING,
    PAUSED,
    FINISHED,
    ABORTED,
    ABORTED_BUT_NEEDS_COMPLETION,
    // This sentinel must be last.
    LAST_RUN_STATE = ABORTED_BUT_NEEDS_COMPLETION
  };
  static std::string ToString(RunState);

  enum class Direction { NORMAL, REVERSE, ALTERNATE_NORMAL, ALTERNATE_REVERSE };

  enum class FillMode { NONE, FORWARDS, BACKWARDS, BOTH, AUTO };

  enum class Phase { BEFORE, ACTIVE, AFTER };

  static std::unique_ptr<KeyframeModel> Create(
      std::unique_ptr<AnimationCurve> curve,
      int keyframe_model_id,
      int target_property_id);

  KeyframeModel(const KeyframeModel&) = delete;
  virtual ~KeyframeModel();

  KeyframeModel& operator=(const KeyframeModel&) = delete;

  int id() const { return id_; }

  virtual int TargetProperty() const;

  RunState run_state() const { return run_state_; }
  virtual void SetRunState(RunState run_state, base::TimeTicks monotonic_time);

  // Pause the keyframe effect at local time |pause_offset|.
  void Pause(base::TimeDelta pause_offset);

  base::TimeTicks start_time() const {
    return start_time_.value_or(base::TimeTicks());
  }

  void set_start_time(base::TimeTicks monotonic_time) {
    start_time_ = monotonic_time;
  }
  bool has_set_start_time() const { return start_time_.has_value(); }

  base::TimeDelta time_offset() const { return time_offset_; }
  void set_time_offset(base::TimeDelta monotonic_time) {
    time_offset_ = monotonic_time;
  }

  Direction direction() const { return direction_; }
  void set_direction(Direction direction) { direction_ = direction; }

  FillMode fill_mode() const { return fill_mode_; }
  void set_fill_mode(FillMode fill_mode) { fill_mode_ = fill_mode; }

  double playback_rate() const { return playback_rate_; }
  void set_playback_rate(double playback_rate) {
    playback_rate_ = playback_rate;
  }

  base::TimeTicks pause_time() const { return pause_time_; }
  void set_pause_time(base::TimeTicks pause_time) { pause_time_ = pause_time; }

  base::TimeDelta total_paused_duration() const {
    return total_paused_duration_;
  }
  void set_total_paused_duration(base::TimeDelta total_paused_duration) {
    total_paused_duration_ = total_paused_duration;
  }

  // This is the number of times that the keyframe model will play. If this
  // value is zero or negative, the keyframe model will not play. If it is
  // std::numeric_limits<double>::infinity(), then the keyframe model will loop
  // indefinitely.
  double iterations() const { return iterations_; }
  void set_iterations(double n) { iterations_ = n; }

  double iteration_start() const { return iteration_start_; }
  void set_iteration_start(double iteration_start) {
    iteration_start_ = iteration_start;
  }

  AnimationCurve* curve() { return curve_.get(); }
  const AnimationCurve* curve() const { return curve_.get(); }

  bool IsFinishedAt(base::TimeTicks monotonic_time) const;
  bool is_finished() const {
    return run_state_ == FINISHED || run_state_ == ABORTED ||
           run_state_ == WAITING_FOR_DELETION;
  }

  bool HasActiveTime(base::TimeTicks monotonic_time) const;

  template <typename T>
  void Retarget(base::TimeTicks now,
                int property_id,
                const T& new_target_value) {
    if (!curve_)
      return;
    base::TimeDelta now_delta = TrimTimeToCurrentIteration(now);

    DCHECK_EQ(CalculatePhase(now_delta), KeyframeModel::Phase::ACTIVE);
    auto* keyframed_curve = AnimationTraits<T>::ToKeyframedCurve(curve_.get());
    DCHECK(keyframed_curve);
    if (auto new_curve = keyframed_curve->Retarget(now_delta, new_target_value))
      curve_ = std::move(new_curve);
  }

  // Some clients may run threaded animations and may need to defer starting
  // until the animation on the other thread has been started.
  virtual bool StartShouldBeDeferred() const;

  // Takes the given absolute time, and using the start time and the number
  // of iterations, returns the relative time in the current iteration.
  // The limit direction is calculated and stored if the limit_direction
  // parameter is not null. This limit is needed when using a step timing
  // function.
  base::TimeDelta TrimTimeToCurrentIteration(
      base::TimeTicks monotonic_time,
      TimingFunction::LimitDirection* limit_direction = nullptr) const;

  KeyframeModel::Phase CalculatePhaseForTesting(
      base::TimeDelta local_time) const;

 protected:
  KeyframeModel(std::unique_ptr<AnimationCurve> curve,
                int keyframe_model_id,
                int target_property_id);

  void ForceRunState(RunState run_state) { run_state_ = run_state; }
  std::optional<base::TimeDelta> CalculateActiveTime(
      base::TimeTicks monotonic_time) const;
  std::optional<base::TimeDelta> CalculateActiveTime(
      base::TimeDelta local_time,
      KeyframeModel::Phase phase) const;

 private:
  KeyframeModel::Phase CalculatePhase(base::TimeDelta local_time) const;

  // Return local time for this keyframe model given the absolute monotonic
  // time.
  //
  // Local time represents the time value that is used to tick this keyframe
  // model and is relative to its start time. It is closely related to the local
  // time concept in web animations [1]. It is:
  //  - for playing animation : wall time - start time - paused duration
  //  - for paused animation  : paused time
  //  - otherwise             : zero
  //
  // Here is small diagram that shows how active, local, and monotonic times
  // relate to each other and to the run state.
  //
  //      run state   Starting  (R)unning  Paused (R) Paused (R)  Finished
  //                    ^                                          ^
  //                    |                                          |
  // monotonic time  ------------------------------------------------->
  //                    |                                          |
  //     local time     +-----------------+      +---+      +--------->
  //                    |                                          |
  //    active time     +          +------+      +---+      +------+
  //                      (-offset)
  //
  // [1] https://drafts.csswg.org/web-animations/#local-time-section
  base::TimeDelta ConvertMonotonicTimeToLocalTime(
      base::TimeTicks monotonic_time) const;

  std::unique_ptr<AnimationCurve> curve_;

  // IDs must be unique.
  int id_;

  int target_property_ = 0;
  RunState run_state_;
  double iterations_;
  double iteration_start_;
  Direction direction_;
  double playback_rate_;
  FillMode fill_mode_;

  std::optional<base::TimeTicks> start_time_;

  // The time offset effectively pushes the start of the keyframe model back in
  // time. This is used for resuming paused KeyframeModels -- an animation is
  // added with a non-zero time offset, causing the keyframe model to skip ahead
  // to the desired point in time.
  base::TimeDelta time_offset_;

  // These are used when converting monotonic to local time to account for time
  // spent while paused. This is not included in AnimationState since it
  // there is absolutely no need for clients of this controller to know
  // about these values.
  base::TimeTicks pause_time_;
  base::TimeDelta total_paused_duration_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAME_MODEL_H_

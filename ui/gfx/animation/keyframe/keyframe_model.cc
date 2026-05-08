// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframe_model.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/time/time.h"

namespace gfx {
namespace {

// This should match the RunState enum.
static constexpr auto s_runStateNames = std::to_array<const char*>(
    {"WAITING_FOR_TARGET_AVAILABILITY", "WAITING_FOR_DELETION", "STARTING",
     "RUNNING", "PAUSED", "PAUSED_EXCLUSIVE", "FINISHED", "ABORTED",
     "ABORTED_BUT_NEEDS_COMPLETION"});

static_assert(static_cast<int>(KeyframeModel::LAST_RUN_STATE) + 1 ==
                  std::size(s_runStateNames),
              "RunStateEnumSize should equal the number of elements in "
              "s_runStateNames");

}  // namespace

std::string KeyframeModel::ToString(RunState state) {
  return s_runStateNames[state];
}

bool KeyframeModel::IsPaused(RunState run_state) {
  return run_state == PAUSED || run_state == PAUSED_EXCLUSIVE;
}

std::unique_ptr<KeyframeModel> KeyframeModel::Create(
    std::unique_ptr<AnimationCurve> curve,
    int keyframe_model_id,
    int target_property_id) {
  return base::WrapUnique(new KeyframeModel(std::move(curve), keyframe_model_id,
                                            target_property_id));
}

KeyframeModel::KeyframeModel(std::unique_ptr<AnimationCurve> curve,
                             int keyframe_model_id,
                             int target_property_id)
    : curve_(std::move(curve)),
      id_(keyframe_model_id),
      target_property_(target_property_id),
      run_state_(WAITING_FOR_TARGET_AVAILABILITY),
      iterations_(1),
      iteration_start_(0),
      direction_(Direction::NORMAL),
      playback_rate_(1),
      fill_mode_(FillMode::BOTH),
      start_delay_(base::TimeDelta()),
      hold_time_(std::nullopt),
      auto_fills_on_finish_(false) {}

KeyframeModel::~KeyframeModel() {
  if (run_state() == RUNNING || IsPaused(run_state())) {
    SetRunState(ABORTED);
  }
}

int KeyframeModel::TargetProperty() const {
  return target_property_;
}

void KeyframeModel::SetRunState(RunState run_state) {
  run_state_ = run_state;
  if ((run_state_ == STARTING || run_state_ == RUNNING) &&
      auto_fills_on_finish_) {
    EnsureFillsWhenFinished();
  }
}

void KeyframeModel::Pause(base::TimeDelta hold_time, RunState pause_run_state) {
  CHECK(IsPaused(pause_run_state));
  start_time_.reset();
  set_hold_time(hold_time);
  SetRunState(pause_run_state);
}

void KeyframeModel::Reverse(base::TimeTicks monotonic_time) {
  base::TimeDelta current_time_to_match =
      CalculateHoldTime(monotonic_time, -playback_rate_);
  set_hold_time(std::nullopt);
  set_playback_rate(-playback_rate_);
  set_start_time(monotonic_time - current_time_to_match / playback_rate());
  SetRunState(RUNNING);
}

void KeyframeModel::UnpauseForTesting(base::TimeTicks monotonic_time) {
  set_start_time(monotonic_time - hold_time().value() / playback_rate());
  set_hold_time(std::nullopt);
  SetRunState(RUNNING);
}

KeyframeModel::Phase KeyframeModel::CalculatePhaseForTesting(
    base::TimeDelta local_time) const {
  return CalculatePhase(local_time);
}

KeyframeModel::Phase KeyframeModel::CalculatePhase(
    base::TimeDelta local_time) const {
  if ((local_time < start_delay_) ||
      (local_time == start_delay_ &&
       ((playback_rate_ < 0) ||
        (playback_rate_ > 0 && run_state_ == PAUSED_EXCLUSIVE)))) {
    return KeyframeModel::Phase::BEFORE;
  }
  // TODO(crbug.com/41428771): By spec end time = max(start delay + duration +
  // end delay, 0). The logic should be updated once "end delay" is supported.
  base::TimeDelta active_after_boundary_time = base::TimeDelta::Max();
  if (std::isfinite(iterations_)) {
    base::TimeDelta active_duration = curve_->Duration() * iterations_;
    active_after_boundary_time =
        std::max(start_delay_ + active_duration, base::TimeDelta());
  }
  if ((local_time > active_after_boundary_time) ||
      (local_time == active_after_boundary_time &&
       ((playback_rate_ > 0) ||
        (playback_rate_ < 0 && run_state_ == PAUSED_EXCLUSIVE)))) {
    return KeyframeModel::Phase::AFTER;
  }
  return KeyframeModel::Phase::ACTIVE;
}

std::optional<base::TimeDelta> KeyframeModel::CalculateActiveTime(
    base::TimeTicks monotonic_time) const {
  base::TimeDelta local_time = ConvertMonotonicTimeToLocalTime(monotonic_time);
  KeyframeModel::Phase phase = CalculatePhase(local_time);
  return CalculateActiveTime(local_time, phase);
}

std::optional<base::TimeDelta> KeyframeModel::CalculateActiveTime(
    base::TimeDelta local_time,
    KeyframeModel::Phase phase) const {
  DCHECK(playback_rate_);
  switch (phase) {
    case KeyframeModel::Phase::BEFORE:
      if (fill_mode_ == FillMode::BACKWARDS || fill_mode_ == FillMode::BOTH) {
        return std::max(local_time - start_delay_, base::TimeDelta());
      }
      return std::nullopt;
    case KeyframeModel::Phase::ACTIVE:
      return local_time - start_delay_;
    case KeyframeModel::Phase::AFTER:
      if (fill_mode_ == FillMode::FORWARDS || fill_mode_ == FillMode::BOTH) {
        DCHECK_NE(iterations_, std::numeric_limits<double>::infinity());
        base::TimeDelta active_duration = curve_->Duration() * iterations_;
        return std::max(std::min(local_time - start_delay_, active_duration),
                        base::TimeDelta());
      }
      return std::nullopt;
    default:
      NOTREACHED();
  }
}

bool KeyframeModel::IsFinishedAt(base::TimeTicks monotonic_time) const {
  if (is_finished())
    return true;

  if (StartShouldBeDeferred())
    return false;

  if (playback_rate_ == 0)
    return false;

  if (run_state_ != RUNNING || !std::isfinite(iterations_)) {
    return false;
  }

  return IsFinishedAtMonotonicTime(monotonic_time);
}

bool KeyframeModel::IsFinishedAtMonotonicTime(
    base::TimeTicks monotonic_time) const {
  base::TimeDelta local_time = ConvertMonotonicTimeToLocalTime(monotonic_time);
  base::TimeDelta active_time = local_time - start_delay_;

  return playback_rate_ < 0 ? active_time <= base::TimeDelta()
                            : active_time >= (curve_->Duration() * iterations_);
}

bool KeyframeModel::HasActiveTime(base::TimeTicks monotonic_time) const {
  return CalculateActiveTime(monotonic_time).has_value();
}

bool KeyframeModel::StartShouldBeDeferred() const {
  return false;
}

base::TimeDelta KeyframeModel::TrimTimeToCurrentIteration(
    base::TimeTicks monotonic_time,
    TimingFunction::LimitDirection* limit_direction) const {
  DCHECK(playback_rate_);
  DCHECK_GE(iteration_start_, 0);

  DCHECK(HasActiveTime(monotonic_time));

  base::TimeDelta local_time = ConvertMonotonicTimeToLocalTime(monotonic_time);
  KeyframeModel::Phase phase = CalculatePhase(local_time);
  base::TimeDelta active_time = CalculateActiveTime(local_time, phase).value();
  base::TimeDelta start_offset = curve_->Duration() * iteration_start_;

  if (limit_direction) {
    if (phase == KeyframeModel::Phase::BEFORE) {
      *limit_direction = TimingFunction::LimitDirection::LEFT;
    } else {
      *limit_direction = TimingFunction::LimitDirection::RIGHT;
    }
  }

  DCHECK(!active_time.is_negative());

  // Always return zero if we have no iterations.
  if (!iterations_) {
    return base::TimeDelta();
  }

  // Don't attempt to trim if we have no duration.
  if (curve_->Duration() <= base::TimeDelta()) {
    return base::TimeDelta();
  }

  base::TimeDelta repeated_duration = std::isfinite(iterations_)
                                          ? (curve_->Duration() * iterations_)
                                          : base::TimeDelta::Max();

  // Apply iterationStart.
  active_time += start_offset;

  // Calculate the iteration time
  base::TimeDelta iteration_time;
  bool has_defined_time_delta =
      (start_offset != active_time) ||
      !(start_offset.is_max() || start_offset.is_min());
  if (has_defined_time_delta &&
      active_time - start_offset == repeated_duration &&
      fmod(iterations_ + iteration_start_, 1) == 0) {
    iteration_time = curve_->Duration();
  } else {
    iteration_time = active_time % curve_->Duration();
  }

  // Calculate the current iteration
  int iteration;
  if (active_time <= base::TimeDelta()) {
    iteration = 0;
  } else if (iteration_time == curve_->Duration()) {
    iteration = ceil(iteration_start_ + iterations_ - 1);
  } else {
    iteration = base::ClampFloor(active_time / curve_->Duration());
  }

  // Check if we are running the keyframe model in reverse direction for the
  // current iteration
  bool reverse =
      (direction_ == Direction::REVERSE) ||
      (direction_ == Direction::ALTERNATE_NORMAL && iteration % 2 == 1) ||
      (direction_ == Direction::ALTERNATE_REVERSE && iteration % 2 == 0);

  // If we are running the keyframe model in reverse direction, reverse the
  // result
  if (reverse)
    iteration_time = curve_->Duration() - iteration_time;

  return iteration_time;
}

base::TimeDelta KeyframeModel::ConvertMonotonicTimeToLocalTime(
    base::TimeTicks monotonic_time) const {
  if (hold_time_) {
    return *hold_time_;
  }

  // When waiting on receiving a start time, then our global clock is 'stuck' at
  // the initial state.
  // TODO(crbug.com/497867796): To match blink/WAAPI, a null value for both
  // start_time_ and hold_time_ should result in a null local time instead of
  // zero. We should update clients that rely on the legacy behavior
  // of returning zero here.
  if ((run_state_ == STARTING && !has_set_start_time()) ||
      StartShouldBeDeferred()) {
    return base::TimeDelta();
  }

  return (monotonic_time - start_time_.value_or(monotonic_time)) *
         playback_rate_;
}

base::TimeDelta KeyframeModel::CalculateHoldTime(base::TimeTicks monotonic_time,
                                                 double playback_rate) const {
  if (hold_time_) {
    return hold_time_.value();
  }

  if (start_time_) {
    return std::max(base::TimeDelta(),
                    std::min(CalculateEndTime(),
                             ConvertMonotonicTimeToLocalTime(monotonic_time)));
  }

  return CalculateInitialHoldTime(playback_rate);
}

base::TimeDelta KeyframeModel::CalculateInitialHoldTime(
    double playback_rate) const {
  // This mirrors blink::Animation::pause/play setting hold time for an idle
  // animation.
  return playback_rate > 0 ? start_delay_ : CalculateEndTime();
}

base::TimeDelta KeyframeModel::CalculateEndTime() const {
  // TODO(crbug.com/41428771): Add support for end delay.
  return curve_->Duration() * iterations_ + start_delay_;
}

// TODO(https://b.corp.google.com/issues/497867796#comment5): Currently, only
// KeyframeModels created by Blink set auto_fills_on_finish_ to true. Ideally,
// we would always reflect the fill mode set by Blink. However, altering the
// fill mode is the simpler way to ensure that KeyframeModels that are finished
// by virtue of their current start time and/or hold time but haven't yet been
// played (this can happen for animation triggers [1]), respect their true fill
// mode. We should see if there is a less complicated way to accomplish this
// without modifying the fill mode.
//
// [1] https://drafts.csswg.org/animation-triggers-1
void KeyframeModel::EnsureFillsWhenFinished() {
  if (playback_rate() >= 0) {
    switch (fill_mode()) {
      case FillMode::NONE:
        set_fill_mode(FillMode::FORWARDS);
        break;
      case FillMode::BACKWARDS:
        set_fill_mode(FillMode::BOTH);
        break;
      case FillMode::FORWARDS:
      case FillMode::BOTH:
        break;
      case FillMode::AUTO:
        NOTREACHED();
    }
  } else {
    switch (fill_mode()) {
      case FillMode::NONE:
        set_fill_mode(FillMode::BACKWARDS);
        break;
      case FillMode::FORWARDS:
        set_fill_mode(FillMode::BOTH);
        break;
      case FillMode::BACKWARDS:
      case FillMode::BOTH:
        break;
      case FillMode::AUTO:
        NOTREACHED();
    }
  }
}

}  // namespace gfx

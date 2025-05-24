// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/lottie/animation.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/lottie/animation_observer.h"

namespace lottie {

namespace {

bool IsCycleValid(const Animation::CycleBoundaries& boundaries,
                  const Animation& animation) {
  return boundaries.start_offset >= base::TimeDelta() &&
         boundaries.end_offset <= animation.GetAnimationDuration() &&
         boundaries.start_offset < boundaries.end_offset;
}

bool IsInCycleBoundaries(base::TimeDelta offset,
                         const Animation::CycleBoundaries& boundaries) {
  return offset >= boundaries.start_offset && offset < boundaries.end_offset;
}

Animation::CycleBoundaries GetCycleAtIndex(
    const std::vector<Animation::CycleBoundaries>& scheduled_cycles,
    int cycle_idx) {
  DCHECK(!scheduled_cycles.empty());
  return scheduled_cycles[std::min(
      cycle_idx, static_cast<int>(scheduled_cycles.size()) - 1)];
}

}  // namespace

Animation::TimerControl::TimerControl(
    std::vector<CycleBoundaries> scheduled_cycles,
    base::TimeDelta initial_offset,
    int initial_completed_cycles,
    const base::TimeDelta& total_duration,
    const base::TimeTicks& start_timestamp,
    bool should_reverse,
    float playback_speed)
    : scheduled_cycles_(std::move(scheduled_cycles)),
      total_duration_(total_duration),
      previous_tick_(start_timestamp),
      current_cycle_progress_(initial_offset),
      should_reverse_(should_reverse),
      completed_cycles_(initial_completed_cycles),
      current_cycle_(GetCycleAtIndex(scheduled_cycles_, completed_cycles_)) {
  SetPlaybackSpeed(playback_speed);
}

Animation::TimerControl::~TimerControl() = default;

void Animation::TimerControl::SetPlaybackSpeed(float playback_speed) {
  DCHECK_GT(playback_speed, 0.f);
  playback_speed_ = playback_speed;
}

void Animation::TimerControl::Step(const base::TimeTicks& timestamp) {
  base::TimeDelta step_size = (timestamp - previous_tick_) * playback_speed_;
  while (!step_size.is_zero()) {
    base::TimeDelta time_until_current_cycle_end =
        IsPlayingInReverse()
            ? (current_cycle_progress_ - current_cycle_.start_offset)
            : (current_cycle_.end_offset - current_cycle_progress_);
    if (step_size >= time_until_current_cycle_end) {
      ++completed_cycles_;
      current_cycle_ = GetCycleAtIndex(scheduled_cycles_, completed_cycles_);
      current_cycle_progress_ = IsPlayingInReverse()
                                    ? current_cycle_.end_offset
                                    : current_cycle_.start_offset;
      step_size -= time_until_current_cycle_end;
    } else {
      if (IsPlayingInReverse()) {
        current_cycle_progress_ -= step_size;
      } else {
        current_cycle_progress_ += step_size;
      }
      step_size = base::TimeDelta();
    }
  }
  previous_tick_ = timestamp;
}

void Animation::TimerControl::Resume(const base::TimeTicks& timestamp) {
  previous_tick_ = timestamp;
}

double Animation::TimerControl::GetNormalizedCurrentCycleProgress() const {
  return current_cycle_progress_ / total_duration_;
}

double Animation::TimerControl::GetNormalizedStartOffset() const {
  return current_cycle_.start_offset / total_duration_;
}

double Animation::TimerControl::GetNormalizedEndOffset() const {
  return current_cycle_.end_offset / total_duration_;
}

bool Animation::TimerControl::IsPlayingInReverse() const {
  return should_reverse_ && completed_cycles_ % 2;
}

// static
Animation::CycleBoundaries Animation::CycleBoundaries::FullCycle(
    const Animation& animation) {
  return {
      /*start_offset=*/base::TimeDelta(),
      /*duration=*/animation.GetAnimationDuration(),
  };
}

// static
Animation::PlaybackConfig Animation::PlaybackConfig::CreateDefault(
    const Animation& animation) {
  return PlaybackConfig(
      /*scheduled_cycles=*/{CycleBoundaries::FullCycle(animation)},
      /*initial_offset=*/base::TimeDelta(),
      /*initial_completed_cycles=*/0, Animation::Style::kLoop);
}

// static
Animation::PlaybackConfig Animation::PlaybackConfig::CreateWithStyle(
    Style style,
    const Animation& animation) {
  PlaybackConfig config = CreateDefault(animation);
  config.style = style;
  return config;
}

Animation::PlaybackConfig::PlaybackConfig() = default;

Animation::PlaybackConfig::PlaybackConfig(
    std::vector<CycleBoundaries> scheduled_cycles,
    base::TimeDelta initial_offset,
    int initial_completed_cycles,
    Style style)
    : scheduled_cycles(std::move(scheduled_cycles)),
      initial_offset(initial_offset),
      initial_completed_cycles(initial_completed_cycles),
      style(style) {}

Animation::PlaybackConfig::PlaybackConfig(const PlaybackConfig& other) =
    default;

Animation::PlaybackConfig& Animation::PlaybackConfig::operator=(
    const PlaybackConfig& other) = default;

Animation::PlaybackConfig::~PlaybackConfig() = default;

Animation::Animation(scoped_refptr<cc::SkottieWrapper> skottie,
                     cc::SkottieColorMap color_map,
                     cc::SkottieFrameDataProvider* frame_data_provider)
    : skottie_(skottie),
      color_map_(std::move(color_map)),
      text_map_(skottie_->GetCurrentTextPropertyValues()) {
  DCHECK(skottie_);
  bool animation_has_external_image_assets =
      !skottie_->GetImageAssetMetadata().asset_storage().empty();
  // Embedded image assets would not be added to `asset_storage()` and not reach
  // here.
  if (animation_has_external_image_assets) {
    DCHECK(frame_data_provider) << "SkottieFrameDataProvider required for "
                                   "animations with external image assets";
    for (const auto& asset_metadata_pair :
         skottie_->GetImageAssetMetadata().asset_storage()) {
      const std::string& asset_id = asset_metadata_pair.first;
      const cc::SkottieResourceMetadataMap::ImageAssetMetadata& asset_metadata =
          asset_metadata_pair.second;
      scoped_refptr<cc::SkottieFrameDataProvider::ImageAsset> new_asset =
          frame_data_provider->LoadImageAsset(
              asset_id, asset_metadata.resource_path, asset_metadata.size);
      DCHECK(new_asset);
      image_assets_.emplace(cc::HashSkottieResourceId(asset_id),
                            std::move(new_asset));
    }
  }
}

Animation::~Animation() {
  observers_.Notify(&AnimationObserver::AnimationIsDeleting, this);
}

void Animation::AddObserver(AnimationObserver* observer) {
  observers_.AddObserver(observer);
}

void Animation::RemoveObserver(AnimationObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::TimeDelta Animation::GetAnimationDuration() const {
  return base::Seconds(skottie_->duration());
}

gfx::Size Animation::GetOriginalSize() const {
#if DCHECK_IS_ON()
  // The size should have no fractional component.
  gfx::SizeF float_size = gfx::SkSizeToSizeF(skottie_->size());
  gfx::Size rounded_size = gfx::ToRoundedSize(float_size);

  float height_diff = std::abs(float_size.height() - rounded_size.height());
  float width_diff = std::abs(float_size.width() - rounded_size.width());

  DCHECK_LE(height_diff, std::numeric_limits<float>::epsilon());
  DCHECK_LE(width_diff, std::numeric_limits<float>::epsilon());
#endif
  return gfx::ToRoundedSize(gfx::SkSizeToSizeF(skottie_->size()));
}

void Animation::Start(std::optional<PlaybackConfig> playback_config) {
  DCHECK(state_ == PlayState::kStopped || state_ == PlayState::kEnded);
  if (!playback_config)
    playback_config = PlaybackConfig::CreateDefault(*this);
  VerifyPlaybackConfigIsValid(*playback_config);

  // Reset the |timer_control_| object for a new animation play.
  timer_control_.reset(nullptr);

  if (gfx::Animation::PrefersReducedMotion()) {
    // Start in a paused state if "prefers reduced motion" is enabled on the
    // system.
    state_ = PlayState::kPaused;
  } else {
    // Schedule a play for the animation and store the necessary information
    // needed to start playing.
    state_ = PlayState::kSchedulePlay;
  }
  playback_config_ = std::move(*playback_config);
}

void Animation::Pause() {
  DCHECK(state_ == PlayState::kPlaying || state_ == PlayState::kSchedulePlay);
  state_ = PlayState::kPaused;
}

void Animation::ResumePlaying() {
  DCHECK(state_ == PlayState::kPaused);
  state_ = PlayState::kScheduleResume;
}

void Animation::Stop() {
  state_ = PlayState::kStopped;
  timer_control_.reset(nullptr);
  observers_.Notify(&AnimationObserver::AnimationStopped, this);
}

std::optional<float> Animation::GetCurrentProgress() const {
  switch (state_) {
    case PlayState::kStopped:
      return std::nullopt;
    case PlayState::kEnded:
      DCHECK(timer_control_);
      return timer_control_->GetNormalizedEndOffset();
    case PlayState::kPaused:
    case PlayState::kSchedulePlay:
    case PlayState::kPlaying:
    case PlayState::kScheduleResume:
      // The timer control may not have been initialized if OnAnimationStep has
      // not been called yet (meaning no frame has actually been painted yet and
      // there is no "progress" at all).
      if (timer_control_) {
        return timer_control_->GetNormalizedCurrentCycleProgress();
      } else {
        return std::nullopt;
      }
  }
}

std::optional<int> Animation::GetNumCompletedCycles() const {
  if (state_ == PlayState::kStopped)
    return std::nullopt;

  // This can happen if Start() has been called but a single frame has not been
  // painted yet.
  if (!timer_control_)
    return playback_config_.initial_completed_cycles;

  if (state_ == PlayState::kEnded) {
    DCHECK_EQ(playback_config_.style, Style::kLinear);
    return 1;
  }

  return timer_control_->completed_cycles();
}

std::optional<Animation::PlaybackConfig> Animation::GetPlaybackConfig() const {
  if (state_ == PlayState::kStopped) {
    return std::nullopt;
  } else {
    return playback_config_;
  }
}

std::optional<Animation::CycleBoundaries> Animation::GetCurrentCycleBoundaries()
    const {
  if (state_ == PlayState::kStopped || !timer_control_) {
    return std::nullopt;
  } else {
    return timer_control_->current_cycle();
  }
}

void Animation::Paint(gfx::Canvas* canvas,
                      const base::TimeTicks& timestamp,
                      const gfx::Size& size) {
  bool animation_cycle_ended = false;
  switch (state_) {
    case PlayState::kStopped:
      return;
    case PlayState::kSchedulePlay:
      InitTimer(timestamp);
      state_ = PlayState::kPlaying;
      observers_.Notify(&AnimationObserver::AnimationWillStartPlaying, this);
      break;
    case PlayState::kPlaying: {
      DCHECK(timer_control_);
      int previous_num_cycles = timer_control_->completed_cycles();
      timer_control_->Step(timestamp);
      int new_num_cycles = timer_control_->completed_cycles();
      animation_cycle_ended = new_num_cycles != previous_num_cycles;
      if (animation_cycle_ended && playback_config_.style == Style::kLinear)
        state_ = PlayState::kEnded;
    } break;
    case PlayState::kPaused:
      // The |timer_control_| may be null if the animation was Start()ed and
      // then Pause()ed before a single frame was painted. Initialize it here
      // so that GetCurrentProgress() below returns a valid timestamp.
      if (!timer_control_)
        InitTimer(timestamp);
      break;
    case PlayState::kScheduleResume:
      state_ = PlayState::kPlaying;
      if (timer_control_) {
        timer_control_->Resume(timestamp);
      } else {
        // The animation may have been paused after a play was scheduled but
        // before it started playing.
        InitTimer(timestamp);
      }
      observers_.Notify(&AnimationObserver::AnimationResuming, this);
      break;
    case PlayState::kEnded:
      break;
  }
  std::optional<float> current_progress = GetCurrentProgress();
  DCHECK(current_progress);
  PaintFrame(canvas, *current_progress, size);

  // Notify animation cycle ended after everything is done in case an observer
  // tries to change the animation's state within its observer implementation.
  if (animation_cycle_ended)
    TryNotifyAnimationCycleEnded();
}

void Animation::PaintFrame(gfx::Canvas* canvas,
                           float t,
                           const gfx::Size& size) {
  TRACE_EVENT1("ui", "Animation::PaintFrame", "timestamp", t);
  DCHECK_GE(t, 0.f);
  DCHECK_LE(t, 1.f);
  // Not all of the image assets necessarily appear in the frame at time |t|. To
  // determine which assets are actually needed, Seek() and capture the set of
  // images in the frame. Seek() without rendering is a cheap operation.
  cc::SkottieFrameDataMap all_frame_data;
  // Using Unretained is safe because the callback is guaranteed to be invoked
  // synchronously within Seek().
  skottie_->Seek(t, base::BindRepeating(&Animation::LoadImageForAsset,
                                        base::Unretained(this), canvas,
                                        std::ref(all_frame_data)));
  canvas->DrawSkottie(skottie(), gfx::Rect(size), t, std::move(all_frame_data),
                      color_map_, text_map_);
  observers_.Notify(&AnimationObserver::AnimationFramePainted, this, t);
}

void Animation::SetPlaybackSpeed(float playback_speed) {
  playback_speed_ = playback_speed;
  if (timer_control_)
    timer_control_->SetPlaybackSpeed(playback_speed_);
}

cc::SkottieWrapper::FrameDataFetchResult Animation::LoadImageForAsset(
    gfx::Canvas* canvas,
    cc::SkottieFrameDataMap& all_frame_data,
    cc::SkottieResourceIdHash asset_id,
    float t,
    sk_sp<SkImage>&,
    SkSamplingOptions&) {
  TRACE_EVENT0("ui", "Animation::LoadImageForAsset");
  cc::SkottieFrameDataProvider::ImageAsset& image_asset =
      *image_assets_.at(asset_id);
  all_frame_data.emplace(asset_id,
                         image_asset.GetFrameData(t, canvas->image_scale()));
  // Since this callback is only used for Seek() and not rendering, the output
  // arguments can be ignored and kNoUpdate can be returned.
  return cc::SkottieWrapper::FrameDataFetchResult::kNoUpdate;
}

void Animation::InitTimer(const base::TimeTicks& timestamp) {
  DCHECK(!timer_control_);
  timer_control_ = std::make_unique<TimerControl>(
      playback_config_.scheduled_cycles, playback_config_.initial_offset,
      playback_config_.initial_completed_cycles, GetAnimationDuration(),
      timestamp, playback_config_.style == Style::kThrobbing, playback_speed_);
}

void Animation::TryNotifyAnimationCycleEnded() {
  DCHECK(timer_control_);
  bool inform_observer = true;
  switch (playback_config_.style) {
    case Style::kLoop:
      break;
    case Style::kThrobbing:
      // For a throbbing animation, the animation cycle ends when the timer
      // goes from 0 to 1 and then back to 0. So the number of timer cycles
      // must be even at the end of one throbbing animation cycle.
      if (timer_control_->completed_cycles() % 2 != 0)
        inform_observer = false;
      break;
    case Style::kLinear:
      break;
  }

  // Inform observer if the cycle has ended.
  if (inform_observer) {
    observers_.Notify(&AnimationObserver::AnimationCycleEnded, this);
  }
}

void Animation::VerifyPlaybackConfigIsValid(
    const PlaybackConfig& playback_config) const {
  DCHECK(!playback_config.scheduled_cycles.empty());
  for (const CycleBoundaries& cycle : playback_config.scheduled_cycles) {
    DCHECK(IsCycleValid(cycle, *this));
  }
  if (playback_config.style == Style::kLinear) {
    DCHECK_EQ(playback_config.scheduled_cycles.size(), 1u);
  }
  DCHECK_GE(playback_config.initial_completed_cycles, 0);
  DCHECK(IsInCycleBoundaries(
      playback_config.initial_offset,
      GetCycleAtIndex(playback_config.scheduled_cycles,
                      playback_config.initial_completed_cycles)));
}

}  // namespace lottie

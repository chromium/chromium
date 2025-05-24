// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LOTTIE_ANIMATION_H_
#define UI_LOTTIE_ANIMATION_H_

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_frame_data_provider.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"
#include "ui/gfx/geometry/size.h"

class SkImage;
struct SkSamplingOptions;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace lottie {
class AnimationTest;
class AnimationObserver;

// This class is a wrapper over the Skia object for lottie vector graphic
// animations. It has its own timeline manager for the animation controls. The
// framerate of the animation and the animation ticks are controlled externally
// and hence the consumer must manage the timer and call paint at the desired
// frame per second.
// This helps keep multiple animations be synchronized by having a common
// external tick clock.
//
// Usage example:
//   1. Rendering a single frame on the canvas:
//        Animation animation_ = Animation(data);
//        animation_.Paint(canvas, t);
//
//   2. Playing the animation and rendering each frame:
//      void SampleClient::Init() {
//        Animation animation_ = Animation(data);
//        animation_.Start(Animation::PlaybackConfig::CreateWithStyle(
//            Animation::Style::kLinear, *animation_));
//      }
//
//      // overrides cc::CompositorAnimationObserver
//      void SampleClient::OnAnimationStep(TimeTicks* timestamp) {
//        timestamp_ = timestamp;
//        SchedulePaint();
//      }
//
//      void SampleClient::OnPaint(Canvas* canvas) {
//        animation_.Paint(canvas, timestamp_);
//      }
//
//   2. If you only want to play a subsection of the animation:
//      void SampleClient::Init() {
//        // This will seek to the 1st second of the animation and from there
//        // play it for 5 seconds.
//        Animation animation_ = Animation(data);
//        animation_.Start(Animation::PlaybackConfig({
//            Seconds(1), Seconds(5), Animation::Style::kLinear}));
//      }
//
//      // overrides cc::CompositorAnimationObserver
//      void SampleClient::OnAnimationStep(TimeTicks*) {
//        timestamp_ = timestamp;
//        SchedulePaint();
//      }
//
//      void SampleClient::OnPaint(Canvas* canvas) {
//        animation_.Paint(canvas, timestamp_, gfx::Size(10, 10));
//      }
//
class COMPONENT_EXPORT(UI_LOTTIE) Animation final {
 public:
  enum class Style {
    kLinear = 0,  // The animation plays from one time instant to another.
    kThrobbing,   // The animation plays from one time instant to another and
                  // then back. The animation plays in loop until stopped.
    kLoop         // Same as LINEAR, except the animation repeats after it ends.
  };

  // An animation goes through a single "cycle" when it's played from one
  // timestamp to another. After reaching the final timestamp, it may either
  // loop back to the initial timestamp again, or even play in reverse depending
  // on the style described above.
  struct COMPONENT_EXPORT(UI_LOTTIE) CycleBoundaries {
    // Returns the range [0, animation.GetAnimationDuration()).
    static CycleBoundaries FullCycle(const Animation& animation);

    // The cycle's range is [start_offset, end_offset). |start_offset| must be
    // < |end_offset|, and both must be in the range
    // [0, GetAnimationDuration()]. They represent non-normalized timestamps in
    // the animation.
    base::TimeDelta start_offset;
    base::TimeDelta end_offset;
  };

  struct COMPONENT_EXPORT(UI_LOTTIE) PlaybackConfig {
    // By default, loop from the beginning of the animation to the end.
    static PlaybackConfig CreateDefault(const Animation& animation);
    // Play from the beginning of the animation to the end with the provided
    // |style|.
    static PlaybackConfig CreateWithStyle(Style style,
                                          const Animation& animation);

    PlaybackConfig();
    PlaybackConfig(std::vector<CycleBoundaries> scheduled_cycles,
                   base::TimeDelta initial_offset,
                   int initial_completed_cycles,
                   Style style);
    PlaybackConfig(const PlaybackConfig& other);
    PlaybackConfig& operator=(const PlaybackConfig& other);
    ~PlaybackConfig();

    // Set of cycles that the animation will iterate through in the order they
    // appear. Must not be empty. After reaching the last entry in
    // |scheduled_cycles|, the animation will continue re-using the last entry's
    // boundaries in all future cycles.
    //
    // Example: {[0, T), [T/2, 3T/4)}. In the first cycle, the animation will
    // play starting at time 0 until it reaches timestamp T. After that, it will
    // loop back to timestamp T/2 and play until 3T/4. The [T/2, 3T/4) cycle
    // repeats indefinitely until the animation is stopped.
    //
    // If |style| is kLinear, |scheduled_cycles| must have exactly one entry.
    std::vector<CycleBoundaries> scheduled_cycles;

    // |initial_offset| and |initial_completed_cycles| combined dictate
    // where to start playing the animation from within the |scheduled_cycles|
    // above. The most common thing is to start playing from the very beginning
    // (|initial_offset| is the |start_offset| of the first scheduled cycle
    // and |initial_completed_cycles| is 0). But this allows the caller to
    // specify an arbitrary starting point.
    base::TimeDelta initial_offset;
    // The animation will start playing as if it has already completed the
    // number of cycles specified below. Note this not only dictates which
    // scheduled cycle the animation starts within, but also the initial
    // direction of the animation for throbbing animations.
    int initial_completed_cycles = 0;

    Style style = Style::kLoop;
  };

  // |frame_data_provider| may be null if it's known that the incoming skottie
  // animation does not contain any image assets.
  explicit Animation(
      scoped_refptr<cc::SkottieWrapper> skottie,
      cc::SkottieColorMap color_map = cc::SkottieColorMap(),
      cc::SkottieFrameDataProvider* frame_data_provider = nullptr);
  Animation(const Animation&) = delete;
  Animation& operator=(const Animation&) = delete;
  ~Animation();

  void AddObserver(AnimationObserver* observer);
  void RemoveObserver(AnimationObserver* observer);

  // Animation properties ------------------------------------------------------
  // Returns the total duration of the animation as reported by |animation_|.
  base::TimeDelta GetAnimationDuration() const;

  // Returns the size of the vector graphic as reported by |animation_|. This is
  // constant for a given |animation_|.
  gfx::Size GetOriginalSize() const;

  // Animation controls --------------------------------------------------------
  // This is an asynchronous call that would start playing the animation on the
  // next animation step. On a successful start the |observer_| would be
  // notified.
  //
  // If a null |playback_config| is provided, the default one is used.
  void Start(std::optional<PlaybackConfig> playback_config = std::nullopt);

  // Pauses the animation.
  void Pause();

  // This is an asynchronous call that would resume playing a paused animation
  // on the next animation step.
  void ResumePlaying();

  // Resets the animation to the first frame and stops.
  void Stop();

  // Returns the current normalized [0..1] value at which the animation frame
  // is.
  // 0 -> first frame and 1 -> last frame.
  //
  // Returns nullopt if a timestamp is currently is unavailable. This is the
  // case if:
  // * The animation is currently Stop()ed.
  // * The animation has been Start()ed but a single frame has not been painted
  //   yet.
  std::optional<float> GetCurrentProgress() const;

  // Returns the currently playing cycle within the PlaybackConfig's
  // |scheduled_cycles|. Returns nullopt under the same circumstances as
  // GetCurrentProgress().
  std::optional<CycleBoundaries> GetCurrentCycleBoundaries() const;

  // Returns the number of animation cycles that have been completed since
  // Play() was called, or nullopt if the animation is currently Stop()ed.
  std::optional<int> GetNumCompletedCycles() const;

  // Returns the currently active PlaybackConfig, or nullopt if the animation
  // is currently Stop()ed.
  std::optional<PlaybackConfig> GetPlaybackConfig() const;

  // Paint operations ----------------------------------------------------------
  // Paints the frame of the animation for the given |timestamp| at the given
  // |size|.
  void Paint(gfx::Canvas* canvas,
             const base::TimeTicks& timestamp,
             const gfx::Size& size);

  // Paints the frame of the animation for the normalized time instance |t|. Use
  // this for special cases when you want to manually manage which frame to
  // paint.
  void PaintFrame(gfx::Canvas* canvas, float t, const gfx::Size& size);

  // Returns the skottie object that contins the animation data.
  scoped_refptr<cc::SkottieWrapper> skottie() const { return skottie_; }

  // Returns the text nodes in the animation and their corresponding current
  // property values. The text nodes' initial property values reflect those
  // embedded in the Lottie animation file. A mutable reference is returned
  // so that the caller may modify the text map with its own custom values
  // before calling Paint(). The caller may do so as many times as desired.
  cc::SkottieTextPropertyValueMap& text_map() { return text_map_; }

  // Sets the rate at which the animation will be played. A |playback_speed| of
  // 1 renders exactly in real time, 0.5 is half as fast, 2 is twice as fast,
  // etc. This may be called at any time, and the |timestamp| passed to Paint()
  // is automatically adjusted internally to account for the playback speed.
  //
  // Defaults to 1 if not called.
  void SetPlaybackSpeed(float playback_speed);

 private:
  friend class AnimationTest;

  enum class PlayState {
    kStopped = 0,     // Animation is stopped.
    kSchedulePlay,    // Animation will start playing on the next animatin step.
    kPlaying,         // Animation is playing.
    kPaused,          // Animation is paused.
    kScheduleResume,  // Animation will resume playing on the next animation
                      // step
    kEnded            // Animation has ended.
  };

  // Class to manage the timeline when playing the animation. Manages the
  // normalized progress [0..1] between the given start and end offset. If the
  // reverse flag is set, the progress runs in reverse.
  class COMPONENT_EXPORT(UI_LOTTIE) TimerControl final {
   public:
    TimerControl(std::vector<CycleBoundaries> scheduled_cycles,
                 base::TimeDelta initial_offset,
                 int initial_completed_cycles,
                 const base::TimeDelta& total_duration,
                 const base::TimeTicks& start_timestamp,
                 bool should_reverse,
                 float playback_speed);
    ~TimerControl();
    TimerControl(const TimerControl&) = delete;
    TimerControl& operator=(const TimerControl&) = delete;

    // Update timeline progress based on the new timetick |timestamp|.
    void Step(const base::TimeTicks& timestamp);

    // Resumes the timer.
    void Resume(const base::TimeTicks& timestamp);

    void SetPlaybackSpeed(float playback_speed);

    double GetNormalizedCurrentCycleProgress() const;
    double GetNormalizedStartOffset() const;
    double GetNormalizedEndOffset() const;
    int completed_cycles() const { return completed_cycles_; }
    CycleBoundaries current_cycle() const { return current_cycle_; }

   private:
    friend class AnimationTest;

    // Only applies to throbbing animations, for which every even numbered
    // cycle plays forwards, and every odd numbered cycle plays reversed.
    bool IsPlayingInReverse() const;

    // See comments in |PlaybackConfig::scheduled_cycles|.
    const std::vector<CycleBoundaries> scheduled_cycles_;

    // Total duration of all cycles.
    const base::TimeDelta total_duration_;

    // The timetick at which |progress_| was updated last.
    base::TimeTicks previous_tick_;

    // This is the progress of the timer in the current cycle.
    base::TimeDelta current_cycle_progress_;

    // If true, the progress will go into reverse after each cycle. This is used
    // for throbbing animations.
    const bool should_reverse_ = false;

    // The number of times each |cycle_duration_| is covered by the timer.
    int completed_cycles_ = 0;

    // See comments above SetPlaybackSpeed().
    float playback_speed_ = 1.f;

    // The boundaries of the current cycle. This is a copy of one of the entries
    // in |scheduled_cycles_|.
    CycleBoundaries current_cycle_;
  };

  void InitTimer(const base::TimeTicks& timestamp);
  void TryNotifyAnimationCycleEnded();
  cc::SkottieWrapper::FrameDataFetchResult LoadImageForAsset(
      gfx::Canvas* canvas,
      cc::SkottieFrameDataMap& all_frame_data,
      cc::SkottieResourceIdHash asset_id,
      float t,
      sk_sp<SkImage>&,
      SkSamplingOptions&);
  void VerifyPlaybackConfigIsValid(const PlaybackConfig& playback_config) const;

  // Manages the timeline for the current playing animation.
  std::unique_ptr<TimerControl> timer_control_;

  // The current state of animation.
  PlayState state_ = PlayState::kStopped;

  // The config from the most recent call to Start().
  PlaybackConfig playback_config_;

  base::ObserverList<AnimationObserver> observers_;

  scoped_refptr<cc::SkottieWrapper> skottie_;
  cc::SkottieColorMap color_map_;
  cc::SkottieTextPropertyValueMap text_map_;
  base::flat_map<cc::SkottieResourceIdHash,
                 scoped_refptr<cc::SkottieFrameDataProvider::ImageAsset>>
      image_assets_;

  float playback_speed_ = 1.f;
};

}  // namespace lottie

#endif  // UI_LOTTIE_ANIMATION_H_

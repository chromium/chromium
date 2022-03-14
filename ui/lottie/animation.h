// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LOTTIE_ANIMATION_H_
#define UI_LOTTIE_ANIMATION_H_

#include <functional>
#include <memory>

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
//        animation_.Start(Animation::Style::LINEAR);
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
//        animation_.Start(Seconds(1),
//                         Seconds(5));
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
  // notified. Use this if you want to play the entire animation.
  void Start(Style style = Style::kLoop);

  // This is an asynchronous call that would start playing the animation on the
  // next animation step. On a successful start the |observer_| would be
  // notified.
  // The animation will be scheduled to play from the |start_offset| to
  // |start_offset| + |duration|. The values will be clamped so as to not go out
  // of bounds.
  void StartSubsection(base::TimeDelta start_offset,
                       base::TimeDelta duration,
                       Style style = Style::kLoop);

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
  float GetCurrentProgress() const;

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
    TimerControl(const base::TimeDelta& offset,
                 const base::TimeDelta& cycle_duration,
                 const base::TimeDelta& total_duration,
                 const base::TimeTicks& start_timestamp,
                 bool should_reverse);
    ~TimerControl() = default;
    TimerControl(const TimerControl&) = delete;
    TimerControl& operator=(const TimerControl&) = delete;

    // Update timeline progress based on the new timetick |timestamp|.
    void Step(const base::TimeTicks& timestamp);

    // Resumes the timer.
    void Resume(const base::TimeTicks& timestamp);

    double GetNormalizedCurrentCycleProgress() const;
    double GetNormalizedStartOffset() const;
    double GetNormalizedEndOffset() const;
    int completed_cycles() const { return completed_cycles_; }

   private:
    friend class AnimationTest;

    // Time duration from 0 which marks the beginning of a cycle.
    const base::TimeDelta start_offset_;

    // Time duration from 0 which marks the end of a cycle.
    const base::TimeDelta end_offset_;

    // Time duration for one cycle. This is essentially a cache of the
    // difference between |end_offset_| - |start_offset_|.
    const base::TimeDelta cycle_duration_;

    // Total duration of all cycles.
    const base::TimeDelta total_duration_;

    // The timetick at which |progress_| was updated last.
    base::TimeTicks previous_tick_;

    // The progress of the timer. This is a monotonically increasing value.
    base::TimeDelta progress_;

    // This is the progress of the timer in the current cycle.
    base::TimeDelta current_cycle_progress_;

    // If true, the progress will go into reverse after each cycle. This is used
    // for throbbing animations.
    bool should_reverse_ = false;

    // The number of times each |cycle_duration_| is covered by the timer.
    int completed_cycles_ = 0;
  };

  void InitTimer(const base::TimeTicks& timestamp);
  void TryNotifyAnimationCycleEnded() const;
  cc::SkottieWrapper::FrameDataFetchResult LoadImageForAsset(
      gfx::Canvas* canvas,
      cc::SkottieFrameDataMap& all_frame_data,
      cc::SkottieResourceIdHash asset_id,
      float t,
      sk_sp<SkImage>&,
      SkSamplingOptions&);

  // Manages the timeline for the current playing animation.
  std::unique_ptr<TimerControl> timer_control_;

  // The style of animation to play.
  Style style_ = Style::kLoop;

  // The current state of animation.
  PlayState state_ = PlayState::kStopped;

  // The below values of scheduled_* are set when we have scheduled a play.
  // These will be used to initialize |timer_control_|.
  base::TimeDelta scheduled_start_offset_;
  base::TimeDelta scheduled_duration_;

  base::ObserverList<AnimationObserver> observers_;

  scoped_refptr<cc::SkottieWrapper> skottie_;
  cc::SkottieColorMap color_map_;
  cc::SkottieTextPropertyValueMap text_map_;
  base::flat_map<cc::SkottieResourceIdHash,
                 scoped_refptr<cc::SkottieFrameDataProvider::ImageAsset>>
      image_assets_;
};

}  // namespace lottie

#endif  // UI_LOTTIE_ANIMATION_H_

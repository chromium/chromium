// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATION_ELEMENT_H_
#define UI_COMPOSITOR_LAYER_ANIMATION_ELEMENT_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/keyframe_model.h"
#include "cc/trees/target_property.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/transform.h"

namespace ui {

class InterpolatedTransform;
class LayerAnimationDelegate;

class AnimationMetricsReporter {
 public:
  virtual ~AnimationMetricsReporter() {}
  virtual void Report(int value) = 0;
};

// LayerAnimationElements represent one segment of an animation between two
// keyframes. They know how to update a LayerAnimationDelegate given a value
// between 0 and 1 (0 for initial, and 1 for final).
class COMPOSITOR_EXPORT LayerAnimationElement {
 public:
  enum AnimatableProperty {
    UNKNOWN = 0,
    TRANSFORM = (1 << 0),
    BOUNDS = (1 << 1),
    OPACITY = (1 << 2),
    VISIBILITY = (1 << 3),
    BRIGHTNESS = (1 << 4),
    GRAYSCALE = (1 << 5),
    COLOR = (1 << 6),
    CLIP = (1 << 7),
    ROUNDED_CORNERS = (1 << 8),

    // Used when iterating over properties.
    FIRST_PROPERTY = TRANSFORM,
    SENTINEL = (1 << 9)
  };

  static AnimatableProperty ToAnimatableProperty(
      cc::TargetProperty::Type property);

  struct COMPOSITOR_EXPORT TargetValue {
    TargetValue();
    // Initializes the target value to match the delegate. NULL may be supplied.
    explicit TargetValue(const LayerAnimationDelegate* delegate);

    gfx::Rect bounds;
    gfx::Transform transform;
    float opacity;
    bool visibility;
    float brightness;
    float grayscale;
    SkColor color;
    gfx::Rect clip_rect;
    gfx::RoundedCornersF rounded_corners;
  };

  typedef uint32_t AnimatableProperties;

  LayerAnimationElement(AnimatableProperties properties,
                        base::TimeDelta duration);

  virtual ~LayerAnimationElement();

  static std::string AnimatablePropertiesToString(
      AnimatableProperties properties);

  // Creates an element that transitions to the given transform. The caller owns
  // the return value.
  static std::unique_ptr<LayerAnimationElement> CreateTransformElement(
      const gfx::Transform& transform,
      base::TimeDelta duration);

  // Creates an element that transitions to another in a way determined by an
  // interpolated transform. The element accepts ownership of the interpolated
  // transform. NB: at every step, the interpolated transform clobbers the
  // existing transform. That is, it does not interpolate between the existing
  // transform and the last value the interpolated transform will assume. It is
  // therefore important that the value of the interpolated at time 0 matches
  // the current transform.
  static std::unique_ptr<LayerAnimationElement>
  CreateInterpolatedTransformElement(
      std::unique_ptr<InterpolatedTransform> interpolated_transform,
      base::TimeDelta duration);

  // Creates an element that transitions to the given bounds. The caller owns
  // the return value.
  static std::unique_ptr<LayerAnimationElement> CreateBoundsElement(
      const gfx::Rect& bounds,
      base::TimeDelta duration);

  // Creates an element that transitions to the given opacity. The caller owns
  // the return value.
  static std::unique_ptr<LayerAnimationElement> CreateOpacityElement(
      float opacity,
      base::TimeDelta duration);

  // Creates an element that sets visibily following a delay. The caller owns
  // the return value.
  static std::unique_ptr<LayerAnimationElement> CreateVisibilityElement(
      bool visibility,
      base::TimeDelta duration);

  // Creates an element that transitions to the given brightness.
  // The caller owns the return value.
  static std::unique_ptr<LayerAnimationElement> CreateBrightnessElement(
      float brightness,
      base::TimeDelta duration);

  // Creates an element that transitions to the given grayscale value.
  // The caller owns the return value.
  static std::unique_ptr<LayerAnimationElement> CreateGrayscaleElement(
      float grayscale,
      base::TimeDelta duration);

  // Creates an element that pauses the given properties. The caller owns the
  // return value.
  static std::unique_ptr<LayerAnimationElement> CreatePauseElement(
      AnimatableProperties properties,
      base::TimeDelta duration);

  // Creates an element that transitions to the given color. The caller owns the
  // return value.
  static std::unique_ptr<LayerAnimationElement> CreateColorElement(
      SkColor color,
      base::TimeDelta duration);

  // Creates an element that transitions the clip rect of the layer to the given
  // bounds. The caller owns the return value.
  static std::unique_ptr<LayerAnimationElement> CreateClipRectElement(
      const gfx::Rect& clip_rect,
      base::TimeDelta duration);

  // Creates an element that transitions the rounded corners of the layer to the
  // given ones. The caller owns the return value.
  static std::unique_ptr<LayerAnimationElement> CreateRoundedCornersElement(
      const gfx::RoundedCornersF& rounded_corners,
      base::TimeDelta duration);

  // Sets the start time for the animation. This must be called before the first
  // call to {Start, IsFinished}. Once the animation is finished, this must
  // be called again in order to restart the animation.
  void set_requested_start_time(base::TimeTicks start_time) {
    requested_start_time_ = start_time;
  }
  base::TimeTicks requested_start_time() const { return requested_start_time_; }

  // Sets the actual start time for the animation, taking into account any
  // queueing delays.
  void set_effective_start_time(base::TimeTicks start_time) {
    effective_start_time_ = start_time;
  }
  base::TimeTicks effective_start_time() const { return effective_start_time_; }

  // This must be called before the first call to Progress. If starting the
  // animation involves dispatching to another thread, then this will proceed
  // with that dispatch, ultimately resulting in the animation getting an
  // effective start time (the time the animation starts on the other thread).
  void Start(LayerAnimationDelegate* delegate, int animation_group_id);

  // Returns true if the animation has started but hasn't finished.
  bool Started() const { return !first_frame_; }

  // Updates the delegate to the appropriate value for |now|. Returns true
  // if a redraw is required.
  bool Progress(base::TimeTicks now, LayerAnimationDelegate* delegate);

  // If calling Progress now, with the given time, will finish the animation,
  // returns true and sets |end_duration| to the actual duration for this
  // animation, incuding any queueing delays.
  bool IsFinished(base::TimeTicks time, base::TimeDelta* total_duration);

  // Updates the delegate to the end of the animation. Returns true if a
  // redraw is required.
  bool ProgressToEnd(LayerAnimationDelegate* delegate);

  // Called if the animation is not allowed to complete. This may be called
  // before OnStarted or Progress.
  void Abort(LayerAnimationDelegate* delegate);

  // Assigns the target value to |target|.
  void GetTargetValue(TargetValue* target) const;

  // The properties that the element modifies.
  AnimatableProperties properties() const { return properties_; }

  // Whether this element animates on the compositor thread.
  virtual bool IsThreaded(LayerAnimationDelegate* delegate) const;

  gfx::Tween::Type tween_type() const { return tween_type_; }
  void set_tween_type(gfx::Tween::Type tween_type) { tween_type_ = tween_type; }

  void set_animation_metrics_reporter(AnimationMetricsReporter* reporter) {
    animation_metrics_reporter_ = reporter;
  }

  // Each LayerAnimationElement has a unique keyframe_model_id. Elements
  // belonging to sequences that are supposed to start together have the same
  // animation_group_id.
  int keyframe_model_id() const { return keyframe_model_id_; }
  int animation_group_id() const { return animation_group_id_; }
  void set_animation_group_id(int id) { animation_group_id_ = id; }

  base::TimeDelta duration() const { return duration_; }

  // The fraction of the animation that has been completed after the last
  // call made to {Progress, ProgressToEnd}.
  double last_progressed_fraction() const { return last_progressed_fraction_; }

  std::string ToString() const;

 protected:
  virtual std::string DebugName() const;

  // Called once each time the animation element is run before any call to
  // OnProgress.
  virtual void OnStart(LayerAnimationDelegate* delegate) = 0;
  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) = 0;
  virtual void OnGetTarget(TargetValue* target) const = 0;
  virtual void OnAbort(LayerAnimationDelegate* delegate) = 0;

  // Actually start the animation, dispatching to another thread if needed.
  virtual void RequestEffectiveStart(LayerAnimationDelegate* delegate);

  LayerAnimationElement(const LayerAnimationElement& element);

 private:
  // For debugging purposes, we sometimes alter the duration we actually use.
  // For example, during tests we often set duration = 0, and it is sometimes
  // useful to slow animations down to see them more clearly.
  base::TimeDelta GetEffectiveDuration(const base::TimeDelta& delta);

  bool first_frame_;
  const AnimatableProperties properties_;
  base::TimeTicks requested_start_time_;

  // When the animation actually started, taking into account queueing delays.
  base::TimeTicks effective_start_time_;
  const base::TimeDelta duration_;
  gfx::Tween::Type tween_type_;

  const int keyframe_model_id_;
  int animation_group_id_;

  double last_progressed_fraction_;

  // To obtain metrics of animation performance tag animation elements and
  // keep track of sequential compositor frame number.
  AnimationMetricsReporter* animation_metrics_reporter_;
  int start_frame_number_;

  base::WeakPtrFactory<LayerAnimationElement> weak_ptr_factory_{this};

  DISALLOW_ASSIGN(LayerAnimationElement);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATION_ELEMENT_H_

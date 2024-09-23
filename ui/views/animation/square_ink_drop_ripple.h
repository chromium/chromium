// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_SQUARE_INK_DROP_RIPPLE_H_
#define UI_VIEWS_ANIMATION_SQUARE_INK_DROP_RIPPLE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class CircleLayerDelegate;
class InkDropHost;
class RectangleLayerDelegate;

namespace test {
class SquareInkDropRippleTestApi;
}  // namespace test

// An ink drop ripple that smoothly animates between a circle and a rounded
// rectangle of different sizes for each of the different InkDropStates. The
// final frame for each InkDropState will be bounded by either a |large_size_|
// rectangle or a |small_size_| rectangle.
//
// The valid InkDropState transitions are defined below:
//
//   {All InkDropStates}           => HIDDEN
//   HIDDEN                        => ACTION_PENDING
//   HIDDEN, ACTION_PENDING        => ACTION_TRIGGERED
//   ACTION_PENDING                => ALTERNATE_ACTION_PENDING
//   ALTERNATE_ACTION_PENDING      => ALTERNATE_ACTION_TRIGGERED
//   {All InkDropStates}           => ACTIVATED
//   {All InkDropStates}           => DEACTIVATED
//
class VIEWS_EXPORT SquareInkDropRipple : public InkDropRipple {
 public:
  // The shape to use for the ACTIVATED/DEACTIVATED states.
  enum class ActivatedShape { kCircle, kRoundedRect };

  SquareInkDropRipple(InkDropHost* ink_drop_host,
                      const gfx::Size& large_size,
                      int large_corner_radius,
                      const gfx::Size& small_size,
                      int small_corner_radius,
                      const gfx::Point& center_point,
                      SkColor color,
                      float visible_opacity);
  SquareInkDropRipple(const SquareInkDropRipple&) = delete;
  SquareInkDropRipple& operator=(const SquareInkDropRipple&) = delete;
  ~SquareInkDropRipple() override;

  void set_activated_shape(ActivatedShape shape) { activated_shape_ = shape; }

  // InkDropRipple:
  ui::Layer* GetRootLayer() override;

 private:
  friend class test::SquareInkDropRippleTestApi;

  // Enumeration of the different shapes that compose the ink drop.
  enum PaintedShape {
    TOP_LEFT_CIRCLE = 0,
    TOP_RIGHT_CIRCLE,
    BOTTOM_RIGHT_CIRCLE,
    BOTTOM_LEFT_CIRCLE,
    HORIZONTAL_RECT,
    VERTICAL_RECT,
    // The total number of shapes, not an actual shape.
    PAINTED_SHAPE_COUNT
  };

  // Returns a human readable string for the |painted_shape| value.
  static std::string ToLayerName(PaintedShape painted_shape);

  // Type that contains a gfx::Tansform for each of the layers required by the
  // ink drop.
  using InkDropTransforms = std::array<gfx::Transform, PAINTED_SHAPE_COUNT>;

  float GetCurrentOpacity() const;

  // InkDropRipple:
  void AnimateStateChange(InkDropState old_ink_drop_state,
                          InkDropState new_ink_drop_state) override;
  void SetStateToActivated() override;
  void SetStateToHidden() override;
  void AbortAllAnimations() override;

  // Sets the |transforms| on all of the shape layers. Note that this does not
  // perform any animation.
  void SetTransforms(const InkDropTransforms transforms);

  // Sets the opacity of the ink drop. Note that this does not perform any
  // animation.
  void SetOpacity(float opacity);

  // Updates all of the Transforms in |transforms_out| for a circle of the given
  // |size|.
  void CalculateCircleTransforms(const gfx::Size& desired_size,
                                 InkDropTransforms* transforms_out) const;

  // Updates all of the Transforms in |transforms_out| for a rounded rectangle
  // of the given |desired_size| and |corner_radius|. The effective size may
  // differ from |desired_size| at certain scale factors to make sure the ripple
  // is pixel-aligned.
  void CalculateRectTransforms(const gfx::Size& desired_size,
                               float corner_radius,
                               InkDropTransforms* transforms_out) const;

  // Calculates a Transform for a circle layer.
  gfx::Transform CalculateCircleTransform(float scale,
                                          float target_center_x,
                                          float target_center_y) const;

  // Calculates a Transform for a rectangle layer.
  gfx::Transform CalculateRectTransform(float x_scale, float y_scale) const;

  // Updates all of the Transforms in |transforms_out| to the current Transforms
  // of the painted shape Layers.
  void GetCurrentTransforms(InkDropTransforms* transforms_out) const;

  // Updates all of the Transforms in |transforms_out| with the target
  // Transforms for the ACTIVATED animation.
  void GetActivatedTargetTransforms(InkDropTransforms* transforms_out) const;

  // Updates all of the Transforms in |transforms_out| with the target
  // Transforms for the DEACTIVATED animation.
  void GetDeactivatedTargetTransforms(InkDropTransforms* transforms_out) const;

  // Adds and configures a new |painted_shape| layer to |painted_layers_|.
  void AddPaintLayer(PaintedShape painted_shape);

  // Called from LayerAnimator when a new LayerAnimationSequence is scheduled
  // which allows for assigning the observer to the sequence.
  void OnLayerAnimationSequenceScheduled(ui::LayerAnimationSequence* sequence);

  // The shape used for the ACTIVATED/DEACTIVATED states.
  ActivatedShape activated_shape_ = ActivatedShape::kRoundedRect;

  // Ink drop opacity when it is visible.
  float visible_opacity_;

  // Maximum size that an ink drop will be drawn to for any InkDropState whose
  // final frame should be large.
  const gfx::Size large_size_;

  // Corner radius used to draw the rounded rectangles corner for any
  // InkDropState whose final frame should be large.
  const int large_corner_radius_;

  // Maximum size that an ink drop will be drawn to for any InkDropState whose
  // final frame should be small.
  const gfx::Size small_size_;

  // Corner radius used to draw the rounded rectangles corner for any
  // InkDropState whose final frame should be small.
  const int small_corner_radius_;

  // The center point of the ripple, relative to the root layer's origin.
  gfx::Point center_point_;

  // ui::LayerDelegate to paint circles for all the circle layers.
  std::unique_ptr<CircleLayerDelegate> circle_layer_delegate_;

  // ui::LayerDelegate to paint rectangles for all the rectangle layers.
  std::unique_ptr<RectangleLayerDelegate> rect_layer_delegate_;

  // The root layer that parents the animating layers. The root layer is used to
  // manipulate opacity and location, and its children are used to manipulate
  // the different painted shapes that compose the ink drop.
  ui::Layer root_layer_;

  // Sequence scheduled callback subscription for the root layer.
  base::CallbackListSubscription root_callback_subscription_;

  // ui::Layers for all of the painted shape layers that compose the ink drop.
  std::array<std::unique_ptr<ui::Layer>, PAINTED_SHAPE_COUNT> painted_layers_;

  // Sequence scheduled callback subscriptions for the painted layers.
  std::array<base::CallbackListSubscription, PAINTED_SHAPE_COUNT>
      callback_subscriptions_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_SQUARE_INK_DROP_RIPPLE_H_

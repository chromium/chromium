// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_element.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

namespace {

// Verify that the TargetValue(TestLayerAnimationDelegate*) constructor
// correctly assigns values. See www.crbug.com/483134.
TEST(TargetValueTest, VerifyLayerAnimationDelegateConstructor) {
  const gfx::Rect kBounds(1, 2, 3, 5);
  const auto kTransform =
      gfx::Transform::Affine(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f);
  const float kOpacity = 1.235f;
  const bool kVisibility = false;
  const float kBrightness = 2.358f;
  const float kGrayscale = 2.5813f;
  const SkColor kColor = SK_ColorCYAN;
  const gfx::Rect kClipRect(2, 3, 4, 5);
  const gfx::RoundedCornersF kRoundedCorners(2.0f, 3.0f, 4.0f, 5.0f);

  TestLayerAnimationDelegate delegate;
  delegate.SetBoundsFromAnimation(kBounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetTransformFromAnimation(kTransform,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetOpacityFromAnimation(kOpacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetVisibilityFromAnimation(kVisibility,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBrightnessFromAnimation(kBrightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetGrayscaleFromAnimation(kGrayscale,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetColorFromAnimation(kColor,
                                 PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetClipRectFromAnimation(kClipRect,
                                    PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetRoundedCornersFromAnimation(
      kRoundedCorners, PropertyChangeReason::NOT_FROM_ANIMATION);

  LayerAnimationElement::TargetValue target_value(&delegate);

  EXPECT_EQ(kBounds, target_value.bounds);
  EXPECT_EQ(kTransform, target_value.transform);
  EXPECT_FLOAT_EQ(kOpacity, target_value.opacity);
  EXPECT_EQ(kVisibility, target_value.visibility);
  EXPECT_FLOAT_EQ(kBrightness, target_value.brightness);
  EXPECT_FLOAT_EQ(kGrayscale, target_value.grayscale);
  EXPECT_EQ(SK_ColorCYAN, target_value.color);
  EXPECT_EQ(kClipRect, target_value.clip_rect);
  EXPECT_EQ(kRoundedCorners, target_value.rounded_corners);
}

// Check that the transformation element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, TransformElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Transform start_transform, target_transform;
  start_transform.Rotate(-30.0);
  target_transform.Rotate(30.0);
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::Seconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateTransformElement(target_transform, delta);
  element->set_animation_group_id(1);

  for (int i = 0; i < 2; ++i) {
    start_time = effective_start_time + delta;
    element->set_requested_start_time(start_time);
    delegate.SetTransformFromAnimation(
        start_transform, PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    CheckApproximatelyEqual(start_transform,
                            delegate.GetTransformForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    effective_start_time = start_time + delta;
    element->set_effective_start_time(effective_start_time);
    element->Progress(effective_start_time, &delegate);
    EXPECT_FLOAT_EQ(0.0, element->last_progressed_fraction());
    element->Progress(effective_start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(0.5, element->last_progressed_fraction());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(effective_start_time + delta,
                                    &element_duration));
    EXPECT_EQ(2 * delta, element_duration);

    element->Progress(effective_start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(1.0, element->last_progressed_fraction());
    CheckApproximatelyEqual(target_transform,
                            delegate.GetTransformForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target_transform, target_value.transform);
}

// Check that the bounds element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, BoundsElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Rect start, target, middle;
  start = target = middle = gfx::Rect(0, 0, 50, 50);
  start.set_x(-90);
  target.set_x(90);
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateBoundsElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetBoundsFromAnimation(start,
                                    PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    CheckApproximatelyEqual(start, delegate.GetBoundsForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    element->Progress(start_time + delta/2, &delegate);
    CheckApproximatelyEqual(middle, delegate.GetBoundsForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    CheckApproximatelyEqual(target, delegate.GetBoundsForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target, target_value.bounds);
}

// Check that the opacity element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, OpacityElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time = effective_start_time + delta;
    element->set_requested_start_time(start_time);
    delegate.SetOpacityFromAnimation(start,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, element->last_progressed_fraction());
    effective_start_time = start_time + delta;
    element->set_effective_start_time(effective_start_time);
    element->Progress(effective_start_time, &delegate);
    EXPECT_FLOAT_EQ(start, element->last_progressed_fraction());
    element->Progress(effective_start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, element->last_progressed_fraction());

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(effective_start_time + delta,
                                    &element_duration));
    EXPECT_EQ(2 * delta, element_duration);

    element->Progress(effective_start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(target, element->last_progressed_fraction());
    EXPECT_FLOAT_EQ(target, delegate.GetOpacityForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.opacity);
}

// Check that the visibility element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, VisibilityElement) {
  TestLayerAnimationDelegate delegate;
  bool start = true;
  bool target = false;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateVisibilityElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetVisibilityFromAnimation(
        start, PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_TRUE(delegate.GetVisibilityForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    element->Progress(start_time + delta/2, &delegate);
    EXPECT_TRUE(delegate.GetVisibilityForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    EXPECT_FALSE(delegate.GetVisibilityForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FALSE(target_value.visibility);
}

// Check that the Brightness element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, BrightnessElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateBrightnessElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetBrightnessFromAnimation(
        start, PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetBrightnessForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    element->Progress(start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetBrightnessForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetBrightnessForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.brightness);
}

// Check that the Grayscale element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, GrayscaleElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float middle = 0.5;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateGrayscaleElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetGrayscaleFromAnimation(
        start, PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetGrayscaleForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    element->Progress(start_time + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetGrayscaleForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetGrayscaleForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target, target_value.grayscale);
}

// Check that the pause element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, PauseElement) {
  LayerAnimationElement::AnimatableProperties properties =
      LayerAnimationElement::TRANSFORM | LayerAnimationElement::BOUNDS |
      LayerAnimationElement::OPACITY | LayerAnimationElement::BRIGHTNESS |
      LayerAnimationElement::GRAYSCALE;

  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreatePauseElement(properties, delta);

  TestLayerAnimationDelegate delegate;
  TestLayerAnimationDelegate copy = delegate;

  start_time += delta;
  element->set_requested_start_time(start_time);
  element->Start(&delegate, 1);

  // Pause should last for |delta|.
  base::TimeDelta element_duration;
  EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
  EXPECT_EQ(delta, element_duration);

  element->Progress(start_time + delta, &delegate);

  // Nothing should have changed.
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(),
                          copy.GetBoundsForAnimation());
  CheckApproximatelyEqual(delegate.GetTransformForAnimation(),
                          copy.GetTransformForAnimation());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(),
                  copy.GetOpacityForAnimation());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(),
                  copy.GetBrightnessForAnimation());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(),
                  copy.GetGrayscaleForAnimation());
}

// Check that the ClipRect element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, ClipRectElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Rect start, target, middle;
  start = target = middle = gfx::Rect(0, 0, 50, 50);

  start.set_x(-10);
  target.set_x(10);

  start.set_y(-20);
  target.set_y(20);

  start.set_width(70);
  target.set_width(30);
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateClipRectElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetClipRectFromAnimation(start,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    CheckApproximatelyEqual(start, delegate.GetClipRectForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    element->Progress(start_time + delta / 2, &delegate);
    CheckApproximatelyEqual(middle, delegate.GetClipRectForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    CheckApproximatelyEqual(target, delegate.GetClipRectForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target, target_value.clip_rect);
}

// Check that the RoundedCorners element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, RoundedCornersElement) {
  TestLayerAnimationDelegate delegate;
  gfx::RoundedCornersF start(1.0f, 2.0f, 3.0f, 4.0f);
  gfx::RoundedCornersF target(11.0f, 12.0f, 13.0f, 14.0f);
  gfx::RoundedCornersF middle(6.0f, 7.0f, 8.0f, 9.0f);

  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateRoundedCornersElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetRoundedCornersFromAnimation(
        start, PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    CheckApproximatelyEqual(start, delegate.GetRoundedCornersForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    element->Progress(start_time + delta / 2, &delegate);
    CheckApproximatelyEqual(middle, delegate.GetRoundedCornersForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    CheckApproximatelyEqual(target, delegate.GetRoundedCornersForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  CheckApproximatelyEqual(target, target_value.rounded_corners);
}

// Check that the GradientMask element progresses the delegate as expected and
// that the element can be reused after it completes.
TEST(LayerAnimationElementTest, GradientMaskElement) {
  TestLayerAnimationDelegate delegate;
  gfx::LinearGradient start(45);
  start.AddStep(0, 0);
  gfx::LinearGradient target(135);
  target.AddStep(.5, 255);
  gfx::LinearGradient middle(90);
  middle.AddStep(.25, 127);

  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);

  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateGradientMaskElement(target, delta);

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    element->set_requested_start_time(start_time);
    delegate.SetGradientMaskFromAnimation(
        start, PropertyChangeReason::NOT_FROM_ANIMATION);
    element->Start(&delegate, 1);
    element->Progress(start_time, &delegate);
    EXPECT_EQ(start, delegate.GetGradientMaskForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
    element->Progress(start_time + delta / 2, &delegate);
    EXPECT_EQ(middle, delegate.GetGradientMaskForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);

    base::TimeDelta element_duration;
    EXPECT_TRUE(element->IsFinished(start_time + delta, &element_duration));
    EXPECT_EQ(delta, element_duration);

    element->Progress(start_time + delta, &delegate);
    EXPECT_EQ(target, delegate.GetGradientMaskForAnimation());
    delegate.ExpectLastPropertyChangeReason(
        PropertyChangeReason::FROM_ANIMATION);
  }

  LayerAnimationElement::TargetValue target_value(&delegate);
  element->GetTargetValue(&target_value);
  EXPECT_EQ(target, target_value.gradient_mask);
}

// Check that a threaded opacity element updates the delegate as expected when
// aborted.
TEST(LayerAnimationElementTest, AbortOpacityElement) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float target = 1.0;
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(target, delta);

  // Choose a non-linear Tween type.
  gfx::Tween::Type tween_type = gfx::Tween::EASE_IN;
  element->set_tween_type(tween_type);

  delegate.SetOpacityFromAnimation(start,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.ExpectLastPropertyChangeReason(
      PropertyChangeReason::NOT_FROM_ANIMATION);

  // Aborting the element before it has started should not update the delegate.
  element->Abort(&delegate);
  EXPECT_FLOAT_EQ(start, delegate.GetOpacityForAnimation());
  delegate.ExpectLastPropertyChangeReasonIsUnset();

  start_time += delta;
  element->set_requested_start_time(start_time);
  element->Start(&delegate, 1);
  element->Progress(start_time, &delegate);
  effective_start_time = start_time + delta;
  element->set_effective_start_time(effective_start_time);
  element->Progress(effective_start_time, &delegate);
  element->Progress(effective_start_time + delta/2, &delegate);

  // Since the element has started, it should update the delegate when
  // aborted.
  element->Abort(&delegate);
  EXPECT_FLOAT_EQ(gfx::Tween::CalculateValue(tween_type, 0.5),
                  delegate.GetOpacityForAnimation());
  delegate.ExpectLastPropertyChangeReason(PropertyChangeReason::FROM_ANIMATION);
}

// Check that a threaded transform element updates the delegate as expected when
// aborted.
TEST(LayerAnimationElementTest, AbortTransformElement) {
  TestLayerAnimationDelegate delegate;
  gfx::Transform start_transform, target_transform;
  start_transform.Rotate(-30.0);
  target_transform.Rotate(30.0);
  base::TimeTicks start_time;
  base::TimeTicks effective_start_time;
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateTransformElement(target_transform, delta);

  // Choose a non-linear Tween type.
  gfx::Tween::Type tween_type = gfx::Tween::EASE_IN;
  element->set_tween_type(tween_type);

  delegate.SetTransformFromAnimation(start_transform,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.ExpectLastPropertyChangeReason(
      PropertyChangeReason::NOT_FROM_ANIMATION);

  // Aborting the element before it has started should not update the delegate.
  element->Abort(&delegate);
  CheckApproximatelyEqual(start_transform, delegate.GetTransformForAnimation());
  delegate.ExpectLastPropertyChangeReasonIsUnset();

  start_time += delta;
  element->set_requested_start_time(start_time);
  element->Start(&delegate, 1);
  element->Progress(start_time, &delegate);
  effective_start_time = start_time + delta;
  element->set_effective_start_time(effective_start_time);
  element->Progress(effective_start_time, &delegate);
  element->Progress(effective_start_time + delta/2, &delegate);

  // Since the element has started, it should update the delegate when
  // aborted.
  element->Abort(&delegate);
  target_transform.Blend(start_transform,
                         gfx::Tween::CalculateValue(tween_type, 0.5));
  CheckApproximatelyEqual(target_transform,
                          delegate.GetTransformForAnimation());
  delegate.ExpectLastPropertyChangeReason(PropertyChangeReason::FROM_ANIMATION);
}

// Check that an opacity element is not threaded if the start and target values
// are the same.
TEST(LayerAnimationElementTest, OpacityElementIsThreaded) {
  TestLayerAnimationDelegate delegate;
  float start = 0.0;
  float target = 1.0;
  delegate.SetOpacityFromAnimation(start,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(target, delta);
  EXPECT_TRUE(element->IsThreaded(&delegate));
  element->ProgressToEnd(&delegate);
  EXPECT_FLOAT_EQ(target, delegate.GetOpacityForAnimation());
  delegate.ExpectLastPropertyChangeReason(PropertyChangeReason::FROM_ANIMATION);

  start = 1.0;
  delegate.SetOpacityFromAnimation(start,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  element = LayerAnimationElement::CreateOpacityElement(target, delta);
  EXPECT_FALSE(element->IsThreaded(&delegate));
  element->ProgressToEnd(&delegate);
  EXPECT_FLOAT_EQ(target, delegate.GetOpacityForAnimation());
  delegate.ExpectLastPropertyChangeReason(PropertyChangeReason::FROM_ANIMATION);
}

TEST(LayerAnimationElementTest, ToString) {
  float target = 1.0;
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(target, delta);
  element->set_animation_group_id(42);
  // TODO(wkorman): Test varying last_progressed_fraction.
  EXPECT_EQ(
      base::StringPrintf("LayerAnimationElement{name=ThreadedOpacityTransition,"
                         " id=%d, group=42, "
                         "last_progressed_fraction=0.00}",
                         element->keyframe_model_id()),
      element->ToString());
}

} // namespace

} // namespace ui

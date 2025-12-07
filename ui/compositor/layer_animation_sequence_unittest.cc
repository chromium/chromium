// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_sequence.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/compositor/test/test_layer_animation_observer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

namespace {

// Check that the sequence behaves sanely when it contains no elements.
TEST(LayerAnimationSequenceTest, NoElement) {
  LayerAnimationSequence sequence;
  base::TimeTicks start_time;
  start_time += base::Seconds(1);
  sequence.set_start_time(start_time);
  EXPECT_TRUE(sequence.IsFinished(start_time));
  EXPECT_EQ(static_cast<LayerAnimationElement::AnimatableProperties>(
                LayerAnimationElement::UNKNOWN),
            sequence.properties());
  EXPECT_FALSE(sequence.HasConflictingProperty(LayerAnimationElement::UNKNOWN));
}

// Check that the sequences progresses the delegate as expected when it contains
// a single non-threaded element.
TEST(LayerAnimationSequenceTest, SingleElement) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start = 0.0f;
  float middle = 0.5f;
  float target = 1.0f;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    sequence.set_start_time(start_time);
    delegate.SetBrightnessFromAnimation(
        start, PropertyChangeReason::NOT_FROM_ANIMATION);
    sequence.Start(&delegate);
    sequence.Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetBrightnessForAnimation());
    sequence.Progress(start_time + base::Milliseconds(500), &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetBrightnessForAnimation());
    EXPECT_TRUE(sequence.IsFinished(start_time + delta));
    sequence.Progress(start_time + base::Milliseconds(1000), &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetBrightnessForAnimation());
  }

  EXPECT_EQ(static_cast<LayerAnimationElement::AnimatableProperties>(
                LayerAnimationElement::BRIGHTNESS),
            sequence.properties());
}

// Check that the sequences progresses the delegate as expected when it contains
// a single threaded element.
TEST(LayerAnimationSequenceTest, SingleThreadedElement) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start = 0.0f;
  float middle = 0.5f;
  float target = 1.0f;
  base::TimeTicks start_time;
  base::TimeTicks effective_start;
  base::TimeDelta delta = base::Seconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateOpacityElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    int starting_group_id = 1;
    sequence.set_animation_group_id(starting_group_id);
    start_time = effective_start + delta;
    sequence.set_start_time(start_time);
    delegate.SetOpacityFromAnimation(start,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
    sequence.Start(&delegate);
    sequence.Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, sequence.last_progressed_fraction());
    effective_start = start_time + delta;
    sequence.OnThreadedAnimationStarted(effective_start,
                                        cc::TargetProperty::OPACITY,
                                        sequence.animation_group_id());
    sequence.Progress(effective_start + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, sequence.last_progressed_fraction());
    EXPECT_TRUE(sequence.IsFinished(effective_start + delta));
    sequence.Progress(effective_start + delta, &delegate);
    EXPECT_FLOAT_EQ(target, sequence.last_progressed_fraction());
    EXPECT_FLOAT_EQ(target, delegate.GetOpacityForAnimation());
  }

  EXPECT_EQ(static_cast<LayerAnimationElement::AnimatableProperties>(
                LayerAnimationElement::OPACITY),
            sequence.properties());
}

// Check that the sequences progresses the delegate as expected when it contains
// multiple elements. Note, see the layer animator tests for repeating
// sequences.
TEST(LayerAnimationSequenceTest, MultipleElement) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start_opacity = 0.0f;
  float target_opacity = 1.0f;
  base::TimeTicks start_time;
  base::TimeTicks opacity_effective_start;
  base::TimeTicks transform_effective_start;
  base::TimeDelta delta = base::Seconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateOpacityElement(target_opacity, delta));

  // Pause bounds for a second.
  sequence.AddElement(LayerAnimationElement::CreatePauseElement(
      LayerAnimationElement::BOUNDS, delta));

  gfx::Transform start_transform, target_transform;
  start_transform.Rotate(-30.0);
  target_transform.Rotate(30.0);

  sequence.AddElement(
      LayerAnimationElement::CreateTransformElement(target_transform, delta));

  for (int i = 0; i < 2; ++i) {
    int starting_group_id = 1;
    sequence.set_animation_group_id(starting_group_id);
    start_time = opacity_effective_start + 4 * delta;
    sequence.set_start_time(start_time);
    delegate.SetOpacityFromAnimation(start_opacity,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
    delegate.SetTransformFromAnimation(
        start_transform, PropertyChangeReason::NOT_FROM_ANIMATION);

    sequence.Start(&delegate);
    sequence.Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(0.0, sequence.last_progressed_fraction());
    opacity_effective_start = start_time + delta;
    EXPECT_EQ(starting_group_id, sequence.animation_group_id());
    sequence.OnThreadedAnimationStarted(opacity_effective_start,
                                        cc::TargetProperty::OPACITY,
                                        sequence.animation_group_id());
    sequence.Progress(opacity_effective_start + delta/2, &delegate);
    EXPECT_FLOAT_EQ(0.5, sequence.last_progressed_fraction());
    sequence.Progress(opacity_effective_start + delta, &delegate);
    EXPECT_FLOAT_EQ(target_opacity, delegate.GetOpacityForAnimation());

    // Now at the start of the pause.
    EXPECT_FLOAT_EQ(0.0, sequence.last_progressed_fraction());
    TestLayerAnimationDelegate copy = delegate;

    // In the middle of the pause -- nothing should have changed.
    sequence.Progress(opacity_effective_start + delta + delta/2,
                      &delegate);
    CheckApproximatelyEqual(delegate.GetBoundsForAnimation(),
                            copy.GetBoundsForAnimation());
    CheckApproximatelyEqual(delegate.GetTransformForAnimation(),
                            copy.GetTransformForAnimation());
    EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(),
                    copy.GetOpacityForAnimation());

    sequence.Progress(opacity_effective_start + 2 * delta, &delegate);
    CheckApproximatelyEqual(start_transform,
                            delegate.GetTransformForAnimation());
    EXPECT_FLOAT_EQ(0.0, sequence.last_progressed_fraction());
    transform_effective_start = opacity_effective_start + 3 * delta;
    EXPECT_NE(starting_group_id, sequence.animation_group_id());
    sequence.OnThreadedAnimationStarted(transform_effective_start,
                                        cc::TargetProperty::TRANSFORM,
                                        sequence.animation_group_id());
    sequence.Progress(transform_effective_start + delta/2, &delegate);
    EXPECT_FLOAT_EQ(0.5, sequence.last_progressed_fraction());
    EXPECT_TRUE(sequence.IsFinished(transform_effective_start + delta));
    sequence.Progress(transform_effective_start + delta, &delegate);
    CheckApproximatelyEqual(target_transform,
                            delegate.GetTransformForAnimation());
  }

  EXPECT_EQ(
      static_cast<LayerAnimationElement::AnimatableProperties>(
          LayerAnimationElement::OPACITY | LayerAnimationElement::TRANSFORM |
          LayerAnimationElement::BOUNDS),
      sequence.properties());
}

// Check that a sequence can still be aborted if it has repeated many times.
TEST(LayerAnimationSequenceTest, AbortingRepeatingSequence) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start_brightness = 0.0f;
  float target_brightness = 1.0f;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(target_brightness, delta));

  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(start_brightness, delta));

  sequence.set_is_repeating(true);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  start_time += delta;
  sequence.set_start_time(start_time);
  sequence.Start(&delegate);
  sequence.Progress(start_time + base::Milliseconds(101000), &delegate);
  EXPECT_FLOAT_EQ(target_brightness, delegate.GetBrightnessForAnimation());
  sequence.Abort(&delegate);

  // Should be able to reuse the sequence after aborting.
  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
  start_time += base::Milliseconds(101000);
  sequence.set_start_time(start_time);
  sequence.Progress(start_time + base::Milliseconds(100000), &delegate);
  EXPECT_FLOAT_EQ(start_brightness, delegate.GetBrightnessForAnimation());
}

// Check that a sequence can be 'fast-forwarded' to the end and the target set.
// Also check that this has no effect if the sequence is repeating.
TEST(LayerAnimationSequenceTest, SetTarget) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start_opacity = 0.0f;
  float target_opacity = 1.0f;
  base::TimeDelta delta = base::Seconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateOpacityElement(target_opacity, delta));

  LayerAnimationElement::TargetValue target_value(&delegate);
  target_value.opacity = start_opacity;
  sequence.GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target_opacity, target_value.opacity);

  sequence.set_is_repeating(true);
  target_value.opacity = start_opacity;
  sequence.GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(start_opacity, target_value.opacity);
}

TEST(LayerAnimationSequenceTest, AddObserver) {
  base::TimeTicks start_time;
  base::TimeDelta delta = base::Seconds(1);
  LayerAnimationSequence sequence;
  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta));
  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    sequence.set_start_time(start_time);
    TestLayerAnimationObserver observer;
    TestLayerAnimationDelegate delegate;
    sequence.AddObserver(&observer);
    EXPECT_TRUE(!observer.last_ended_sequence());
    sequence.Progress(start_time + delta, &delegate);
    EXPECT_EQ(observer.last_ended_sequence(), &sequence);
    sequence.RemoveObserver(&observer);
  }
}

TEST(LayerAnimationSequenceTest, ToString) {
  base::TimeDelta delta = base::Seconds(1);
  LayerAnimationSequence sequence;
  EXPECT_EQ(
      "LayerAnimationSequence{size=0, properties=, elements=[], "
      "is_repeating=0, "
      "group_id=0}",
      sequence.ToString());

  std::unique_ptr<LayerAnimationElement> brightness =
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta);
  int brightness_id = brightness->keyframe_model_id();
  sequence.AddElement(std::move(brightness));
  EXPECT_EQ(
      base::StringPrintf(
          "LayerAnimationSequence{size=1, properties=BRIGHTNESS, "
          "elements=[LayerAnimationElement{name=BrightnessTransition, id=%d, "
          "group=0, last_progressed_fraction=0.00}], "
          "is_repeating=0, group_id=0}",
          brightness_id),
      sequence.ToString());

  std::unique_ptr<LayerAnimationElement> opacity =
      LayerAnimationElement::CreateOpacityElement(1.0f, delta);
  int opacity_id = opacity->keyframe_model_id();
  sequence.AddElement(std::move(opacity));
  sequence.set_is_repeating(true);
  sequence.set_animation_group_id(1973);
  EXPECT_EQ(
      base::StringPrintf(
          "LayerAnimationSequence{size=2, properties=OPACITY|BRIGHTNESS, "
          "elements=[LayerAnimationElement{name=BrightnessTransition, id=%d, "
          "group=0, last_progressed_fraction=0.00}, "
          "LayerAnimationElement{name=ThreadedOpacityTransition, id=%d, "
          "group=0, "
          "last_progressed_fraction=0.00}], is_repeating=1, "
          "group_id=1973}",
          brightness_id, opacity_id),
      sequence.ToString());
}

// TODO(b/352744702): Remove this test or convert to DEATH test after
// https://crrev.com/c/5713998 has rolled out and any cases like this have been
// removed.
#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()
// Check that the sequence doesn't get stuck in an infinite loop when it's
// repeating and has a total duration of zero.
TEST(LayerAnimationSequenceTest, RepeatingWithZeroDuration) {
  const float final_brightness = 1.f;
  const int num_steps_to_test = 3;
  const base::TimeDelta animation_step_size = base::Seconds(1);

  LayerAnimationSequence sequence;
  sequence.set_is_repeating(true);
  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(0.5f, base::TimeDelta()));
  sequence.AddElement(LayerAnimationElement::CreateBrightnessElement(
      final_brightness, base::TimeDelta()));
  TestLayerAnimationDelegate delegate;
  delegate.SetBrightnessFromAnimation(0.f,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  base::TimeTicks now = base::TimeTicks::Now();
  sequence.set_start_time(now);
  sequence.Start(&delegate);
  for (int i = 0; i < num_steps_to_test; ++i, now += animation_step_size) {
    sequence.Progress(now, &delegate);
    EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), final_brightness);
  }
}
#endif

} // namespace

} // namespace ui

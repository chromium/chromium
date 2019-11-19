// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/compositor/test/test_layer_animation_observer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

// Converts |color| to a string. Each component of the color is separated by a
// space and the order if A R G B.
std::string ColorToString(SkColor color) {
  return base::StringPrintf("%d %d %d %d", SkColorGetA(color),
                            SkColorGetR(color), SkColorGetG(color),
                            SkColorGetB(color));
}

// Creates vector with two LayerAnimationSequences, based on |first| and
// |second| layer animation elements.
std::vector<LayerAnimationSequence*> CreateMultiSequence(
    std::unique_ptr<LayerAnimationElement> first,
    std::unique_ptr<LayerAnimationElement> second) {
  LayerAnimationSequence* first_sequence = new LayerAnimationSequence();
  first_sequence->AddElement(std::move(first));
  LayerAnimationSequence* second_sequence = new LayerAnimationSequence();
  second_sequence->AddElement(std::move(second));

  std::vector<ui::LayerAnimationSequence*> animations;
  animations.push_back(first_sequence);
  animations.push_back(second_sequence);
  return animations;
}

// Creates a default animator with timers disabled for test. |delegate| and
// |observer| are attached if non-null.
LayerAnimator* CreateDefaultTestAnimator(LayerAnimationDelegate* delegate,
                                         LayerAnimationObserver* observer) {
  LayerAnimator* animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);
  if (delegate)
    animator->SetDelegate(delegate);
  if (observer)
    animator->AddObserver(observer);
  return animator;
}

// Creates a default animator with timers disabled for test. |delegate| is
// attached if non-null.
LayerAnimator* CreateDefaultTestAnimator(LayerAnimationDelegate* delegate) {
  return CreateDefaultTestAnimator(delegate, nullptr);
}

// Creates a default animator with timers disabled for test.
LayerAnimator* CreateDefaultTestAnimator() {
  return CreateDefaultTestAnimator(nullptr, nullptr);
}

// Creates an implicit animator with timers disabled for test. |delegate| and
// |observer| are attached if non-null.
LayerAnimator* CreateImplicitTestAnimator(LayerAnimationDelegate* delegate,
                                          LayerAnimationObserver* observer) {
  LayerAnimator* animator(LayerAnimator::CreateImplicitAnimator());
  animator->set_disable_timer_for_test(true);
  if (delegate)
    animator->SetDelegate(delegate);
  if (observer)
    animator->AddObserver(observer);
  return animator;
}

// Creates an implicit animator with timers disabled for test. |delegate| is
// attached if non-null.
LayerAnimator* CreateImplicitTestAnimator(LayerAnimationDelegate* delegate) {
  return CreateImplicitTestAnimator(delegate, nullptr);
}

// Creates an implicit animator with timers disabled for test.
LayerAnimator* CreateImplicitTestAnimator() {
  return CreateImplicitTestAnimator(nullptr, nullptr);
}

class TestImplicitAnimationObserver : public ImplicitAnimationObserver {
 public:
  explicit TestImplicitAnimationObserver(bool notify_when_animator_destructed)
    : animations_completed_(false),
      notify_when_animator_destructed_(notify_when_animator_destructed) {
  }

  bool animations_completed() const { return animations_completed_; }
  void set_animations_completed(bool completed) {
    animations_completed_ = completed;
  }

  bool WasAnimationAbortedForProperty(
      LayerAnimationElement::AnimatableProperty property) const {
    return ImplicitAnimationObserver::WasAnimationAbortedForProperty(property);
  }

  bool WasAnimationCompletedForProperty(
      LayerAnimationElement::AnimatableProperty property) const {
    return ImplicitAnimationObserver::WasAnimationCompletedForProperty(
        property);
  }

 private:
  // ImplicitAnimationObserver implementation
  void OnImplicitAnimationsCompleted() override {
    animations_completed_ = true;
  }

  bool RequiresNotificationWhenAnimatorDestroyed() const override {
    return notify_when_animator_destructed_;
  }

  bool animations_completed_;
  bool notify_when_animator_destructed_;

  DISALLOW_COPY_AND_ASSIGN(TestImplicitAnimationObserver);
};

// When notified that an animation has ended, stops all other animations.
class DeletingLayerAnimationObserver : public LayerAnimationObserver {
 public:
  explicit DeletingLayerAnimationObserver(LayerAnimator* animator)
      : animator_(animator) {}

  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override {
    animator_->StopAnimating();
  }

  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {
    animator_->StopAnimating();
  }

  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override {}

 private:
  LayerAnimator* animator_;

  DISALLOW_COPY_AND_ASSIGN(DeletingLayerAnimationObserver);
};

// When notified that an animation has started, aborts all animations.
class AbortAnimationsOnStartedLayerAnimationObserver
    : public LayerAnimationObserver {
 public:
  explicit AbortAnimationsOnStartedLayerAnimationObserver(
      LayerAnimator* animator)
      : animator_(animator) {}

  void OnLayerAnimationStarted(LayerAnimationSequence* sequence) override {
    animator_->AbortAllAnimations();
  }

  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override {}

 private:
  LayerAnimator* animator_;

  DISALLOW_COPY_AND_ASSIGN(AbortAnimationsOnStartedLayerAnimationObserver);
};

class LayerAnimatorDestructionObserver {
 public:
  LayerAnimatorDestructionObserver() : animator_deleted_(false) {}
  virtual ~LayerAnimatorDestructionObserver() {}

  void NotifyAnimatorDeleted() {
    animator_deleted_ = true;
  }

  bool IsAnimatorDeleted() {
    return animator_deleted_;
  }

 private:
  bool animator_deleted_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimatorDestructionObserver);
};

class TestLayerAnimator : public LayerAnimator {
 public:
  TestLayerAnimator() : LayerAnimator(base::TimeDelta::FromSeconds(0)),
      destruction_observer_(NULL) {}

  void SetDestructionObserver(
      LayerAnimatorDestructionObserver* observer) {
    destruction_observer_ = observer;
  }

 protected:
  ~TestLayerAnimator() override {
    if (destruction_observer_) {
      destruction_observer_->NotifyAnimatorDeleted();
    }
  }

  void ProgressAnimation(LayerAnimationSequence* sequence,
                         base::TimeTicks now) override {
    EXPECT_TRUE(HasAnimation(sequence));
    LayerAnimator::ProgressAnimation(sequence, now);
  }

 private:
  LayerAnimatorDestructionObserver* destruction_observer_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimator);
};

// The test layer animation sequence updates a live instances count when it is
// created and destroyed.
class TestLayerAnimationSequence : public LayerAnimationSequence {
 public:
  TestLayerAnimationSequence(std::unique_ptr<LayerAnimationElement> element,
                             int* num_live_instances)
      : LayerAnimationSequence(std::move(element)),
        num_live_instances_(num_live_instances) {
    (*num_live_instances_)++;
  }

  ~TestLayerAnimationSequence() override { (*num_live_instances_)--; }

 private:
  int* num_live_instances_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimationSequence);
};

}  // namespace

namespace test {

// Tracks the number of calls to the LayerAnimationObserver overrides and fails
// CHECKS if the counts are inconsistent, e.g. the number of started sequences
// is greater than the number of attached sequences.
class CountCheckingLayerAnimationObserver : public LayerAnimationObserver {
 public:
  explicit CountCheckingLayerAnimationObserver(LayerAnimationObserver* observer)
      : observer_(observer) {}

  void OnLayerAnimationStarted(LayerAnimationSequence* sequence) override {
    ++started_count_;
    EXPECT_LE(started_count_, attached_sequence_count_);
    EXPECT_LE(started_count_, sequences_scheduled_);
    if (observer_)
      observer_->OnLayerAnimationStarted(sequence);
  }

  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override {
    ++successful_count_;
    EXPECT_LE(GetNumSequencesCompleted(), sequences_scheduled_)
        << " aborted_count_=" << aborted_count_
        << " successful_count_=" << successful_count_;
    EXPECT_LE(GetNumSequencesCompleted(), attached_sequence_count_)
        << " aborted_count_=" << aborted_count_
        << " successful_count_=" << successful_count_;
    if (observer_)
      observer_->OnLayerAnimationEnded(sequence);
  }

  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {
    ++aborted_count_;
    EXPECT_LE(GetNumSequencesCompleted(), sequences_scheduled_)
        << " aborted_count_=" << aborted_count_
        << " successful_count_=" << successful_count_;
    EXPECT_LE(GetNumSequencesCompleted(), attached_sequence_count_)
        << " aborted_count_=" << aborted_count_
        << " successful_count_=" << successful_count_;
    if (observer_)
      observer_->OnLayerAnimationAborted(sequence);
  }

  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override {
    ++sequences_scheduled_;
    EXPECT_LE(sequences_scheduled_, attached_sequence_count_);
    if (observer_)
      observer_->OnLayerAnimationScheduled(sequence);
  }

  int GetNumSequencesCompleted() { return aborted_count_ + successful_count_; }

 protected:
  void OnAttachedToSequence(LayerAnimationSequence* sequence) override {
    ++attached_sequence_count_;
    if (observer_)
      observer_->OnAttachedToSequence(sequence);
  }

  void OnDetachedFromSequence(LayerAnimationSequence* sequence) override {
    ++detached_sequence_count_;
    EXPECT_LE(detached_sequence_count_, attached_sequence_count_);
    EXPECT_LE(detached_sequence_count_, GetNumSequencesCompleted())
        << " aborted_count_=" << aborted_count_
        << " successful_count_=" << successful_count_;
    if (observer_)
      observer_->OnDetachedFromSequence(sequence);
  }

 private:
  // Observer to which LayerAnimationObserver calls are delgated.
  LayerAnimationObserver* observer_;

  // The total number of animation sequences that have been attached.
  int attached_sequence_count_ = 0;

  // The total number of animation sequences that have been detached.
  int detached_sequence_count_ = 0;

  // The total number of animation sequences that have been scheduled.
  int sequences_scheduled_ = 0;

  // The number of animation sequences that have been started.
  int started_count_ = 0;

  // The number of animation sequences that were aborted.
  int aborted_count_ = 0;

  // The number of animation sequences that completed successfully.
  int successful_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CountCheckingLayerAnimationObserver);
};

}  // namespace test

// Checks that setting a property on an implicit animator causes an animation to
// happen.
TEST(LayerAnimatorTest, ImplicitAnimation) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator(&delegate));
  base::TimeTicks now = base::TimeTicks::Now();
  animator->SetBrightness(0.5);
  EXPECT_TRUE(animator->is_animating());
  animator->Step(now + base::TimeDelta::FromSeconds(1));
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), 0.5);
  delegate.ExpectLastPropertyChangeReason(PropertyChangeReason::FROM_ANIMATION);
}

// Checks that if the animator is a default animator, that implicit animations
// are not started.
TEST(LayerAnimatorTest, NoImplicitAnimation) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  animator->SetBrightness(0.5);
  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), 0.5);
  delegate.ExpectLastPropertyChangeReason(
      PropertyChangeReason::NOT_FROM_ANIMATION);
}

// Checks that StopAnimatingProperty stops animation for that property, and also
// skips the stopped animation to the end.
TEST(LayerAnimatorTest, StopAnimatingProperty) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator(&delegate));
  double target_opacity(0.5);
  gfx::Rect target_bounds(0, 0, 50, 50);
  animator->SetOpacity(target_opacity);
  animator->SetBounds(target_bounds);
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), 0.5);
  animator->StopAnimatingProperty(LayerAnimationElement::BOUNDS);
  EXPECT_FALSE(animator->is_animating());
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Checks that multiple running animations for separate properties can be
// stopped simultaneously and that all animations are advanced to their target
// values.
TEST(LayerAnimatorTest, StopAnimating) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator(&delegate));
  double target_opacity(0.5);
  gfx::Rect target_bounds(0, 0, 50, 50);
  animator->SetOpacity(target_opacity);
  animator->SetBounds(target_bounds);
  EXPECT_TRUE(animator->is_animating());
  animator->StopAnimating();
  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), 0.5);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Checks that multiple running animations for separate properties can be
// stopped simultaneously and that aborted animations are NOT advanced to their
// target values.
TEST(LayerAnimatorTest, AbortAllAnimations) {
  TestLayerAnimationDelegate delegate;
  double initial_opacity(1.0);
  gfx::Rect initial_bounds(0, 0, 10, 10);
  delegate.SetOpacityFromAnimation(initial_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBoundsFromAnimation(initial_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator(&delegate));
  double target_opacity(0.5);
  gfx::Rect target_bounds(0, 0, 50, 50);
  animator->SetOpacity(target_opacity);
  animator->SetBounds(target_bounds);
  EXPECT_TRUE(animator->is_animating());
  animator->AbortAllAnimations();
  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(initial_opacity, delegate.GetOpacityForAnimation());
  CheckApproximatelyEqual(initial_bounds, delegate.GetBoundsForAnimation());
}

// Schedule a non-threaded animation that can run immediately. This is the
// trivial case and should result in the animation being started immediately.
TEST(LayerAnimatorTest, ScheduleAnimationThatCanRunImmediately) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double middle_brightness(0.5);
  double target_brightness(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);
}

// Schedule a threaded animation that can run immediately.
TEST(LayerAnimatorTest, ScheduleThreadedAnimationThatCanRunImmediately) {
  double epsilon = 0.00001;
  TestLayerAnimationDelegate delegate;
  LayerAnimatorTestController test_controller(
      CreateDefaultTestAnimator(&delegate));
  LayerAnimator* animator = test_controller.animator();

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  test_controller.animator()->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  base::TimeTicks start_time = test_controller.animator()->last_step_time();
  base::TimeTicks effective_start = start_time + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(effective_start + delta / 2);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_NEAR(
      0.5,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)->
          last_progressed_fraction(),
      epsilon);

  animator->Step(effective_start + delta);

  EXPECT_FALSE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
}

// Schedule two non-threaded animations on separate properties. Both animations
// should start immediately and should progress in lock step.
TEST(LayerAnimatorTest, ScheduleTwoAnimationsThatCanRunImmediately) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double middle_brightness(0.5);
  double target_brightness(1.0);

  gfx::Rect start_bounds, target_bounds, middle_bounds;
  start_bounds = target_bounds = middle_bounds = gfx::Rect(0, 0, 50, 50);
  start_bounds.set_x(-90);
  target_bounds.set_x(90);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), middle_bounds);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Schedule a threaded and a non-threaded animation on separate properties. Both
// animations should progress in lock step.
TEST(LayerAnimatorTest, ScheduleThreadedAndNonThreadedAnimations) {
  double epsilon = 0.00001;
  TestLayerAnimationDelegate delegate;
  LayerAnimatorTestController test_controller(
      CreateDefaultTestAnimator(&delegate));
  LayerAnimator* animator = test_controller.animator();

  double start_opacity(0.0);
  double target_opacity(1.0);

  gfx::Rect start_bounds, target_bounds, middle_bounds;
  start_bounds = target_bounds = middle_bounds = gfx::Rect(0, 0, 50, 50);
  start_bounds.set_x(-90);
  target_bounds.set_x(90);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  std::vector<LayerAnimationSequence*> animations;
  animations.push_back(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  animations.push_back(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  test_controller.animator()->ScheduleTogether(animations);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  base::TimeTicks start_time = test_controller.animator()->last_step_time();
  base::TimeTicks effective_start = start_time + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(effective_start + delta / 2);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_NEAR(
      0.5,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)->
          last_progressed_fraction(),
      epsilon);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), middle_bounds);

  animator->Step(effective_start + delta);

  EXPECT_FALSE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Schedule two animations on the same property. In this case, the two
// animations should run one after another.
TEST(LayerAnimatorTest, ScheduleTwoAnimationsOnSameProperty) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double middle_brightness(0.5);
  double target_brightness(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

// Schedule [{g}, {g,b}, {b}] and ensure that {b} doesn't run right away. That
// is, ensure that all animations targetting a particular property are run in
// order.
TEST(LayerAnimatorTest, ScheduleBlockedAnimation) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_grayscale(0.0);
  double middle_grayscale(0.5);
  double target_grayscale(1.0);

  gfx::Rect start_bounds, target_bounds, middle_bounds;
  start_bounds = target_bounds = middle_bounds = gfx::Rect(0, 0, 50, 50);
  start_bounds.set_x(-90);
  target_bounds.set_x(90);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetGrayscaleFromAnimation(start_grayscale,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateGrayscaleElement(target_grayscale,
                                                        delta)));

  std::unique_ptr<LayerAnimationSequence> bounds_and_grayscale(
      new LayerAnimationSequence(LayerAnimationElement::CreateGrayscaleElement(
          start_grayscale, delta)));

  bounds_and_grayscale->AddElement(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta));

  animator->ScheduleAnimation(bounds_and_grayscale.release());

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBoundsElement(start_bounds, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), middle_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), target_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(3000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(4000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);
}

// Schedule {g} and then schedule {g} and {b} together. In this case, since
// ScheduleTogether is being used, the bounds animation should not start until
// the second grayscale animation starts.
TEST(LayerAnimatorTest, ScheduleTogether) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_grayscale(0.0);
  double target_grayscale(1.0);

  gfx::Rect start_bounds, target_bounds;
  start_bounds = target_bounds = gfx::Rect(0, 0, 50, 50);
  start_bounds.set_x(-90);
  target_bounds.set_x(90);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetGrayscaleFromAnimation(start_grayscale,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateGrayscaleElement(target_grayscale,
                                                        delta)));

  std::vector<LayerAnimationSequence*> sequences;
  sequences.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateGrayscaleElement(start_grayscale, delta)));
  sequences.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  animator->ScheduleTogether(sequences);

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), target_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Start non-threaded animation (that can run immediately). This is the trivial
// case (see the trival case for ScheduleAnimation).
TEST(LayerAnimatorTest, StartAnimationThatCanRunImmediately) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double middle_brightness(0.5);
  double target_brightness(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);
}

// Start threaded animation (that can run immediately).
TEST(LayerAnimatorTest, StartThreadedAnimationThatCanRunImmediately) {
  double epsilon = 0.00001;
  TestLayerAnimationDelegate delegate;
  LayerAnimatorTestController test_controller(
      CreateDefaultTestAnimator(&delegate));
  LayerAnimator* animator = test_controller.animator();

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  test_controller.animator()->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  base::TimeTicks start_time = test_controller.animator()->last_step_time();
  base::TimeTicks effective_start = start_time + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(effective_start + delta / 2);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_NEAR(
      0.5,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)->
          last_progressed_fraction(),
      epsilon);

  animator->Step(effective_start + delta);
  EXPECT_FALSE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
}

// Preempt by immediately setting new target.
TEST(LayerAnimatorTest, PreemptBySettingNewTarget) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
}

// Preempt by animating to new target, with a non-threaded animation.
TEST(LayerAnimatorTest, PreemptByImmediatelyAnimatingToNewTarget) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double middle_brightness(0.5);
  double target_brightness(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(),
                  0.5 * (start_brightness + middle_brightness));

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

// Preempt by animating to new target, with a threaded animation.
TEST(LayerAnimatorTest, PreemptThreadedByImmediatelyAnimatingToNewTarget) {
  double epsilon = 0.00001;
  TestLayerAnimationDelegate delegate;
  LayerAnimatorTestController test_controller(
      CreateDefaultTestAnimator(&delegate));
  LayerAnimator* animator = test_controller.animator();

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  test_controller.animator()->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  test_controller.animator()->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  base::TimeTicks start_time = test_controller.animator()->last_step_time();
  base::TimeTicks effective_start = start_time + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(effective_start + delta / 2);

  test_controller.animator()->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_NEAR(delegate.GetOpacityForAnimation(), middle_opacity, epsilon);

  test_controller.animator()->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_TRUE(test_controller.animator()->is_animating());

  base::TimeTicks second_effective_start = effective_start + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      second_effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(second_effective_start + delta / 2);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_NEAR(
      0.5,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)->
          last_progressed_fraction(),
      epsilon);

  animator->Step(second_effective_start + delta);

  EXPECT_FALSE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
}

// Preempt by enqueuing the new animation.
TEST(LayerAnimatorTest, PreemptEnqueueNewAnimation) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double middle_brightness(0.5);
  double target_brightness(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                          delta)));

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  EXPECT_TRUE(animator->is_animating());

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

// Start an animation when there are sequences waiting in the queue. In this
// case, all pending and running animations should be finished, and the new
// animation started.
TEST(LayerAnimatorTest, PreemptyByReplacingQueuedAnimations) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double middle_brightness(0.5);
  double target_brightness(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(LayerAnimator::REPLACE_QUEUED_ANIMATIONS);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(middle_brightness,
                                                         delta)));

  // Queue should now have two animations. Starting a third should replace the
  // second.
  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

TEST(LayerAnimatorTest, StartTogetherSetsLastStepTime) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_grayscale(0.0);
  double target_grayscale(1.0);
  double start_brightness(0.1);
  double target_brightness(0.9);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetGrayscaleFromAnimation(start_grayscale,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  animator->set_last_step_time(base::TimeTicks());

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(target_grayscale,
                                                        delta),
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)
      ));

  // If last step time was not set correctly, the resulting delta should be
  // miniscule (fractions of a millisecond). If set correctly, then the delta
  // should be enormous. Arbitrarily choosing 1 minute as the threshold,
  // though a much smaller value would probably have sufficed.
  delta = base::TimeTicks::Now() - animator->last_step_time();
  EXPECT_GT(60.0, delta.InSecondsF());
}

//-------------------------------------------------------
// Preempt by immediately setting new target.
TEST(LayerAnimatorTest, MultiPreemptBySettingNewTarget) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_opacity(0.0);
  double target_opacity(1.0);
  double start_brightness(0.1);
  double target_brightness(0.9);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta),
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)
      ));

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta),
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)
      ));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

// Preempt by animating to new target.
TEST(LayerAnimatorTest, MultiPreemptByImmediatelyAnimatingToNewTarget) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_grayscale(0.0);
  double middle_grayscale(0.5);
  double target_grayscale(1.0);

  double start_brightness(0.1);
  double middle_brightness(0.2);
  double target_brightness(0.3);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetGrayscaleFromAnimation(start_grayscale,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(target_grayscale,
                                                        delta),
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)
      ));

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(start_grayscale, delta),
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), middle_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(start_grayscale, delta),
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(),
                  0.5 * (start_grayscale + middle_grayscale));
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(),
                  0.5 * (start_brightness + middle_brightness));

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

// Preempt a threaded animation by animating to new target.
TEST(LayerAnimatorTest, MultiPreemptThreadedByImmediatelyAnimatingToNewTarget) {
  double epsilon = 0.00001;
  TestLayerAnimationDelegate delegate;
  LayerAnimatorTestController test_controller(
      CreateDefaultTestAnimator(&delegate));
  LayerAnimator* animator = test_controller.animator();

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  double start_brightness(0.1);
  double middle_brightness(0.2);
  double target_brightness(0.3);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  test_controller.animator()->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  test_controller.animator()->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta),
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)
      ));

  base::TimeTicks start_time = test_controller.animator()->last_step_time();
  base::TimeTicks effective_start = start_time + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(effective_start + delta / 2);

  test_controller.animator()->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta),
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_NEAR(delegate.GetOpacityForAnimation(), middle_opacity, epsilon);
  EXPECT_NEAR(delegate.GetBrightnessForAnimation(), middle_brightness, epsilon);

  test_controller.animator()->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta),
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(test_controller.animator()->is_animating());

  base::TimeTicks second_effective_start = effective_start + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      second_effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(second_effective_start + delta / 2);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_NEAR(
      0.5,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)->
          last_progressed_fraction(),
      epsilon);
  EXPECT_NEAR(delegate.GetBrightnessForAnimation(),
              0.5 * (start_brightness + middle_brightness),
              epsilon);

  animator->Step(second_effective_start + delta);

  EXPECT_FALSE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

// Preempt by enqueuing the new animation.
TEST(LayerAnimatorTest, MultiPreemptEnqueueNewAnimation) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_grayscale(0.0);
  double middle_grayscale(0.5);
  double target_grayscale(1.0);

  double start_brightness(0.1);
  double middle_brightness(0.2);
  double target_brightness(0.3);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetGrayscaleFromAnimation(start_grayscale,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(target_grayscale,
                                                        delta),
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(start_grayscale, delta),
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), middle_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  EXPECT_TRUE(animator->is_animating());

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), target_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), middle_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}

// Start an animation when there are sequences waiting in the queue. In this
// case, all pending and running animations should be finished, and the new
// animation started.
TEST(LayerAnimatorTest, MultiPreemptByReplacingQueuedAnimations) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_grayscale(0.0);
  double middle_grayscale(0.5);
  double target_grayscale(1.0);

  double start_brightness(0.1);
  double middle_brightness(0.2);
  double target_brightness(0.3);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetGrayscaleFromAnimation(start_grayscale,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->set_preemption_strategy(LayerAnimator::REPLACE_QUEUED_ANIMATIONS);

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(target_grayscale,
                                                        delta),
          LayerAnimationElement::CreateBrightnessElement(target_brightness,
                                                         delta)));

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(middle_grayscale,
                                                        delta),
          LayerAnimationElement::CreateBrightnessElement(middle_brightness,
                                                         delta)));

  // Queue should now have two animations. Starting a third should replace the
  // second.
  animator->StartTogether(
      CreateMultiSequence(
          LayerAnimationElement::CreateGrayscaleElement(start_grayscale, delta),
          LayerAnimationElement::CreateBrightnessElement(start_brightness,
                                                         delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), middle_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), target_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), middle_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), middle_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetGrayscaleForAnimation(), start_grayscale);
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);
}
//-------------------------------------------------------
// Test that non-threaded cyclic sequences continue to animate.
TEST(LayerAnimatorTest, CyclicSequences) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_brightness(0.0);
  double target_brightness(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  std::unique_ptr<LayerAnimationSequence> sequence(
      new LayerAnimationSequence(LayerAnimationElement::CreateBrightnessElement(
          target_brightness, delta)));

  sequence->AddElement(
      LayerAnimationElement::CreateBrightnessElement(start_brightness, delta));

  sequence->set_is_cyclic(true);

  animator->StartAnimation(sequence.release());

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(3000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  // Skip ahead by a lot.
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000000000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), start_brightness);

  // Skip ahead by a lot.
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000001000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetBrightnessForAnimation(), target_brightness);

  animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);

  EXPECT_FALSE(animator->is_animating());
}

// Test that threaded cyclic sequences continue to animate.
TEST(LayerAnimatorTest, ThreadedCyclicSequences) {
  TestLayerAnimationDelegate delegate;
  LayerAnimatorTestController test_controller(
      CreateDefaultTestAnimator(&delegate));
  LayerAnimator* animator = test_controller.animator();

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  std::unique_ptr<LayerAnimationSequence> sequence(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  sequence->AddElement(
      LayerAnimationElement::CreateOpacityElement(start_opacity, delta));

  sequence->set_is_cyclic(true);

  test_controller.animator()->StartAnimation(sequence.release());

  base::TimeTicks start_time = test_controller.animator()->last_step_time();
  base::TimeTicks effective_start = start_time + delta;

  test_controller.animator()->OnThreadedAnimationStarted(
      effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(effective_start + delta);
  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  base::TimeTicks second_effective_start = effective_start + 2 * delta;
  test_controller.animator()->OnThreadedAnimationStarted(
      second_effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(second_effective_start + delta);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  base::TimeTicks third_effective_start = second_effective_start + 2 * delta;
  test_controller.animator()->OnThreadedAnimationStarted(
      third_effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  animator->Step(third_effective_start + delta);
  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  base::TimeTicks fourth_effective_start = third_effective_start + 2 * delta;
  test_controller.animator()->OnThreadedAnimationStarted(
      fourth_effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  // Skip ahead by a lot.
  animator->Step(fourth_effective_start + 1000 * delta);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  base::TimeTicks fifth_effective_start = fourth_effective_start + 1001 * delta;
  test_controller.animator()->OnThreadedAnimationStarted(
      fifth_effective_start, cc::TargetProperty::OPACITY,
      test_controller.GetRunningSequence(LayerAnimationElement::OPACITY)
          ->animation_group_id());

  // Skip ahead by a lot.
  animator->Step(fifth_effective_start + 999 * delta);

  EXPECT_TRUE(test_controller.animator()->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  test_controller.animator()->StopAnimatingProperty(
      LayerAnimationElement::OPACITY);

  EXPECT_FALSE(test_controller.animator()->is_animating());
}

TEST(LayerAnimatorTest, AddObserverExplicit) {
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(
      CreateDefaultTestAnimator(&delegate, &observer));
  observer.set_requires_notification_when_animator_destroyed(true);

  EXPECT_TRUE(!observer.last_ended_sequence());

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetBrightnessFromAnimation(0.0f,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta));

  animator->StartAnimation(sequence);

  EXPECT_EQ(observer.last_scheduled_sequence(), sequence);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ(observer.last_ended_sequence(), sequence);

  // |sequence| has been destroyed. Recreate it to test abort.
  sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta));

  animator->StartAnimation(sequence);

  animator.reset();

  EXPECT_EQ(observer.last_aborted_sequence(), sequence);
}

// Tests that an observer added to a scoped settings object is still notified
// when the object goes out of scope.
TEST(LayerAnimatorTest, ImplicitAnimationObservers) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  TestImplicitAnimationObserver observer(false);

  EXPECT_FALSE(observer.animations_completed());
  animator->SetBrightness(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer);
    animator->SetBrightness(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  base::TimeTicks start_time = animator->last_step_time();
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationCompletedForProperty(
      LayerAnimationElement::BRIGHTNESS));
  EXPECT_FLOAT_EQ(0.0f, delegate.GetBrightnessForAnimation());
}

// Tests that caching render surface added to a scoped settings object is still
// reset when the object goes out of scope.
TEST(LayerAnimatorTest, CacheRenderSurface) {
  ui::Layer layer;
  scoped_refptr<LayerAnimator> animator(layer.GetAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer(false);

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_FALSE(layer.cc_layer_for_testing()->cache_render_surface());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    settings.AddObserver(&observer);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationCompletedForProperty(
      LayerAnimationElement::OPACITY));
  EXPECT_FLOAT_EQ(0.0f, layer.opacity());
  EXPECT_FALSE(layer.cc_layer_for_testing()->cache_render_surface());
}

// Tests that caching render surface added to a scoped settings object will not
// crash when the layer was destroyed.
TEST(LayerAnimatorTest, CacheRenderSurfaceOnWillBeDestroyedLayer) {
  // Case 1: layer is a pointer.
  auto layer1 = std::make_unique<Layer>();
  scoped_refptr<LayerAnimator> animator(layer1->GetAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer1(false);

  EXPECT_FALSE(observer1.animations_completed());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    settings.AddObserver(&observer1);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer1.animations_completed());
  layer1.reset();
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(observer1.animations_completed());

  // Case 2: layer is a local variable.
  TestImplicitAnimationObserver observer2(false);
  {
    ui::Layer layer2;
    animator = layer2.GetAnimator();

    EXPECT_FALSE(observer2.animations_completed());
    animator->SetBrightness(1.0f);

    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    settings.AddObserver(&observer2);
    animator->SetBrightness(0.0f);
  }

  EXPECT_FALSE(observer2.animations_completed());
  animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
  EXPECT_TRUE(observer2.animations_completed());

  // Case 3: Animation finishes before layer is destroyed.
  ui::Layer layer3;
  animator = layer3.GetAnimator();
  TestImplicitAnimationObserver observer3(false);

  EXPECT_FALSE(observer3.animations_completed());
  animator->SetGrayscale(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    settings.AddObserver(&observer3);
    animator->SetGrayscale(0.0f);
  }

  EXPECT_FALSE(observer3.animations_completed());
  animator->StopAnimatingProperty(LayerAnimationElement::GRAYSCALE);
  EXPECT_TRUE(observer3.animations_completed());
}

// Tests that caching render surface added to two scoped settings objects is
// still reset when animation finishes.
TEST(LayerAnimatorTest, CacheRenderSurfaceInTwoAnimations) {
  ui::Layer layer;
  scoped_refptr<LayerAnimator> animator(layer.GetAnimator());
  animator->set_disable_timer_for_test(true);

  // Case 1: the original cache status if false.
  EXPECT_FALSE(layer.cc_layer_for_testing()->cache_render_surface());
  animator->SetBrightness(1.0f);
  animator->SetOpacity(1.0f);

  // Start the brightness animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    animator->SetBrightness(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());

  // Start the opacity animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    animator->SetOpacity(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());

  // Finish the brightness animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
    EXPECT_FLOAT_EQ(0.0f, layer.layer_brightness());
    EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());
  }

  // Finish the opacity animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    EXPECT_FLOAT_EQ(0.0f, layer.opacity());
    EXPECT_FALSE(layer.cc_layer_for_testing()->cache_render_surface());
  }

  // Case 2: the original cache status if true.
  layer.AddCacheRenderSurfaceRequest();
  EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());
  animator->SetBrightness(1.0f);
  animator->SetOpacity(1.0f);

  // Start the brightness animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    animator->SetBrightness(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());

  // Start the opacity animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.CacheRenderSurface();
    animator->SetOpacity(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());

  // Finish the brightness animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
    EXPECT_FLOAT_EQ(0.0f, layer.layer_brightness());
    EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());
  }

  // Finish the opacity animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    EXPECT_FLOAT_EQ(0.0f, layer.opacity());
    EXPECT_TRUE(layer.cc_layer_for_testing()->cache_render_surface());
  }
}

// Tests that deferred painting request added to a scoped settings object is
// still reset when the object goes out of scope.
TEST(LayerAnimatorTest, DeferredPaint) {
  ui::Layer layer;
  scoped_refptr<LayerAnimator> animator(layer.GetAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer(false);

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_FALSE(layer.IsPaintDeferredForTesting());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    settings.AddObserver(&observer);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_TRUE(layer.IsPaintDeferredForTesting());
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationCompletedForProperty(
      LayerAnimationElement::OPACITY));
  EXPECT_FLOAT_EQ(0.0f, layer.opacity());
  EXPECT_FALSE(layer.IsPaintDeferredForTesting());
}

// Tests that deferred painting request is added to child layers and will be
// removed even the child layer was reparented.
TEST(LayerAnimatorTest, DeferredPaintOnChildLayer) {
  ui::Layer layer;
  ui::Layer child_layer1;
  ui::Layer child_layer2;
  layer.Add(&child_layer1);
  layer.Add(&child_layer2);

  scoped_refptr<LayerAnimator> animator(layer.GetAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer(false);

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_FALSE(layer.IsPaintDeferredForTesting());
  EXPECT_FALSE(child_layer1.IsPaintDeferredForTesting());
  EXPECT_FALSE(child_layer2.IsPaintDeferredForTesting());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    settings.AddObserver(&observer);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_TRUE(layer.IsPaintDeferredForTesting());
  EXPECT_TRUE(child_layer1.IsPaintDeferredForTesting());
  EXPECT_TRUE(child_layer2.IsPaintDeferredForTesting());

  // Reparent child_layer2.
  ui::Layer new_parent_layer;
  new_parent_layer.Add(&child_layer2);
  EXPECT_TRUE(child_layer2.IsPaintDeferredForTesting());

  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationCompletedForProperty(
      LayerAnimationElement::OPACITY));
  EXPECT_FLOAT_EQ(0.0f, layer.opacity());
  EXPECT_FALSE(layer.IsPaintDeferredForTesting());
  EXPECT_FALSE(child_layer1.IsPaintDeferredForTesting());
  EXPECT_FALSE(child_layer2.IsPaintDeferredForTesting());
}

// Tests that deffered paint request added to two scoped settings objects is
// still reset when animation finishes.
TEST(LayerAnimatorTest, DeferredPaintInTwoAnimations) {
  ui::Layer layer;
  scoped_refptr<LayerAnimator> animator(layer.GetAnimator());
  animator->set_disable_timer_for_test(true);

  // Case 1: the original deferred paint status if false.
  EXPECT_FALSE(layer.IsPaintDeferredForTesting());
  animator->SetBrightness(1.0f);
  animator->SetOpacity(1.0f);

  // Start the brightness animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    animator->SetBrightness(0.0f);
  }
  EXPECT_TRUE(layer.IsPaintDeferredForTesting());

  // Start the opacity animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    animator->SetOpacity(0.0f);
  }
  EXPECT_TRUE(layer.IsPaintDeferredForTesting());

  // Finish the brightness animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
    EXPECT_FLOAT_EQ(0.0f, layer.layer_brightness());
    EXPECT_TRUE(layer.IsPaintDeferredForTesting());
  }

  // Finish the opacity animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    EXPECT_FLOAT_EQ(0.0f, layer.opacity());
    EXPECT_FALSE(layer.IsPaintDeferredForTesting());
  }

  // Case 2: the original cache status if true.
  layer.AddDeferredPaintRequest();
  EXPECT_TRUE(layer.IsPaintDeferredForTesting());
  animator->SetBrightness(1.0f);
  animator->SetOpacity(1.0f);

  // Start the brightness animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    animator->SetBrightness(0.0f);
  }
  EXPECT_TRUE(layer.IsPaintDeferredForTesting());

  // Start the opacity animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    animator->SetOpacity(0.0f);
  }
  EXPECT_TRUE(layer.IsPaintDeferredForTesting());

  // Finish the brightness animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
    EXPECT_FLOAT_EQ(0.0f, layer.layer_brightness());
    EXPECT_TRUE(layer.IsPaintDeferredForTesting());
  }

  // Finish the opacity animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    EXPECT_FLOAT_EQ(0.0f, layer.opacity());
    EXPECT_TRUE(layer.IsPaintDeferredForTesting());
  }
}

// Tests that deferred paint request added to a scoped settings object will
// not crash when the layer was destroyed.
TEST(LayerAnimatorTest, DeferredPaintOnWillBeDestroyedLayer) {
  // Case 1: layer is a pointer.
  auto layer1 = std::make_unique<Layer>();
  scoped_refptr<LayerAnimator> animator(layer1->GetAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer1(false);

  EXPECT_FALSE(observer1.animations_completed());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    settings.AddObserver(&observer1);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer1.animations_completed());
  layer1.reset();
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(observer1.animations_completed());

  // Case 2: layer is a local variable.
  TestImplicitAnimationObserver observer2(false);
  {
    ui::Layer layer2;
    animator = layer2.GetAnimator();

    EXPECT_FALSE(observer2.animations_completed());
    animator->SetBrightness(1.0f);

    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    settings.AddObserver(&observer2);
    animator->SetBrightness(0.0f);
  }

  EXPECT_FALSE(observer2.animations_completed());
  animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
  EXPECT_TRUE(observer2.animations_completed());

  // Case 3: Animation finishes before layer is destroyed.
  ui::Layer layer3;
  animator = layer3.GetAnimator();
  TestImplicitAnimationObserver observer3(false);

  EXPECT_FALSE(observer3.animations_completed());
  animator->SetGrayscale(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.DeferPaint();
    settings.AddObserver(&observer3);
    animator->SetGrayscale(0.0f);
  }

  EXPECT_FALSE(observer3.animations_completed());
  animator->StopAnimatingProperty(LayerAnimationElement::GRAYSCALE);
  EXPECT_TRUE(observer3.animations_completed());
}

// Tests that trilinear filtering request added to a scoped settings object is
// still reset when the object goes out of scope.
TEST(LayerAnimatorTest, TrilinearFiltering) {
  ui::Layer layer;
  scoped_refptr<LayerAnimator> animator(layer.GetAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer(false);

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_FALSE(layer.cc_layer_for_testing()->trilinear_filtering());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    settings.AddObserver(&observer);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationCompletedForProperty(
      LayerAnimationElement::OPACITY));
  EXPECT_FLOAT_EQ(0.0f, layer.opacity());
  EXPECT_FALSE(layer.cc_layer_for_testing()->trilinear_filtering());
}

// Tests that trilinear filtering request added to a scoped settings object will
// not crash when the layer was destroyed.
TEST(LayerAnimatorTest, TrilinearFilteringOnWillBeDestroyedLayer) {
  // Case 1: layer is a pointer.
  auto layer1 = std::make_unique<Layer>();
  scoped_refptr<LayerAnimator> animator(layer1->GetAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer1(false);

  EXPECT_FALSE(observer1.animations_completed());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    settings.AddObserver(&observer1);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer1.animations_completed());
  layer1.reset();
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(observer1.animations_completed());

  // Case 2: layer is a local variable.
  TestImplicitAnimationObserver observer2(false);
  {
    ui::Layer layer2;
    animator = layer2.GetAnimator();

    EXPECT_FALSE(observer2.animations_completed());
    animator->SetBrightness(1.0f);

    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    settings.AddObserver(&observer2);
    animator->SetBrightness(0.0f);
  }

  EXPECT_FALSE(observer2.animations_completed());
  animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
  EXPECT_TRUE(observer2.animations_completed());

  // Case 3: Animation finishes before layer is destroyed.
  ui::Layer layer3;
  animator = layer3.GetAnimator();
  TestImplicitAnimationObserver observer3(false);

  EXPECT_FALSE(observer3.animations_completed());
  animator->SetGrayscale(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    settings.AddObserver(&observer3);
    animator->SetGrayscale(0.0f);
  }

  EXPECT_FALSE(observer3.animations_completed());
  animator->StopAnimatingProperty(LayerAnimationElement::GRAYSCALE);
  EXPECT_TRUE(observer3.animations_completed());
}

// Tests that trilinear filtering request added to two scoped settings objects
// is still reset when animation finishes.
TEST(LayerAnimatorTest, TrilinearFilteringInTwoAnimations) {
  ui::Layer layer;
  scoped_refptr<LayerAnimator> animator(layer.GetAnimator());
  animator->set_disable_timer_for_test(true);

  // Case 1: the original trilinear filtering status if false.
  EXPECT_FALSE(layer.cc_layer_for_testing()->trilinear_filtering());
  animator->SetBrightness(1.0f);
  animator->SetOpacity(1.0f);

  // Start the brightness animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    animator->SetBrightness(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());

  // Start the opacity animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    animator->SetOpacity(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());

  // Finish the brightness animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
    EXPECT_FLOAT_EQ(0.0f, layer.layer_brightness());
    EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());
  }

  // Finish the opacity animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    EXPECT_FLOAT_EQ(0.0f, layer.opacity());
    EXPECT_FALSE(layer.cc_layer_for_testing()->trilinear_filtering());
  }

  // Case 2: the original original trilinear status if true.
  layer.AddTrilinearFilteringRequest();
  EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());
  animator->SetBrightness(1.0f);
  animator->SetOpacity(1.0f);

  // Start the brightness animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    animator->SetBrightness(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());

  // Start the opacity animation.
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.TrilinearFiltering();
    animator->SetOpacity(0.0f);
  }
  EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());

  // Finish the brightness animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::BRIGHTNESS);
    EXPECT_FLOAT_EQ(0.0f, layer.layer_brightness());
    EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());
  }

  // Finish the opacity animation.
  {
    animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    EXPECT_FLOAT_EQ(0.0f, layer.opacity());
    EXPECT_TRUE(layer.cc_layer_for_testing()->trilinear_filtering());
  }
}

// Tests that an observer added to a scoped settings object is still notified
// when the object goes out of scope due to the animation being interrupted.
TEST(LayerAnimatorTest, InterruptedImplicitAnimationObservers) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  TestImplicitAnimationObserver observer(false);

  EXPECT_FALSE(observer.animations_completed());
  animator->SetBrightness(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer);
    animator->SetBrightness(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  // This should interrupt the implicit animation causing the observer to be
  // notified immediately.
  animator->SetBrightness(1.0f);
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationCompletedForProperty(
      LayerAnimationElement::BRIGHTNESS));
  EXPECT_FLOAT_EQ(1.0f, delegate.GetBrightnessForAnimation());
}

// Tests that LayerAnimator is not deleted after the animation completes as long
// as there is a live ScopedLayerAnimationSettings object wrapped around it.
TEST(LayerAnimatorTest, AnimatorKeptAliveBySettings) {
  // Note we are using a raw pointer unlike in other tests.
  TestLayerAnimator* animator = new TestLayerAnimator();
  LayerAnimatorDestructionObserver destruction_observer;
  animator->SetDestructionObserver(&destruction_observer);
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  {
    // ScopedLayerAnimationSettings should keep the Animator alive as long as
    // it is alive, even beyond the end of the animation.
    ScopedLayerAnimationSettings settings(animator);
    base::TimeTicks now = base::TimeTicks::Now();
    animator->SetBrightness(0.5);
    animator->Step(now + base::TimeDelta::FromSeconds(1));
    EXPECT_FALSE(destruction_observer.IsAnimatorDeleted());
  }
  // ScopedLayerAnimationSettings was destroyed, so Animator should be deleted.
  EXPECT_TRUE(destruction_observer.IsAnimatorDeleted());
}

// Tests that an observer added to a scoped settings object is not notified
// when the animator is destroyed unless explicitly requested.
TEST(LayerAnimatorTest, ImplicitObserversAtAnimatorDestruction) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  TestImplicitAnimationObserver observer_notify(true);
  TestImplicitAnimationObserver observer_do_not_notify(false);

  EXPECT_FALSE(observer_notify.animations_completed());
  EXPECT_FALSE(observer_do_not_notify.animations_completed());

  animator->SetBrightness(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer_notify);
    settings.AddObserver(&observer_do_not_notify);
    animator->SetBrightness(0.0f);
  }

  EXPECT_FALSE(observer_notify.animations_completed());
  EXPECT_FALSE(observer_do_not_notify.animations_completed());
  animator.reset();
  EXPECT_TRUE(observer_notify.animations_completed());
  EXPECT_TRUE(observer_notify.WasAnimationAbortedForProperty(
      LayerAnimationElement::BRIGHTNESS));
  EXPECT_FALSE(observer_do_not_notify.animations_completed());
}

TEST(LayerAnimatorTest, AbortedAnimationStatusInImplicitObservers) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  TestImplicitAnimationObserver observer(false);

  EXPECT_FALSE(observer.animations_completed());
  animator->SetBrightness(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer);
    animator->SetBrightness(0.0f);
  }
  EXPECT_FALSE(observer.animations_completed());

  animator->AbortAllAnimations();
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationAbortedForProperty(
      LayerAnimationElement::BRIGHTNESS));
  EXPECT_FALSE(observer.WasAnimationAbortedForProperty(
      LayerAnimationElement::OPACITY));

  observer.set_animations_completed(false);
  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer);
    animator->SetOpacity(0.0f);
  }
  EXPECT_FALSE(observer.animations_completed());

  animator->AbortAllAnimations();
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationAbortedForProperty(
      LayerAnimationElement::BRIGHTNESS));
  EXPECT_TRUE(observer.WasAnimationAbortedForProperty(
      LayerAnimationElement::OPACITY));
}

TEST(LayerAnimatorTest, RemoveObserverShouldRemoveFromSequences) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  TestLayerAnimationObserver observer;
  TestLayerAnimationObserver removed_observer;

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta));

  sequence->AddObserver(&observer);
  sequence->AddObserver(&removed_observer);

  animator->StartAnimation(sequence);

  EXPECT_EQ(observer.last_scheduled_sequence(), sequence);
  EXPECT_TRUE(!observer.last_ended_sequence());
  EXPECT_EQ(removed_observer.last_scheduled_sequence(), sequence);
  EXPECT_TRUE(!removed_observer.last_ended_sequence());

  // This should stop the observer from observing sequence.
  animator->RemoveObserver(&removed_observer);

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ(observer.last_ended_sequence(), sequence);
  EXPECT_TRUE(!removed_observer.last_ended_sequence());
}

TEST(LayerAnimatorTest, ObserverReleasedBeforeAnimationSequenceEnds) {
  TestLayerAnimationDelegate delegate;
  std::unique_ptr<TestLayerAnimationObserver> observer(
      new TestLayerAnimationObserver);
  scoped_refptr<LayerAnimator> animator(
      CreateDefaultTestAnimator(&delegate, observer.get()));

  delegate.SetOpacityFromAnimation(0.0f,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  animator->StartAnimation(sequence);

  // |observer| should be attached to |sequence|.
  EXPECT_TRUE(sequence->observers_.might_have_observers());

  // Now, release |observer|
  observer.reset();

  // And |sequence| should no longer be attached to |observer|.
  EXPECT_FALSE(sequence->observers_.might_have_observers());
}

TEST(LayerAnimatorTest, ObserverAttachedAfterAnimationStarted) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  TestImplicitAnimationObserver observer(false);

  delegate.SetBrightnessFromAnimation(0.0f,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  {
    ScopedLayerAnimationSettings setter(animator.get());

    base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
    LayerAnimationSequence* sequence = new LayerAnimationSequence(
        LayerAnimationElement::CreateBrightnessElement(1.0f, delta));

    animator->StartAnimation(sequence);
    base::TimeTicks start_time = animator->last_step_time();
    animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

    setter.AddObserver(&observer);

    // Start observing an in-flight animation.
    sequence->AddObserver(&observer);

    animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  }

  EXPECT_TRUE(observer.animations_completed());
  EXPECT_TRUE(observer.WasAnimationCompletedForProperty(
                  LayerAnimationElement::BRIGHTNESS));
}

TEST(LayerAnimatorTest, ObserverDetachedBeforeAnimationFinished) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  TestImplicitAnimationObserver observer(false);

  delegate.SetBrightnessFromAnimation(0.0f,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta));

  {
    ScopedLayerAnimationSettings setter(animator.get());
    setter.AddObserver(&observer);

    animator->StartAnimation(sequence);
    base::TimeTicks start_time = animator->last_step_time();
    animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));
  }

  EXPECT_FALSE(observer.animations_completed());

  // Stop observing an in-flight animation.
  sequence->RemoveObserver(&observer);

  EXPECT_TRUE(observer.animations_completed());

  // The animation didn't complete, and neither was it aborted.
  EXPECT_FALSE(observer.WasAnimationCompletedForProperty(
                  LayerAnimationElement::BRIGHTNESS));
  EXPECT_FALSE(observer.WasAnimationAbortedForProperty(
                  LayerAnimationElement::BRIGHTNESS));
}

// This checks that if an animation is deleted due to a callback, that the
// animator does not try to use the deleted animation. For example, if we have
// two running animations, and the first finishes and the resulting callback
// causes the second to be deleted, we should not attempt to animate the second
// animation.
TEST(LayerAnimatorTest, ObserverDeletesAnimationsOnEnd) {
  ScopedAnimationDurationScaleMode normal_duration_mode(
      ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  scoped_refptr<LayerAnimator> animator(new TestLayerAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_brightness(0.0);
  double target_brightness(1.0);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(5, 5, 5, 5);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  base::TimeDelta brightness_delta = base::TimeDelta::FromSeconds(1);
  base::TimeDelta halfway_delta = base::TimeDelta::FromSeconds(2);
  base::TimeDelta bounds_delta = base::TimeDelta::FromSeconds(3);

  std::unique_ptr<DeletingLayerAnimationObserver> observer(
      new DeletingLayerAnimationObserver(animator.get()));

  animator->AddObserver(observer.get());

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(
              target_brightness, brightness_delta)));

  animator->StartAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(
          target_bounds, bounds_delta)));
  ASSERT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  base::TimeTicks start_time = animator->last_step_time();
  animator->Step(start_time + halfway_delta);

  // Completing the brightness animation should have stopped the bounds
  // animation.
  ASSERT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  animator->RemoveObserver(observer.get());
}

// Ensure that stopping animation in a bounds change does not crash and that
// animation gets stopped correctly.
// This scenario is possible when animation is restarted from inside a
// callback triggered by the animation progress.
TEST(LayerAnimatorTest, CallbackDeletesAnimationInProgress) {

  class TestLayerAnimationDeletingDelegate : public TestLayerAnimationDelegate {
   public:
    TestLayerAnimationDeletingDelegate(LayerAnimator* animator, int max_width)
      : animator_(animator),
        max_width_(max_width) {
    }

    void SetBoundsFromAnimation(const gfx::Rect& bounds,
                                PropertyChangeReason reason) override {
      TestLayerAnimationDelegate::SetBoundsFromAnimation(bounds, reason);
      if (bounds.width() > max_width_)
        animator_->StopAnimating();
    }
   private:
    LayerAnimator* animator_;
    int max_width_;
    // Allow copy and assign.
  };

  ScopedAnimationDurationScaleMode normal_duration_mode(
      ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  scoped_refptr<LayerAnimator> animator(new TestLayerAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDeletingDelegate delegate(animator.get(), 30);
  animator->SetDelegate(&delegate);

  gfx::Rect start_bounds(0, 0, 0, 0);
  gfx::Rect target_bounds(5, 5, 50, 50);

  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  base::TimeDelta bounds_delta1 = base::TimeDelta::FromMilliseconds(333);
  base::TimeDelta bounds_delta2 = base::TimeDelta::FromMilliseconds(666);
  base::TimeDelta bounds_delta = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta final_delta = base::TimeDelta::FromMilliseconds(1500);

  animator->StartAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(
          target_bounds, bounds_delta)));
  ASSERT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  base::TimeTicks start_time = animator->last_step_time();
  ASSERT_NO_FATAL_FAILURE(animator->Step(start_time + bounds_delta1));
  ASSERT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  // The next step should change the animated bounds past the threshold and
  // cause the animaton to stop.
  ASSERT_NO_FATAL_FAILURE(animator->Step(start_time + bounds_delta2));
  ASSERT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
  ASSERT_NO_FATAL_FAILURE(animator->Step(start_time + final_delta));

  // Completing the animation should have stopped the bounds
  // animation.
  ASSERT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
}

// Similar to the ObserverDeletesAnimationsOnEnd test above except that it
// tests the behavior when the OnLayerAnimationAborted() callback causes
// all of the animator's other animations to be deleted.
TEST(LayerAnimatorTest, ObserverDeletesAnimationsOnAbort) {
  ScopedAnimationDurationScaleMode test_duration_mode(
      ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  scoped_refptr<LayerAnimator> animator(new TestLayerAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_brightness(0.0);
  double target_brightness(1.0);
  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(5, 5, 5, 5);
  base::TimeDelta brightness_delta = base::TimeDelta::FromSeconds(1);
  base::TimeDelta bounds_delta = base::TimeDelta::FromSeconds(2);

  delegate.SetBrightnessFromAnimation(start_brightness,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  std::unique_ptr<DeletingLayerAnimationObserver> observer(
      new DeletingLayerAnimationObserver(animator.get()));
  animator->AddObserver(observer.get());

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(
              target_brightness, brightness_delta)));
  animator->StartAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(
          target_bounds, bounds_delta)));
  ASSERT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBrightnessElement(
              target_brightness, brightness_delta)));

  // Starting the second brightness animation should have aborted the initial
  // brightness animation. |observer| should have stopped the bounds animation
  // as a result.
  ASSERT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  animator->RemoveObserver(observer.get());
}

// Check that setting a property during an animation with a default animator
// cancels the original animation.
TEST(LayerAnimatorTest, SettingPropertyDuringAnAnimation) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  std::unique_ptr<LayerAnimationSequence> sequence =
      std::make_unique<LayerAnimationSequence>(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta));

  animator->StartAnimation(sequence.release());

  animator->SetOpacity(0.5);

  EXPECT_FALSE(animator->is_animating());
  EXPECT_EQ(0.5, animator->GetTargetOpacity());
}

// Tests that the preemption mode IMMEDIATELY_SET_NEW_TARGET, doesn't cause the
// second sequence to be leaked.
TEST(LayerAnimatorTest, ImmediatelySettingNewTargetDoesNotLeak) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  animator->set_preemption_strategy(LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect middle_bounds(10, 10, 100, 100);
  gfx::Rect target_bounds(5, 5, 5, 5);

  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  {
    // start an implicit bounds animation.
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetBounds(middle_bounds);
  }

  EXPECT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  int num_live_instances = 0;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  std::unique_ptr<TestLayerAnimationSequence> sequence =
      std::make_unique<TestLayerAnimationSequence>(
          LayerAnimationElement::CreateBoundsElement(target_bounds, delta),
          &num_live_instances);

  EXPECT_EQ(1, num_live_instances);

  // This should interrupt the running sequence causing us to immediately set
  // the target value. The sequence should alse be destructed.
  animator->StartAnimation(sequence.release());

  EXPECT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
  EXPECT_EQ(0, num_live_instances);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Verifies GetTargetOpacity() works when multiple sequences are scheduled.
TEST(LayerAnimatorTest, GetTargetOpacity) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);

  delegate.SetOpacityFromAnimation(0.0,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetOpacity(0.5);
    EXPECT_EQ(0.5, animator->GetTargetOpacity());

    // Because the strategy is ENQUEUE_NEW_ANIMATION the target should now be 1.
    animator->SetOpacity(1.0);
    EXPECT_EQ(1.0, animator->GetTargetOpacity());
  }
}

// Verifies GetTargetBrightness() works when multiple sequences are scheduled.
TEST(LayerAnimatorTest, GetTargetBrightness) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);

  delegate.SetBrightnessFromAnimation(0.0,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetBrightness(0.5);
    EXPECT_EQ(0.5, animator->GetTargetBrightness());

    // Because the strategy is ENQUEUE_NEW_ANIMATION the target should now be 1.
    animator->SetBrightness(1.0);
    EXPECT_EQ(1.0, animator->GetTargetBrightness());
  }
}

// Verifies GetTargetGrayscale() works when multiple sequences are scheduled.
TEST(LayerAnimatorTest, GetTargetGrayscale) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);

  delegate.SetGrayscaleFromAnimation(0.0,
                                     PropertyChangeReason::NOT_FROM_ANIMATION);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetGrayscale(0.5);
    EXPECT_EQ(0.5, animator->GetTargetGrayscale());

    // Because the strategy is ENQUEUE_NEW_ANIMATION the target should now be 1.
    animator->SetGrayscale(1.0);
    EXPECT_EQ(1.0, animator->GetTargetGrayscale());
  }
}

// Verifies color property is modified appropriately.
TEST(LayerAnimatorTest, Color) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  SkColor start_color  = SkColorSetARGB( 64, 20, 40,  60);
  SkColor middle_color = SkColorSetARGB(128, 35, 70, 120);
  SkColor target_color = SkColorSetARGB(192, 40, 80, 140);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetColorFromAnimation(start_color,
                                 PropertyChangeReason::NOT_FROM_ANIMATION);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateColorElement(target_color, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_EQ(ColorToString(start_color),
            ColorToString(delegate.GetColorForAnimation()));

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_EQ(ColorToString(middle_color),
            ColorToString(delegate.GetColorForAnimation()));

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_EQ(ColorToString(target_color),
            ColorToString(delegate.GetColorForAnimation()));
}

// Verifies SchedulePauseForProperties().
TEST(LayerAnimatorTest, SchedulePauseForProperties) {
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator());
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);
  animator->SchedulePauseForProperties(
      base::TimeDelta::FromMilliseconds(100),
      LayerAnimationElement::TRANSFORM | LayerAnimationElement::BOUNDS);
  EXPECT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::TRANSFORM));
  EXPECT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
  EXPECT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::OPACITY));
}

// Verifies that IsAnimatingOnePropertyOf() returns true iff at least one of the
// input properties is animated.
TEST(LayerAnimatorTest, IsAnimatingOnePropertyOf) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator(&delegate));
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);
  animator->SetBrightness(0.5f);
  animator->SetGrayscale(0.5f);
  EXPECT_TRUE(animator->IsAnimatingOnePropertyOf(
      LayerAnimationElement::LayerAnimationElement::BRIGHTNESS));
  EXPECT_TRUE(animator->IsAnimatingOnePropertyOf(
      LayerAnimationElement::LayerAnimationElement::GRAYSCALE));
  EXPECT_TRUE(animator->IsAnimatingOnePropertyOf(
      LayerAnimationElement::LayerAnimationElement::BRIGHTNESS |
      LayerAnimationElement::LayerAnimationElement::GRAYSCALE));
  EXPECT_TRUE(animator->IsAnimatingOnePropertyOf(
      LayerAnimationElement::LayerAnimationElement::BRIGHTNESS));
  EXPECT_FALSE(animator->IsAnimatingOnePropertyOf(
      LayerAnimationElement::LayerAnimationElement::BOUNDS));
}

class AnimatorOwner {
 public:
  AnimatorOwner() : animator_(CreateDefaultTestAnimator()) {}

  LayerAnimator* animator() { return animator_.get(); }

 private:
  scoped_refptr<LayerAnimator> animator_;

  DISALLOW_COPY_AND_ASSIGN(AnimatorOwner);
};

class DeletingObserver : public LayerAnimationObserver {
 public:
  explicit DeletingObserver(bool* was_deleted)
      : animator_owner_(new AnimatorOwner),
        delete_on_animation_ended_(false),
        delete_on_animation_aborted_(false),
        delete_on_animation_scheduled_(false),
        was_deleted_(was_deleted) {
    animator()->AddObserver(this);
  }

  ~DeletingObserver() override {
    animator()->RemoveObserver(this);
    *was_deleted_ = true;
  }

  LayerAnimator* animator() { return animator_owner_->animator(); }

  bool delete_on_animation_ended() const {
    return delete_on_animation_ended_;
  }
  void set_delete_on_animation_ended(bool enabled) {
    delete_on_animation_ended_ = enabled;
  }

  bool delete_on_animation_aborted() const {
    return delete_on_animation_aborted_;
  }
  void set_delete_on_animation_aborted(bool enabled) {
    delete_on_animation_aborted_ = enabled;
  }

  bool delete_on_animation_scheduled() const {
    return delete_on_animation_scheduled_;
  }
  void set_delete_on_animation_scheduled(bool enabled) {
    delete_on_animation_scheduled_ = enabled;
  }

  // LayerAnimationObserver implementation.
  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override {
    if (delete_on_animation_ended_)
      delete this;
  }

  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {
    if (delete_on_animation_aborted_)
      delete this;
  }

  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override {
    if (delete_on_animation_scheduled_)
      delete this;
  }

 private:
  std::unique_ptr<AnimatorOwner> animator_owner_;
  bool delete_on_animation_ended_;
  bool delete_on_animation_aborted_;
  bool delete_on_animation_scheduled_;
  bool* was_deleted_;

  DISALLOW_COPY_AND_ASSIGN(DeletingObserver);
};

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterFinishingAnimation) {
  bool observer_was_deleted = false;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  observer->set_delete_on_animation_ended(true);
  observer->set_delete_on_animation_aborted(true);
  LayerAnimator* animator = observer->animator();
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetBrightnessFromAnimation(0.0f,
                                      PropertyChangeReason::NOT_FROM_ANIMATION);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* brightness_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta));
  animator->StartAnimation(brightness_sequence);

  delta = base::TimeDelta::FromSeconds(2);
  LayerAnimationSequence* bounds_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta));
  animator->StartAnimation(bounds_sequence);

  base::TimeTicks start_time = animator->last_step_time();
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(observer_was_deleted);
}

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterStoppingAnimating) {
  bool observer_was_deleted = false;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  observer->set_delete_on_animation_ended(true);
  observer->set_delete_on_animation_aborted(true);
  LayerAnimator* animator = observer->animator();
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* opacity_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));
  animator->StartAnimation(opacity_sequence);

  delta = base::TimeDelta::FromSeconds(2);
  LayerAnimationSequence* bounds_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta));
  animator->StartAnimation(bounds_sequence);

  animator->StopAnimating();

  EXPECT_TRUE(observer_was_deleted);
}

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterScheduling) {
  bool observer_was_deleted = false;
  TestLayerAnimationDelegate delegate;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  observer->set_delete_on_animation_scheduled(true);
  LayerAnimator* animator = observer->animator();
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  std::vector<LayerAnimationSequence*> to_start;

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta)));

  delta = base::TimeDelta::FromSeconds(2);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  animator->ScheduleTogether(to_start);

  EXPECT_TRUE(observer_was_deleted);
}

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterAborted) {
  bool observer_was_deleted = false;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  TestLayerAnimationDelegate delegate;
  observer->set_delete_on_animation_aborted(true);
  LayerAnimator* animator = observer->animator();
  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds,
                                  PropertyChangeReason::NOT_FROM_ANIMATION);

  std::vector<LayerAnimationSequence*> to_start;

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta)));

  delta = base::TimeDelta::FromSeconds(2);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  animator->ScheduleTogether(to_start);

  EXPECT_FALSE(observer_was_deleted);

  animator->StartAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta)));

  EXPECT_TRUE(observer_was_deleted);
}


TEST(LayerAnimatorTest, TestSetterRespectEnqueueStrategy) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  float start_opacity = 0.0f;
  float target_opacity = 1.0f;
  float magic_opacity = 0.123f;

  delegate.SetOpacityFromAnimation(start_opacity,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  ScopedLayerAnimationSettings settings(animator.get());
  settings.SetPreemptionStrategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(base::TimeDelta::FromSeconds(1));
  animator->SetOpacity(target_opacity);

  EXPECT_EQ(start_opacity, delegate.GetOpacityForAnimation());

  settings.SetPreemptionStrategy(
      LayerAnimator::ENQUEUE_NEW_ANIMATION);
  settings.SetTransitionDuration(base::TimeDelta());
  animator->SetOpacity(magic_opacity);

  EXPECT_EQ(start_opacity, delegate.GetOpacityForAnimation());
}

class CollectionLayerAnimationDelegate : public TestLayerAnimationDelegate {
 public:
  CollectionLayerAnimationDelegate() : collection(NULL) {}
  ~CollectionLayerAnimationDelegate() override {}

  // LayerAnimationDelegate:
  LayerAnimatorCollection* GetLayerAnimatorCollection() override {
    return &collection;
  }

 private:
  LayerAnimatorCollection collection;
};

TEST(LayerAnimatorTest, LayerAnimatorCollectionTickTime) {
  Layer layer;
  LayerAnimator* animator = layer.GetAnimator();
  CollectionLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  LayerAnimatorCollection* collection = delegate.GetLayerAnimatorCollection();
  base::TimeTicks null;
  collection->OnAnimationStep(null);
  EXPECT_TRUE(collection->last_tick_time().is_null());

  // Adding an animator to the collection should update the last tick time.
  collection->StartAnimator(layer.GetAnimator());
  EXPECT_TRUE(collection->HasActiveAnimators());
  EXPECT_FALSE(collection->last_tick_time().is_null());

  collection->StopAnimator(layer.GetAnimator());
  EXPECT_FALSE(collection->HasActiveAnimators());

  animator->SetDelegate(nullptr);
}

// Verifies that sequences are removed from the animation queue prior to be
// notified of being started. This will allow observers to abort animations in
// the OnLayerAnimationStarted() callback without triggering a sequence to be
// started a second time.
TEST(LayerAnimatorTest,
     SequencesAreRemovedFromQueueBeforeBeingNotifiedOfStarted) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  AbortAnimationsOnStartedLayerAnimationObserver aborting_observer(
      animator.get());
  test::CountCheckingLayerAnimationObserver counting_observer(
      &aborting_observer);

  const base::TimeDelta sequence_duration = base::TimeDelta::FromSeconds(1);

  animator->ScheduleAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.f, sequence_duration)));

  LayerAnimationSequence* observed_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(0.f, sequence_duration));
  observed_sequence->AddObserver(&counting_observer);
  animator->ScheduleAnimation(observed_sequence);

  EXPECT_TRUE(animator->is_animating());
  const base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + sequence_duration * 3);
  EXPECT_FALSE(animator->is_animating());
}

TEST(LayerAnimatorTest, AnimatorStartedCorrectly) {
  Layer layer;
  LayerAnimatorTestController test_controller(layer.GetAnimator());
  LayerAnimator* animator = test_controller.animator();
  ASSERT_FALSE(animator->is_started_);

  TestLayerAnimationDelegate test_delegate;
  animator->SetDelegate(&test_delegate);
  double target_opacity = 1.0;
  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);
  animator->ScheduleAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(target_opacity, time_delta)));
  EXPECT_FALSE(animator->is_started_);

  CollectionLayerAnimationDelegate collection_delegate;
  animator->SetDelegate(&collection_delegate);
  animator->UpdateAnimationState();
  EXPECT_TRUE(animator->is_started_);
  animator->SetDelegate(NULL);
}

TEST(LayerAnimatorTest, AnimatorRemovedFromCollectionWhenLayerIsDestroyed) {
  std::unique_ptr<Layer> layer(new Layer(LAYER_TEXTURED));
  LayerAnimatorTestController test_controller(layer->GetAnimator());
  scoped_refptr<LayerAnimator> animator = test_controller.animator();
  CollectionLayerAnimationDelegate collection_delegate;
  animator->SetDelegate(&collection_delegate);

  double target_opacity = 1.0;
  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);
  animator->ScheduleAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(target_opacity, time_delta)));

  EXPECT_TRUE(
      collection_delegate.GetLayerAnimatorCollection()->HasActiveAnimators());

  layer.reset();
  EXPECT_EQ(NULL, animator->delegate());
  EXPECT_FALSE(
      collection_delegate.GetLayerAnimatorCollection()->HasActiveAnimators());
}

TEST(LayerAnimatorTest, LayerMovedBetweenCompositorsDuringAnimation) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::UI);
  const bool enable_pixel_output = false;
  TestContextFactories context_factories(enable_pixel_output);
  const gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<TestCompositorHost> host_1(
      TestCompositorHost::Create(bounds, context_factories.GetContextFactory(),
                                 context_factories.GetContextFactoryPrivate()));
  std::unique_ptr<TestCompositorHost> host_2(
      TestCompositorHost::Create(bounds, context_factories.GetContextFactory(),
                                 context_factories.GetContextFactoryPrivate()));
  host_1->Show();
  host_2->Show();

  Compositor* compositor_1 = host_1->GetCompositor();
  Layer root_1;
  compositor_1->SetRootLayer(&root_1);
  cc::MutatorHost* mutator_host_1 =
      root_1.cc_layer_for_testing()->layer_tree_host()->mutator_host();

  Compositor* compositor_2 = host_2->GetCompositor();
  Layer root_2;
  compositor_2->SetRootLayer(&root_2);
  cc::MutatorHost* mutator_host_2 =
      root_2.cc_layer_for_testing()->layer_tree_host()->mutator_host();

  // Verify that neither compositor has active animators.
  EXPECT_FALSE(compositor_1->layer_animator_collection()->HasActiveAnimators());
  EXPECT_FALSE(compositor_2->layer_animator_collection()->HasActiveAnimators());

  Layer layer;
  layer.SetOpacity(0.5f);
  root_1.Add(&layer);
  EXPECT_FALSE(
      mutator_host_1->HasTickingKeyframeModelForTesting(layer.element_id()));
  EXPECT_FALSE(
      mutator_host_2->HasTickingKeyframeModelForTesting(layer.element_id()));

  LayerAnimator* animator = layer.GetAnimator();
  double target_opacity = 1.0;
  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);

  animator->ScheduleAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(target_opacity, time_delta)));
  EXPECT_TRUE(compositor_1->layer_animator_collection()->HasActiveAnimators());
  EXPECT_FALSE(compositor_2->layer_animator_collection()->HasActiveAnimators());
  EXPECT_TRUE(
      mutator_host_1->HasTickingKeyframeModelForTesting(layer.element_id()));
  EXPECT_FALSE(
      mutator_host_2->HasTickingKeyframeModelForTesting(layer.element_id()));

  root_2.Add(&layer);
  EXPECT_FALSE(compositor_1->layer_animator_collection()->HasActiveAnimators());
  EXPECT_TRUE(compositor_2->layer_animator_collection()->HasActiveAnimators());
  EXPECT_FALSE(
      mutator_host_1->HasTickingKeyframeModelForTesting(layer.element_id()));
  EXPECT_TRUE(
      mutator_host_2->HasTickingKeyframeModelForTesting(layer.element_id()));

  host_2.reset();
  host_1.reset();
}

TEST(LayerAnimatorTest, ThreadedAnimationSurvivesIfLayerRemovedAdded) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::UI);
  const bool enable_pixel_output = false;
  TestContextFactories context_factories(enable_pixel_output);
  const gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<TestCompositorHost> host(
      TestCompositorHost::Create(bounds, context_factories.GetContextFactory(),
                                 context_factories.GetContextFactoryPrivate()));
  host->Show();

  Compositor* compositor = host->GetCompositor();

  Layer root;
  compositor->SetRootLayer(&root);

  Layer layer;
  layer.SetOpacity(0.5f);
  root.Add(&layer);

  LayerAnimator* animator = layer.GetAnimator();
  double target_opacity = 1.0;
  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);

  animator->ScheduleAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(target_opacity, time_delta)));

  cc::MutatorHost* mutator =
      layer.cc_layer_for_testing()->layer_tree_host()->mutator_host();
  EXPECT_TRUE(mutator->HasTickingKeyframeModelForTesting(layer.element_id()));

  root.Remove(&layer);

  root.Add(&layer);

  mutator = layer.cc_layer_for_testing()->layer_tree_host()->mutator_host();
  EXPECT_TRUE(mutator->HasTickingKeyframeModelForTesting(layer.element_id()));

  host.reset();
}

// A simple AnimationMetricsReporter class that remembers smoothness metric
// when animation completes.
class TestMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  TestMetricsReporter() {}
  ~TestMetricsReporter() override {}

  bool report_called() { return report_called_; }
  int value() const { return value_; }

 protected:
  void Report(int value) override {
    value_ = value;
    report_called_ = true;
  }

 private:
  bool report_called_ = false;
  int value_ = -1;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsReporter);
};

// Starts an animation and tests that incrementing compositor frame count can
// be used to report animation smoothness metrics. This verifies that when an
// animation is smooth (frame count matches refresh rate) that we receive a
// smoothness value of 100.
TEST(LayerAnimatorTest, ReportMetricsSmooth) {
  base::ScopedMockClockOverride mock_clock_;
  const base::TimeDelta kAnimationDuration =
      base::TimeDelta::FromMilliseconds(100);

  std::unique_ptr<Layer> root(new Layer(LAYER_SOLID_COLOR));
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator(&delegate));

  std::unique_ptr<ui::LayerAnimationElement> animation_element =
      ui::LayerAnimationElement::CreateColorElement(SK_ColorRED,
                                                    kAnimationDuration);
  ui::LayerAnimationSequence* animation_sequence =
      new ui::LayerAnimationSequence(std::move(animation_element));
  TestMetricsReporter reporter;

  animation_sequence->SetAnimationMetricsReporter(&reporter);
  animator->StartAnimation(animation_sequence);
  // Advances the frame count, simulating having produced 6 frames, and received
  // an ack for each one. This implicitly simulates calling LayerAnimator::Step
  // with an increased clock 6 times.
  delegate.SetFrameNumber(6);

  // Advancing the clock does not implicitly advance frame count. It allows the
  // next call of LayerAnimator::Step to detect that the animation is finished,
  // thus triggering the processing of metrics reporting.
  mock_clock_.Advance(kAnimationDuration);
  animator->Step(mock_clock_.NowTicks());

  CHECK(reporter.report_called());
  // With the clock being controlled, the animations should be smooth regardless
  // of the test bots. Smoothness is based on a calculated duration of the
  // animation when compared to the requested duration, as an integer
  // percentage. With 100 being 100%. When the number of frames match the
  // refresh rate, reporter should be notified of a smoothness of 100.
  EXPECT_EQ(reporter.value(), 100);
}

// Starts an animation and tests that incrementing compositor frame count can
// be used to report animation smoothness metrics. This verifies that when an
// animation is not smooth (frame count doesn't match refresh rate) that we
// receive a smoothness value that is not 100.
TEST(LayerAnimatorTest, ReportMetricsNotSmooth) {
  base::ScopedMockClockOverride mock_clock_;
  const base::TimeDelta kAnimationDuration =
      base::TimeDelta::FromMilliseconds(100);

  std::unique_ptr<Layer> root(new Layer(LAYER_SOLID_COLOR));
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator(&delegate));

  std::unique_ptr<ui::LayerAnimationElement> animation_element =
      ui::LayerAnimationElement::CreateColorElement(SK_ColorRED,
                                                    kAnimationDuration);
  ui::LayerAnimationSequence* animation_sequence =
      new ui::LayerAnimationSequence(std::move(animation_element));
  TestMetricsReporter reporter;

  animation_sequence->SetAnimationMetricsReporter(&reporter);
  animator->StartAnimation(animation_sequence);
  // Advances the frame count, simulating having produced 1 frame, and received
  // an ack for it. This implicitly simulates calling LayerAnimator::Step with
  // an increased clock 1 time.
  delegate.SetFrameNumber(1);

  // Advancing the clock does not implicitly advance frame count. It allows the
  // next call of LayerAnimator::Step to detect that the animation is finished,
  // thus triggering the processing of metrics reporting.
  mock_clock_.Advance(kAnimationDuration);
  animator->Step(mock_clock_.NowTicks());

  CHECK(reporter.report_called());
  // With the clock being controlled, we have direct control of the smoothness
  // regardless of the test bots. Smoothness is based on a calculated duration
  // of the animation when compared to the requested duration, as an integer
  // percentage. With 100 being 100%. With a single frame being generated over
  // 100ms, the reporter should be notified of a smoothness matching the
  // interval between refreshes.
  int expected = base::Time::kMillisecondsPerSecond / delegate.GetRefreshRate();
  EXPECT_EQ(reporter.value(), expected);
}

class LayerOwnerAnimationObserver : public LayerAnimationObserver {
 public:
  explicit LayerOwnerAnimationObserver(LayerAnimator* animator)
      : animator_layer_(new Layer(LAYER_TEXTURED)) {
    animator_layer_->SetAnimator(animator);
  }

  ~LayerOwnerAnimationObserver() override {}

  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override {
    ASSERT_TRUE(sequence);
    animator_layer_.reset();
  }

  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {
    ASSERT_TRUE(sequence);
    animator_layer_.reset();
  }

  LayerAnimationDelegate* animator_layer() {
    return animator_layer_.get();
  }

  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override {}

 private:
  std::unique_ptr<Layer> animator_layer_;

  DISALLOW_COPY_AND_ASSIGN(LayerOwnerAnimationObserver);
};

TEST(LayerAnimatorTest, ObserverDeletesLayerInStopAnimating) {
  scoped_refptr<LayerAnimator> animator(CreateImplicitTestAnimator());
  LayerOwnerAnimationObserver observer(animator.get());
  LayerAnimationDelegate* delegate = observer.animator_layer();

  const gfx::Rect start_bounds(0, 0, 50, 50);
  const gfx::Rect target_bounds(10, 10, 100, 100);
  const double target_opacity = 1.0;

  delegate->SetOpacityFromAnimation(0.0f,
                                    PropertyChangeReason::NOT_FROM_ANIMATION);
  delegate->SetBoundsFromAnimation(start_bounds,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);

  base::TimeDelta time_delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* opacity = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(target_opacity, time_delta));
  opacity->AddObserver(&observer);
  animator->ScheduleAnimation(opacity);
  time_delta = base::TimeDelta::FromSeconds(2);
  LayerAnimationSequence* move = new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, time_delta));
  animator->ScheduleAnimation(move);
  EXPECT_TRUE(animator->is_animating());
  animator->Step(animator->last_step_time() +
                 base::TimeDelta::FromMilliseconds(500));
  animator->StopAnimating();

  EXPECT_EQ(nullptr, observer.animator_layer());
  EXPECT_TRUE(animator->is_animating());
}

// Verifies the LayerAnimatorObserver notification order for an animation
// sequence that completes successfully.
TEST(LayerAnimatorObserverNotificationOrderTest,
     SuccessfulCompletionOfSequence) {
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(
      CreateDefaultTestAnimator(&delegate, &observer));
  observer.set_requires_notification_when_animator_destroyed(true);

  const base::TimeDelta animation_duration = base::TimeDelta::FromSeconds(100);

  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, animation_duration));

  EXPECT_TRUE(observer.NoEventsObserved());

  animator->StartAnimation(sequence);

  EXPECT_EQ(observer.last_attached_sequence(), sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), sequence);
  EXPECT_EQ(observer.last_started_sequence(), sequence);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), nullptr);

  EXPECT_TRUE(observer.AttachedEpochIsBeforeScheduledEpoch());
  EXPECT_TRUE(observer.ScheduledEpochIsBeforeStartedEpoch());

  observer.ResetLayerAnimationObserverations();

  const base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + animation_duration);

  EXPECT_EQ(observer.last_attached_sequence(), nullptr);
  EXPECT_EQ(observer.last_scheduled_sequence(), nullptr);
  EXPECT_EQ(observer.last_started_sequence(), nullptr);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), sequence);
  EXPECT_EQ(observer.last_detached_sequence(), sequence);

  EXPECT_TRUE(observer.EndedEpochIsBeforeDetachedEpoch());
}

// Verifies the LayerAnimatorObserver notification order for an animation
// sequence that is aborted after being scheduled.
TEST(LayerAnimatorObserverNotificationOrderTest, AbortingAScheduledSequence) {
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(
      CreateDefaultTestAnimator(&delegate, &observer));
  observer.set_requires_notification_when_animator_destroyed(true);

  const base::TimeDelta animation_duration = base::TimeDelta::FromSeconds(100);

  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, animation_duration));

  EXPECT_TRUE(observer.NoEventsObserved());

  animator->StartAnimation(sequence);

  EXPECT_EQ(observer.last_attached_sequence(), sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), sequence);
  EXPECT_EQ(observer.last_started_sequence(), sequence);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), nullptr);

  EXPECT_TRUE(observer.AttachedEpochIsBeforeScheduledEpoch());
  EXPECT_TRUE(observer.ScheduledEpochIsBeforeStartedEpoch());

  observer.ResetLayerAnimationObserverations();

  animator->AbortAllAnimations();

  EXPECT_EQ(observer.last_attached_sequence(), nullptr);
  EXPECT_EQ(observer.last_scheduled_sequence(), nullptr);
  EXPECT_EQ(observer.last_started_sequence(), nullptr);
  EXPECT_EQ(observer.last_aborted_sequence(), sequence);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), sequence);

  EXPECT_TRUE(observer.AbortedEpochIsBeforeDetachedEpoch());
}

// Verifies the LayerAnimatorObserver notification order for an animation
// sequence that is queued up after another sequence that
// completes successfully.
TEST(LayerAnimatorObserverNotificationOrderTest,
     RunningASequenceThatIsQueuedForLaterStartTime) {
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(
      CreateDefaultTestAnimator(&delegate, &observer));
  observer.set_requires_notification_when_animator_destroyed(true);

  const base::TimeDelta animation_duration = base::TimeDelta::FromSeconds(100);

  LayerAnimationSequence* first_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, animation_duration));

  LayerAnimationSequence* queued_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, animation_duration));

  EXPECT_TRUE(observer.NoEventsObserved());

  animator->StartAnimation(first_sequence);

  EXPECT_EQ(observer.last_attached_sequence(), first_sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), first_sequence);
  EXPECT_EQ(observer.last_started_sequence(), first_sequence);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), nullptr);

  EXPECT_TRUE(observer.AttachedEpochIsBeforeScheduledEpoch());
  EXPECT_TRUE(observer.ScheduledEpochIsBeforeStartedEpoch());

  observer.ResetLayerAnimationObserverations();

  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);
  animator->StartAnimation(queued_sequence);

  EXPECT_EQ(observer.last_attached_sequence(), queued_sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), queued_sequence);
  EXPECT_EQ(observer.last_started_sequence(), nullptr);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), nullptr);

  EXPECT_TRUE(observer.AttachedEpochIsBeforeScheduledEpoch());

  observer.ResetLayerAnimationObserverations();

  base::TimeTicks start_time = animator->last_step_time();

  animator->Step(start_time + animation_duration);

  EXPECT_EQ(observer.last_attached_sequence(), nullptr);
  EXPECT_EQ(observer.last_scheduled_sequence(), nullptr);
  EXPECT_EQ(observer.last_started_sequence(), queued_sequence);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), first_sequence);
  EXPECT_EQ(observer.last_detached_sequence(), first_sequence);

  EXPECT_TRUE(observer.EndedEpochIsBeforeDetachedEpoch());
  EXPECT_TRUE(observer.EndedEpochIsBeforeStartedEpoch());
}

// Verifies the LayerAnimatorObserver notification order for an animation
// sequence that pre-empts another sequence.
TEST(LayerAnimatorObserverNotificationOrderTest,
     RunningASequenceThatPreEmptsAnotherSequence) {
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(
      CreateDefaultTestAnimator(&delegate, &observer));
  observer.set_requires_notification_when_animator_destroyed(true);

  const base::TimeDelta animation_duration = base::TimeDelta::FromSeconds(100);

  LayerAnimationSequence* first_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, animation_duration));

  LayerAnimationSequence* queued_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, animation_duration));

  EXPECT_TRUE(observer.NoEventsObserved());

  animator->StartAnimation(first_sequence);

  EXPECT_EQ(observer.last_attached_sequence(), first_sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), first_sequence);
  EXPECT_EQ(observer.last_started_sequence(), first_sequence);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), nullptr);

  EXPECT_TRUE(observer.AttachedEpochIsBeforeScheduledEpoch());
  EXPECT_TRUE(observer.ScheduledEpochIsBeforeStartedEpoch());

  observer.ResetLayerAnimationObserverations();

  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->StartAnimation(queued_sequence);

  EXPECT_EQ(observer.last_attached_sequence(), queued_sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), queued_sequence);
  EXPECT_EQ(observer.last_started_sequence(), queued_sequence);
  EXPECT_EQ(observer.last_aborted_sequence(), first_sequence);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), first_sequence);

  EXPECT_TRUE(observer.AbortedEpochIsBeforeDetachedEpoch());
  EXPECT_TRUE(observer.AbortedEpochIsBeforeStartedEpoch());
  EXPECT_TRUE(observer.AttachedEpochIsBeforeScheduledEpoch());
  EXPECT_TRUE(observer.ScheduledEpochIsBeforeStartedEpoch());
}

// Verifies that an observer can be attached to a LayerAnimationSequence that
// has already been scheduled.
TEST(LayerAnimatorObserverNotificationOrderTest,
     ObserverAddedAfterAnimationStarts) {
  TestLayerAnimationDelegate delegate;
  scoped_refptr<LayerAnimator> animator(CreateDefaultTestAnimator(&delegate));

  constexpr base::TimeDelta kAnimationDuration =
      base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBrightnessElement(1.0f, kAnimationDuration));
  animator->StartAnimation(sequence);

  TestLayerAnimationObserver observer;
  observer.set_requires_notification_when_animator_destroyed(true);
  animator->AddObserver(&observer);

  EXPECT_EQ(observer.last_attached_sequence(), sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), nullptr);
  EXPECT_EQ(observer.last_started_sequence(), nullptr);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), nullptr);
  EXPECT_EQ(observer.last_detached_sequence(), nullptr);

  animator->StopAnimating();

  EXPECT_EQ(observer.last_attached_sequence(), sequence);
  EXPECT_EQ(observer.last_scheduled_sequence(), nullptr);
  EXPECT_EQ(observer.last_started_sequence(), nullptr);
  EXPECT_EQ(observer.last_aborted_sequence(), nullptr);
  EXPECT_EQ(observer.last_ended_sequence(), sequence);
  EXPECT_EQ(observer.last_detached_sequence(), sequence);

  animator->RemoveObserver(&observer);
}

}  // namespace ui

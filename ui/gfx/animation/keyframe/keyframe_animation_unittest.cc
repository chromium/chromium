// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframe_effect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/test/animation_utils.h"
#include "ui/gfx/geometry/test/size_test_util.h"
#include "ui/gfx/test/gfx_util.h"

namespace gfx {

static constexpr float kNoise = 1e-6f;
static constexpr float kEpsilon = 1e-5f;

// Tests client-specific property ids.
static constexpr int kLayoutOffsetPropertId = 19;
static constexpr int kBackgroundColorPropertId = 20;
static constexpr int kOpacityPropertId = 21;
static constexpr int kBoundsPropertId = 22;
static constexpr int kTransformPropertId = 23;

class TestAnimationTarget : public SizeAnimationCurve::Target,
                            public TransformAnimationCurve::Target,
                            public FloatAnimationCurve::Target,
                            public ColorAnimationCurve::Target {
 public:
  TestAnimationTarget() {
    layout_offset_.AppendTranslate(0, 0, 0);
    operations_.AppendTranslate(0, 0, 0);
    operations_.AppendRotate(1, 0, 0, 0);
    operations_.AppendScale(1, 1, 1);
  }

  const SizeF& size() const { return size_; }
  const TransformOperations& operations() const { return operations_; }
  const TransformOperations& layout_offset() const { return layout_offset_; }
  float opacity() const { return opacity_; }
  SkColor background_color() const { return background_color_; }

  void OnSizeAnimated(const SizeF& size,
                      int target_property_id,
                      KeyframeModel* keyframe_model) override {
    size_ = size;
  }

  void OnTransformAnimated(const TransformOperations& operations,
                           int target_property_id,
                           KeyframeModel* keyframe_model) override {
    if (target_property_id == kLayoutOffsetPropertId) {
      layout_offset_ = operations;
    } else {
      operations_ = operations;
    }
  }

  void OnFloatAnimated(const float& opacity,
                       int target_property_id,
                       KeyframeModel* keyframe_model) override {
    opacity_ = opacity;
  }

  void OnColorAnimated(const SkColor& color,
                       int target_property_id,
                       KeyframeModel* keyframe_model) override {
    background_color_ = color;
  }

 private:
  TransformOperations layout_offset_;
  TransformOperations operations_;
  SizeF size_ = {10.0f, 10.0f};
  float opacity_ = 1.0f;
  SkColor background_color_ = SK_ColorRED;
};

TEST(KeyframeAnimationTest, AddRemoveKeyframeModels) {
  KeyframeEffect animator;
  EXPECT_TRUE(animator.keyframe_models().empty());
  TestAnimationTarget target;

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 1, kBoundsPropertId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertId, animator.keyframe_models()[0]->TargetProperty());

  TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  animator.AddKeyframeModel(
      CreateTransformAnimation(&target, 2, kTransformPropertId, from_operations,
                               to_operations, MicrosecondsToDelta(10000)));

  EXPECT_EQ(2ul, animator.keyframe_models().size());
  EXPECT_EQ(kTransformPropertId,
            animator.keyframe_models()[1]->TargetProperty());

  animator.AddKeyframeModel(
      CreateTransformAnimation(&target, 3, kTransformPropertId, from_operations,
                               to_operations, MicrosecondsToDelta(10000)));
  EXPECT_EQ(3ul, animator.keyframe_models().size());
  EXPECT_EQ(kTransformPropertId,
            animator.keyframe_models()[2]->TargetProperty());

  animator.RemoveKeyframeModels(kTransformPropertId);
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertId, animator.keyframe_models()[0]->TargetProperty());

  animator.RemoveKeyframeModel(animator.keyframe_models()[0]->id());
  EXPECT_TRUE(animator.keyframe_models().empty());
}

TEST(KeyframeAnimationTest, AnimationLifecycle) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 1, kBoundsPropertId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertId, animator.keyframe_models()[0]->TargetProperty());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animator.keyframe_models()[0]->run_state());

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  animator.Tick(start_time);
  EXPECT_EQ(KeyframeModel::RUNNING, animator.keyframe_models()[0]->run_state());

  EXPECT_SIZEF_EQ(SizeF(10, 100), target.size());

  // Tick beyond the animation
  animator.Tick(start_time + MicrosecondsToDelta(20000));

  EXPECT_TRUE(animator.keyframe_models().empty());

  // Should have assumed the final value.
  EXPECT_SIZEF_EQ(SizeF(20, 200), target.size());
}

TEST(KeyframeAnimationTest, AnimationQueue) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 1, kBoundsPropertId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertId, animator.keyframe_models()[0]->TargetProperty());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animator.keyframe_models()[0]->run_state());

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  animator.Tick(start_time);
  EXPECT_EQ(KeyframeModel::RUNNING, animator.keyframe_models()[0]->run_state());
  EXPECT_SIZEF_EQ(SizeF(10, 100), target.size());

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 2, kBoundsPropertId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));

  TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  animator.AddKeyframeModel(
      CreateTransformAnimation(&target, 3, kTransformPropertId, from_operations,
                               to_operations, MicrosecondsToDelta(10000)));

  EXPECT_EQ(3ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertId, animator.keyframe_models()[1]->TargetProperty());
  EXPECT_EQ(kTransformPropertId,
            animator.keyframe_models()[2]->TargetProperty());
  int id1 = animator.keyframe_models()[1]->id();

  animator.Tick(start_time + MicrosecondsToDelta(1));

  // Only the transform animation should have started (since there's no
  // conflicting animation).
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animator.keyframe_models()[1]->run_state());
  EXPECT_EQ(KeyframeModel::RUNNING, animator.keyframe_models()[2]->run_state());

  // Tick beyond the first animator. This should cause it (and the transform
  // animation) to get removed and for the second bounds animation to start.
  animator.Tick(start_time + MicrosecondsToDelta(15000));

  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(KeyframeModel::RUNNING, animator.keyframe_models()[0]->run_state());
  EXPECT_EQ(id1, animator.keyframe_models()[0]->id());

  // Tick beyond all animations. There should be none remaining.
  animator.Tick(start_time + MicrosecondsToDelta(30000));
  EXPECT_TRUE(animator.keyframe_models().empty());
}

TEST(KeyframeAnimationTest, FinishedTransition) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertId};
  transition.duration = MsToDelta(10);
  animator.set_transition(transition);

  base::TimeTicks start_time = MsToTicks(1000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.0f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId, from, to);

  animator.Tick(start_time);
  EXPECT_EQ(from, target.opacity());

  // We now simulate a long pause where the element hasn't been ticked (eg, it
  // may have been hidden). If this happens, the unticked transition must still
  // be treated as having finished.
  animator.TransitionFloatTo(&target, start_time + MsToDelta(1000),
                             kOpacityPropertId, target.opacity(), 1.0f);

  animator.Tick(start_time + MsToDelta(1000));
  EXPECT_EQ(to, target.opacity());
}

TEST(KeyframeAnimationTest, OpacityTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  float nearby = to + kNoise;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId, from,
                             nearby);
  EXPECT_EQ(keyframe_model_id, animator.keyframe_models().front()->id());

  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_GT(from, target.opacity());
  EXPECT_LT(to, target.opacity());

  animator.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_EQ(to, target.opacity());
}

TEST(KeyframeAnimationTest, ReversedOpacityTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  animator.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1000),
                             kOpacityPropertId, target.opacity(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.opacity());
}

TEST(KeyframeAnimationTest, LayoutOffsetTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kLayoutOffsetPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  TransformOperations from = target.layout_offset();

  TransformOperations to;
  to.AppendTranslate(8, 0, 0);

  animator.TransitionTransformOperationsTo(&target, start_time,
                                           kLayoutOffsetPropertId, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.layout_offset(), tolerance));
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  animator.TransitionTransformOperationsTo(
      &target, start_time, kLayoutOffsetPropertId, from, nearby);
  EXPECT_EQ(keyframe_model_id, animator.keyframe_models().front()->id());

  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.layout_offset().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.layout_offset().at(0).translate.x);

  animator.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_TRUE(to.ApproximatelyEqual(target.layout_offset(), tolerance));
}

TEST(KeyframeAnimationTest, TransformTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kTransformPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  TransformOperations from = target.operations();

  TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  animator.TransitionTransformOperationsTo(&target, start_time,
                                           kTransformPropertId, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  animator.TransitionTransformOperationsTo(&target, start_time,
                                           kTransformPropertId, from, nearby);
  EXPECT_EQ(keyframe_model_id, animator.keyframe_models().front()->id());

  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  animator.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_TRUE(to.ApproximatelyEqual(target.operations(), tolerance));
}

TEST(KeyframeAnimationTest, ReversedTransformTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kTransformPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  TransformOperations from = target.operations();

  TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  animator.TransitionTransformOperationsTo(&target, start_time,
                                           kTransformPropertId, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  TransformOperations value_before_reversing = target.operations();
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  animator.TransitionTransformOperationsTo(
      &target, start_time + MicrosecondsToDelta(1000), kTransformPropertId,
      target.operations(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_TRUE(value_before_reversing.ApproximatelyEqual(target.operations(),
                                                        tolerance));

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
}

TEST(KeyframeAnimationTest, BoundsTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kBoundsPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SizeF from = target.size();
  SizeF to(20.0f, 20.0f);

  animator.TransitionSizeTo(&target, start_time, kBoundsPropertId, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  SizeF nearby = to;
  nearby.set_width(to.width() + kNoise);
  animator.TransitionSizeTo(&target, start_time, kBoundsPropertId, from,
                            nearby);
  EXPECT_EQ(keyframe_model_id, animator.keyframe_models().front()->id());

  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  animator.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_FLOAT_SIZE_EQ(to, target.size());
}

TEST(KeyframeAnimationTest, ReversedBoundsTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kBoundsPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SizeF from = target.size();
  SizeF to(20.0f, 20.0f);

  animator.TransitionSizeTo(&target, start_time, kBoundsPropertId, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  SizeF value_before_reversing = target.size();
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  animator.TransitionSizeTo(&target, start_time + MicrosecondsToDelta(1000),
                            kBoundsPropertId, target.size(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_SIZE_EQ(value_before_reversing, target.size());

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_FLOAT_SIZE_EQ(from, target.size());
}

TEST(KeyframeAnimationTest, BackgroundColorTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kBackgroundColorPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  animator.TransitionColorTo(&target, start_time, kBackgroundColorPropertId,
                             from, to);

  EXPECT_EQ(from, target.background_color());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_GT(SkColorGetR(from), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetR(to), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetG(from), SkColorGetG(target.background_color()));
  EXPECT_GT(SkColorGetG(to), SkColorGetG(target.background_color()));
  EXPECT_EQ(0u, SkColorGetB(target.background_color()));
  EXPECT_EQ(255u, SkColorGetA(target.background_color()));

  animator.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_EQ(to, target.background_color());
}

TEST(KeyframeAnimationTest, ReversedBackgroundColorTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kBackgroundColorPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  animator.TransitionColorTo(&target, start_time, kBackgroundColorPropertId,
                             from, to);

  EXPECT_EQ(from, target.background_color());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  SkColor value_before_reversing = target.background_color();
  EXPECT_GT(SkColorGetR(from), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetR(to), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetG(from), SkColorGetG(target.background_color()));
  EXPECT_GT(SkColorGetG(to), SkColorGetG(target.background_color()));
  EXPECT_EQ(0u, SkColorGetB(target.background_color()));
  EXPECT_EQ(255u, SkColorGetA(target.background_color()));

  animator.TransitionColorTo(&target, start_time + MicrosecondsToDelta(1000),
                             kBackgroundColorPropertId,
                             target.background_color(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_EQ(value_before_reversing, target.background_color());

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.background_color());
}

TEST(KeyframeAnimationTest, DoubleReversedTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  animator.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1000),
                             kOpacityPropertId, target.opacity(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  animator.Tick(start_time + MicrosecondsToDelta(1500));
  value_before_reversing = target.opacity();
  // If the code for reversing transitions does not account for an existing time
  // offset, then reversing a second time will give incorrect values.
  animator.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1500),
                             kOpacityPropertId, target.opacity(), to);
  animator.Tick(start_time + MicrosecondsToDelta(1500));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());
}

TEST(KeyframeAnimationTest, RedundantTransition) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_redundant_transition = target.opacity();

  // While an existing transition is in progress to the same value, we should
  // not start a new transition.
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId,
                             target.opacity(), to);

  EXPECT_EQ(1lu, animator.keyframe_models().size());
  EXPECT_EQ(value_before_redundant_transition, target.opacity());
}

TEST(KeyframeAnimationTest, TransitionToSameValue) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  // Transitioning to the same value should be a no-op.
  float from = 1.0f;
  float to = 1.0f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertId, from, to);
  EXPECT_EQ(from, target.opacity());
  EXPECT_TRUE(animator.keyframe_models().empty());
}

TEST(KeyframeAnimationTest, CorrectTargetValue) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  base::TimeDelta duration = MicrosecondsToDelta(10000);

  float from_opacity = 1.0f;
  float to_opacity = 0.5f;
  SizeF from_bounds = SizeF(10, 200);
  SizeF to_bounds = SizeF(20, 200);
  SkColor from_color = SK_ColorRED;
  SkColor to_color = SK_ColorGREEN;
  TransformOperations from_transform;
  from_transform.AppendTranslate(10, 100, 1000);
  TransformOperations to_transform;
  to_transform.AppendTranslate(20, 200, 2000);

  // Verify the default value is returned if there's no running animations.
  EXPECT_EQ(from_opacity,
            animator.GetTargetFloatValue(kOpacityPropertId, from_opacity));
  EXPECT_SIZEF_EQ(from_bounds,
                  animator.GetTargetSizeValue(kBoundsPropertId, from_bounds));
  EXPECT_EQ(from_color, animator.GetTargetColorValue(kBackgroundColorPropertId,
                                                     from_color));
  EXPECT_TRUE(from_transform.ApproximatelyEqual(
      animator.GetTargetTransformOperationsValue(kTransformPropertId,
                                                 from_transform),
      kEpsilon));

  // Add keyframe_models.
  animator.AddKeyframeModel(CreateFloatAnimation(
      &target, 2, kOpacityPropertId, from_opacity, to_opacity, duration));
  animator.AddKeyframeModel(CreateSizeAnimation(
      &target, 1, kBoundsPropertId, from_bounds, to_bounds, duration));
  animator.AddKeyframeModel(CreateColorAnimation(
      &target, 3, kBackgroundColorPropertId, from_color, to_color, duration));
  animator.AddKeyframeModel(CreateTransformAnimation(
      &target, 4, kTransformPropertId, from_transform, to_transform, duration));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  // Verify target value.
  EXPECT_EQ(to_opacity,
            animator.GetTargetFloatValue(kOpacityPropertId, from_opacity));
  EXPECT_SIZEF_EQ(to_bounds,
                  animator.GetTargetSizeValue(kBoundsPropertId, from_bounds));
  EXPECT_EQ(to_color, animator.GetTargetColorValue(kBackgroundColorPropertId,
                                                   from_color));
  EXPECT_TRUE(to_transform.ApproximatelyEqual(
      animator.GetTargetTransformOperationsValue(kTransformPropertId,
                                                 from_transform),
      kEpsilon));
}

}  // namespace gfx

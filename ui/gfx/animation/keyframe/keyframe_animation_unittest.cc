// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframe_effect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/test/animation_utils.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace gfx {

static constexpr float kNoise = 1e-6f;
static constexpr float kEpsilon = 1e-5f;

// Tests client-specific property ids.
static constexpr int kLayoutOffsetPropertyId = 19;
static constexpr int kBackgroundColorPropertyId = 20;
static constexpr int kOpacityPropertyId = 21;
static constexpr int kBoundsPropertyId = 22;
static constexpr int kTransformPropertyId = 23;
static constexpr int kRectPropertyId = 24;

class TestAnimationTarget : public SizeAnimationCurve::Target,
                            public TransformAnimationCurve::Target,
                            public FloatAnimationCurve::Target,
                            public ColorAnimationCurve::Target,
                            public RectAnimationCurve::Target {
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
  Rect rect() const { return rect_; }

  void OnSizeAnimated(const SizeF& size,
                      int target_property_id,
                      KeyframeModel* keyframe_model) override {
    size_ = size;
  }

  void OnTransformAnimated(const TransformOperations& operations,
                           int target_property_id,
                           KeyframeModel* keyframe_model) override {
    if (target_property_id == kLayoutOffsetPropertyId) {
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

  void OnRectAnimated(const Rect& rect,
                      int target_property_id,
                      KeyframeModel* keyframe_model) override {
    rect_ = rect;
  }

 private:
  TransformOperations layout_offset_;
  TransformOperations operations_;
  SizeF size_ = {10.0f, 10.0f};
  float opacity_ = 1.0f;
  SkColor background_color_ = SK_ColorRED;
  Rect rect_;
};

TEST(KeyframeAnimationTest, AddRemoveKeyframeModels) {
  KeyframeEffect animator;
  EXPECT_TRUE(animator.keyframe_models().empty());
  TestAnimationTarget target;

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 1, kBoundsPropertyId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertyId, animator.keyframe_models()[0]->TargetProperty());

  TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  animator.AddKeyframeModel(CreateTransformAnimation(
      &target, 2, kTransformPropertyId, from_operations, to_operations,
      MicrosecondsToDelta(10000)));

  EXPECT_EQ(2ul, animator.keyframe_models().size());
  EXPECT_EQ(kTransformPropertyId,
            animator.keyframe_models()[1]->TargetProperty());

  animator.AddKeyframeModel(CreateTransformAnimation(
      &target, 3, kTransformPropertyId, from_operations, to_operations,
      MicrosecondsToDelta(10000)));
  EXPECT_EQ(3ul, animator.keyframe_models().size());
  EXPECT_EQ(kTransformPropertyId,
            animator.keyframe_models()[2]->TargetProperty());

  animator.RemoveKeyframeModels(kTransformPropertyId);
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertyId, animator.keyframe_models()[0]->TargetProperty());

  animator.RemoveKeyframeModel(animator.keyframe_models()[0]->id());
  EXPECT_TRUE(animator.keyframe_models().empty());
}

TEST(KeyframeAnimationTest, AnimationLifecycle) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 1, kBoundsPropertyId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertyId, animator.keyframe_models()[0]->TargetProperty());
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

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 1, kBoundsPropertyId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertyId, animator.keyframe_models()[0]->TargetProperty());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animator.keyframe_models()[0]->run_state());

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  animator.Tick(start_time);
  EXPECT_EQ(KeyframeModel::RUNNING, animator.keyframe_models()[0]->run_state());
  EXPECT_SIZEF_EQ(SizeF(10, 100), target.size());

  animator.AddKeyframeModel(CreateSizeAnimation(&target, 2, kBoundsPropertyId,
                                                SizeF(10, 100), SizeF(20, 200),
                                                MicrosecondsToDelta(10000)));

  TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  animator.AddKeyframeModel(CreateTransformAnimation(
      &target, 3, kTransformPropertyId, from_operations, to_operations,
      MicrosecondsToDelta(10000)));

  EXPECT_EQ(3ul, animator.keyframe_models().size());
  EXPECT_EQ(kBoundsPropertyId, animator.keyframe_models()[1]->TargetProperty());
  EXPECT_EQ(kTransformPropertyId,
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
  transition.target_properties = {kOpacityPropertyId};
  transition.duration = MsToDelta(10);
  animator.set_transition(transition);

  base::TimeTicks start_time = MsToTicks(1000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.0f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId, from, to);

  animator.Tick(start_time);
  EXPECT_EQ(from, target.opacity());

  // We now simulate a long pause where the element hasn't been ticked (eg, it
  // may have been hidden). If this happens, the unticked transition must still
  // be treated as having finished.
  animator.TransitionFloatTo(&target, start_time + MsToDelta(1000),
                             kOpacityPropertyId, target.opacity(), 1.0f);

  animator.Tick(start_time + MsToDelta(1000));
  EXPECT_EQ(to, target.opacity());
}

TEST(KeyframeAnimationTest, OpacityTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  float nearby = to + kNoise;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId, from,
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
  transition.target_properties = {kOpacityPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  animator.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1000),
                             kOpacityPropertyId, target.opacity(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.opacity());
}

TEST(KeyframeAnimationTest, RetargetOpacityTransition) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      gfx::KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 1.0f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(MicrosecondsToDelta(10000), 0.0f, nullptr));
  curve->set_target(&target);
  animator.AddKeyframeModel(KeyframeModel::Create(
      std::move(curve), KeyframeEffect::GetNextKeyframeModelId(),
      kOpacityPropertyId));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);
  EXPECT_EQ(1.f, target.opacity());
  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_FLOAT_EQ(0.5f, target.opacity());

  animator.GetKeyframeModel(kOpacityPropertyId)
      ->Retarget(start_time + MicrosecondsToDelta(5000), kOpacityPropertyId,
                 1.f);
  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_FLOAT_EQ(0.5f, target.opacity());

  animator.Tick(start_time + MicrosecondsToDelta(7500));
  EXPECT_FLOAT_EQ(0.75f, target.opacity());
}

TEST(KeyframeAnimationTest, RetargetTransitionBeforeLastKeyframe) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      gfx::KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 1.0f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(MicrosecondsToDelta(5000), 0.5f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(MicrosecondsToDelta(10000), 0.0f, nullptr));
  curve->set_target(&target);
  animator.AddKeyframeModel(KeyframeModel::Create(
      std::move(curve), KeyframeEffect::GetNextKeyframeModelId(),
      kOpacityPropertyId));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);
  EXPECT_EQ(1.f, target.opacity());

  animator.GetKeyframeModel(kOpacityPropertyId)
      ->Retarget(start_time + MicrosecondsToDelta(4000), kOpacityPropertyId,
                 0.1f);
  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_FLOAT_EQ(0.5f, target.opacity());

  animator.Tick(start_time + MicrosecondsToDelta(7500));
  EXPECT_FLOAT_EQ(0.3f, target.opacity());
}

TEST(KeyframeAnimationTest, LayoutOffsetTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kLayoutOffsetPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  TransformOperations from = target.layout_offset();

  TransformOperations to;
  to.AppendTranslate(8, 0, 0);

  animator.TransitionTransformOperationsTo(&target, start_time,
                                           kLayoutOffsetPropertyId, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.layout_offset(), tolerance));
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  animator.TransitionTransformOperationsTo(
      &target, start_time, kLayoutOffsetPropertyId, from, nearby);
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
  transition.target_properties = {kTransformPropertyId};
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
                                           kTransformPropertyId, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  animator.TransitionTransformOperationsTo(&target, start_time,
                                           kTransformPropertyId, from, nearby);
  EXPECT_EQ(keyframe_model_id, animator.keyframe_models().front()->id());

  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  animator.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_TRUE(
      to.Apply().ApproximatelyEqual(target.operations().Apply(), tolerance));
}

TEST(KeyframeAnimationTest, ReversedTransformTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kTransformPropertyId};
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
                                           kTransformPropertyId, from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  TransformOperations value_before_reversing = target.operations();
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  animator.TransitionTransformOperationsTo(
      &target, start_time + MicrosecondsToDelta(1000), kTransformPropertyId,
      target.operations(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_TRUE(value_before_reversing.Apply().ApproximatelyEqual(
      target.operations().Apply(), tolerance));

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_TRUE(
      from.Apply().ApproximatelyEqual(target.operations().Apply(), tolerance));
}

TEST(KeyframeAnimationTest, RetargetTransformTransition) {
  float tolerance = 0.0f;
  TestAnimationTarget target;
  KeyframeEffect animator;

  TransformOperations from;
  from.AppendScale(1, 1, 1);
  from.AppendTranslate(0, 0, 0);
  TransformOperations to;
  to.AppendScale(11, 11, 11);
  to.AppendTranslate(-10, -10, -10);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      gfx::KeyframedTransformAnimationCurve::Create());
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(
      TransformKeyframe::Create(MicrosecondsToDelta(10000), to, nullptr));
  curve->set_target(&target);
  animator.AddKeyframeModel(KeyframeModel::Create(
      std::move(curve), KeyframeEffect::GetNextKeyframeModelId(),
      kTransformPropertyId));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);
  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  animator.Tick(start_time + MicrosecondsToDelta(5000));

  EXPECT_FLOAT_EQ(6.f, target.operations().at(0).scale.x);
  EXPECT_FLOAT_EQ(-5.f, target.operations().at(1).translate.x);

  TransformOperations new_to;
  new_to.AppendScale(110, 110, 110);
  new_to.AppendTranslate(-101, -101, -101);

  animator.GetKeyframeModel(kTransformPropertyId)
      ->Retarget(start_time + MicrosecondsToDelta(5000), kTransformPropertyId,
                 new_to);
  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_FLOAT_EQ(6.f, target.operations().at(0).scale.x);
  EXPECT_FLOAT_EQ(-5.f, target.operations().at(1).translate.x);

  animator.Tick(start_time + MicrosecondsToDelta(7500));
  EXPECT_FLOAT_EQ(58.f, target.operations().at(0).scale.x);
  EXPECT_FLOAT_EQ(-53.f, target.operations().at(1).translate.x);
}

TEST(KeyframeAnimationTest, BoundsTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kBoundsPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SizeF from = target.size();
  SizeF to(20.0f, 20.0f);

  animator.TransitionSizeTo(&target, start_time, kBoundsPropertyId, from, to);

  EXPECT_SIZEF_EQ(from, target.size());
  animator.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animator.keyframe_models().front()->id();
  SizeF nearby = to;
  nearby.set_width(to.width() + kNoise);
  animator.TransitionSizeTo(&target, start_time, kBoundsPropertyId, from,
                            nearby);
  EXPECT_EQ(keyframe_model_id, animator.keyframe_models().front()->id());

  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  animator.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_SIZEF_EQ(to, target.size());
}

TEST(KeyframeAnimationTest, RetargetSizeTransition) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  SizeF from(1, 2);
  SizeF to(11, 22);

  std::unique_ptr<KeyframedSizeAnimationCurve> curve(
      gfx::KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(
      SizeKeyframe::Create(MicrosecondsToDelta(10000), to, nullptr));
  curve->set_target(&target);
  animator.AddKeyframeModel(KeyframeModel::Create(
      std::move(curve), KeyframeEffect::GetNextKeyframeModelId(),
      kBoundsPropertyId));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);
  EXPECT_EQ(from, target.size());
  animator.Tick(start_time + MicrosecondsToDelta(5000));

  EXPECT_SIZEF_EQ(SizeF(6, 12), target.size());

  SizeF new_to(600, 1200);

  animator.GetKeyframeModel(kBoundsPropertyId)
      ->Retarget(start_time + MicrosecondsToDelta(5000), kRectPropertyId,
                 new_to);
  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_SIZEF_EQ(SizeF(6, 12), target.size());

  animator.Tick(start_time + MicrosecondsToDelta(7500));
  EXPECT_SIZEF_EQ(SizeF(303, 606), target.size());
}

TEST(KeyframeAnimationTest, ReversedBoundsTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kBoundsPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SizeF from = target.size();
  SizeF to(20.0f, 20.0f);

  animator.TransitionSizeTo(&target, start_time, kBoundsPropertyId, from, to);

  EXPECT_SIZEF_EQ(from, target.size());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  SizeF value_before_reversing = target.size();
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  animator.TransitionSizeTo(&target, start_time + MicrosecondsToDelta(1000),
                            kBoundsPropertyId, target.size(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_SIZEF_EQ(value_before_reversing, target.size());

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_SIZEF_EQ(from, target.size());
}

TEST(KeyframeAnimationTest, BackgroundColorTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kBackgroundColorPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  animator.TransitionColorTo(&target, start_time, kBackgroundColorPropertyId,
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
  transition.target_properties = {kBackgroundColorPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  animator.TransitionColorTo(&target, start_time, kBackgroundColorPropertyId,
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
                             kBackgroundColorPropertyId,
                             target.background_color(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_EQ(value_before_reversing, target.background_color());

  animator.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.background_color());
}

TEST(KeyframeAnimationTest, RetargetColorTransition) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  SkColor from = SkColorSetRGB(0, 0, 0);
  SkColor to = SkColorSetRGB(10, 10, 10);

  std::unique_ptr<KeyframedColorAnimationCurve> curve(
      gfx::KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(
      ColorKeyframe::Create(MicrosecondsToDelta(10000), to, nullptr));
  curve->set_target(&target);
  animator.AddKeyframeModel(KeyframeModel::Create(
      std::move(curve), KeyframeEffect::GetNextKeyframeModelId(),
      kBackgroundColorPropertyId));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);
  EXPECT_EQ(from, target.background_color());
  animator.Tick(start_time + MicrosecondsToDelta(5000));

  EXPECT_EQ(5u, SkColorGetR(target.background_color()));

  SkColor new_to = SkColorSetRGB(101, 101, 101);

  animator.GetKeyframeModel(kBackgroundColorPropertyId)
      ->Retarget(start_time + MicrosecondsToDelta(5000), kRectPropertyId,
                 new_to);
  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_EQ(5u, SkColorGetR(target.background_color()));

  animator.Tick(start_time + MicrosecondsToDelta(7500));
  EXPECT_EQ(53u, SkColorGetR(target.background_color()));
}

TEST(KeyframeAnimationTest, DoubleReversedTransitions) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  animator.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1000),
                             kOpacityPropertyId, target.opacity(), from);
  animator.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  animator.Tick(start_time + MicrosecondsToDelta(1500));
  value_before_reversing = target.opacity();
  // If the code for reversing transitions does not account for an existing time
  // offset, then reversing a second time will give incorrect values.
  animator.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1500),
                             kOpacityPropertyId, target.opacity(), to);
  animator.Tick(start_time + MicrosecondsToDelta(1500));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());
}

TEST(KeyframeAnimationTest, RedundantTransition) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId, from, to);

  EXPECT_EQ(from, target.opacity());
  animator.Tick(start_time);

  animator.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_redundant_transition = target.opacity();

  // While an existing transition is in progress to the same value, we should
  // not start a new transition.
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId,
                             target.opacity(), to);

  EXPECT_EQ(1lu, animator.keyframe_models().size());
  EXPECT_EQ(value_before_redundant_transition, target.opacity());
}

TEST(KeyframeAnimationTest, TransitionToSameValue) {
  TestAnimationTarget target;
  KeyframeEffect animator;
  Transition transition;
  transition.target_properties = {kOpacityPropertyId};
  transition.duration = MicrosecondsToDelta(10000);
  animator.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  // Transitioning to the same value should be a no-op.
  float from = 1.0f;
  float to = 1.0f;
  animator.TransitionFloatTo(&target, start_time, kOpacityPropertyId, from, to);
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
            animator.GetTargetFloatValue(kOpacityPropertyId, from_opacity));
  EXPECT_SIZEF_EQ(from_bounds,
                  animator.GetTargetSizeValue(kBoundsPropertyId, from_bounds));
  EXPECT_EQ(from_color, animator.GetTargetColorValue(kBackgroundColorPropertyId,
                                                     from_color));
  EXPECT_TRUE(from_transform.ApproximatelyEqual(
      animator.GetTargetTransformOperationsValue(kTransformPropertyId,
                                                 from_transform),
      kEpsilon));

  // Add keyframe_models.
  animator.AddKeyframeModel(CreateFloatAnimation(
      &target, 2, kOpacityPropertyId, from_opacity, to_opacity, duration));
  animator.AddKeyframeModel(CreateSizeAnimation(
      &target, 1, kBoundsPropertyId, from_bounds, to_bounds, duration));
  animator.AddKeyframeModel(CreateColorAnimation(
      &target, 3, kBackgroundColorPropertyId, from_color, to_color, duration));
  animator.AddKeyframeModel(
      CreateTransformAnimation(&target, 4, kTransformPropertyId, from_transform,
                               to_transform, duration));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);

  // Verify target value.
  EXPECT_EQ(to_opacity,
            animator.GetTargetFloatValue(kOpacityPropertyId, from_opacity));
  EXPECT_SIZEF_EQ(to_bounds,
                  animator.GetTargetSizeValue(kBoundsPropertyId, from_bounds));
  EXPECT_EQ(to_color, animator.GetTargetColorValue(kBackgroundColorPropertyId,
                                                   from_color));
  EXPECT_TRUE(to_transform.ApproximatelyEqual(
      animator.GetTargetTransformOperationsValue(kTransformPropertyId,
                                                 from_transform),
      kEpsilon));
}

TEST(KeyframeAnimationTest, RetargetRectTransition) {
  TestAnimationTarget target;
  KeyframeEffect animator;

  Rect from(1, 2, 3, 4);
  Rect to(11, 22, 33, 44);

  std::unique_ptr<KeyframedRectAnimationCurve> curve(
      gfx::KeyframedRectAnimationCurve::Create());
  curve->AddKeyframe(RectKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(
      RectKeyframe::Create(MicrosecondsToDelta(10000), to, nullptr));
  curve->set_target(&target);
  animator.AddKeyframeModel(KeyframeModel::Create(
      std::move(curve), KeyframeEffect::GetNextKeyframeModelId(),
      kRectPropertyId));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animator.Tick(start_time);
  EXPECT_EQ(from, target.rect());
  animator.Tick(start_time + MicrosecondsToDelta(5000));

  EXPECT_EQ(Rect(6, 12, 18, 24), target.rect());

  Rect new_to(600, 1200, 1800, 2400);

  animator.GetKeyframeModel(kRectPropertyId)
      ->Retarget(start_time + MicrosecondsToDelta(5000), kRectPropertyId,
                 new_to);
  animator.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_EQ(Rect(6, 12, 18, 24), target.rect());

  animator.Tick(start_time + MicrosecondsToDelta(7500));
  EXPECT_EQ(Rect(303, 606, 909, 1212), target.rect());
}

}  // namespace gfx

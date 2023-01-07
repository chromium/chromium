// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/multi_animation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_container_element.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {

TEST(MultiAnimationTest, Basic) {
  // Create a MultiAnimation with two parts.
  MultiAnimation::Parts parts;
  parts.push_back(MultiAnimation::Part(base::Milliseconds(100), Tween::LINEAR));
  parts.push_back(
      MultiAnimation::Part(base::Milliseconds(100), Tween::EASE_OUT));

  MultiAnimation animation(parts);
  AnimationContainerElement* as_element =
      static_cast<AnimationContainerElement*>(&animation);
  as_element->SetStartTime(base::TimeTicks());

  // Step to 50, which is half way through the first part.
  as_element->Step(base::TimeTicks() + base::Milliseconds(50));
  EXPECT_EQ(.5, animation.GetCurrentValue());

  // Step to 120, which is 20% through the second part.
  as_element->Step(base::TimeTicks() + base::Milliseconds(120));
  EXPECT_DOUBLE_EQ(Tween::CalculateValue(Tween::EASE_OUT, .2),
                   animation.GetCurrentValue());

  // Step to 320, which is 20% through the second part.
  as_element->Step(base::TimeTicks() + base::Milliseconds(320));
  EXPECT_DOUBLE_EQ(Tween::CalculateValue(Tween::EASE_OUT, .2),
                   animation.GetCurrentValue());
}

// Makes sure multi-animation stops if cycles is false.
TEST(MultiAnimationTest, DontCycle) {
  MultiAnimation::Parts parts;
  parts.push_back(MultiAnimation::Part(base::Milliseconds(200), Tween::LINEAR));
  MultiAnimation animation(parts);
  AnimationContainerElement* as_element =
      static_cast<AnimationContainerElement*>(&animation);
  as_element->SetStartTime(base::TimeTicks());
  animation.set_continuous(false);

  // Step to 300, which is greater than the cycle time.
  as_element->Step(base::TimeTicks() + base::Milliseconds(300));
  EXPECT_EQ(1.0, animation.GetCurrentValue());
  EXPECT_FALSE(animation.is_animating());
}

class CurrentValueDelegate : public AnimationDelegate {
 public:
  CurrentValueDelegate() = default;

  double latest_current_value() { return latest_current_value_; }

  // AnimationDelegate overrides:
  void AnimationProgressed(const Animation* animation) override {
    latest_current_value_ = animation->GetCurrentValue();
  }

 private:
  double latest_current_value_ = 0.0;
};

// Makes sure multi-animation runs the final frame when exceeding the cycle time
// and not running continuously.
TEST(MultiAnimationTest, ExceedCycleNonContinuous) {
  MultiAnimation::Parts parts;
  parts.push_back(MultiAnimation::Part(base::Milliseconds(200), Tween::LINEAR));
  MultiAnimation animation(parts);
  CurrentValueDelegate delegate;
  animation.set_delegate(&delegate);
  animation.set_continuous(false);
  AnimationContainerElement* as_element =
      static_cast<AnimationContainerElement*>(&animation);
  as_element->SetStartTime(base::TimeTicks());

  // Step to 300, which is greater than the cycle time.
  as_element->Step(base::TimeTicks() + base::Milliseconds(300));
  EXPECT_EQ(1.0, delegate.latest_current_value());
}

// Makes sure multi-animation cycles correctly.
TEST(MultiAnimationTest, Cycle) {
  MultiAnimation::Parts parts;
  parts.push_back(MultiAnimation::Part(base::Milliseconds(200), Tween::LINEAR));
  MultiAnimation animation(parts);
  AnimationContainerElement* as_element =
      static_cast<AnimationContainerElement*>(&animation);
  as_element->SetStartTime(base::TimeTicks());

  // Step to 300, which is greater than the cycle time.
  as_element->Step(base::TimeTicks() + base::Milliseconds(300));
  EXPECT_EQ(.5, animation.GetCurrentValue());
}

// Make sure MultiAnimation::GetCurrentValue is derived from the start and end
// of the current MultiAnimation::Part.
TEST(MultiAnimationTest, GetCurrentValueDerivedFromStartAndEndOfCurrentPart) {
  // Create a MultiAnimation with two parts. The second part goes from 0.8 to
  // 0.4 instead of the default 0 -> 1.
  constexpr double kSecondPartStart = 0.8;
  constexpr double kSecondPartEnd = 0.4;
  MultiAnimation::Parts parts;
  parts.push_back(MultiAnimation::Part(base::Milliseconds(100), Tween::LINEAR));
  parts.push_back(MultiAnimation::Part(base::Milliseconds(100), Tween::EASE_OUT,
                                       kSecondPartStart, kSecondPartEnd));

  MultiAnimation animation(parts);
  animation.set_continuous(false);
  AnimationContainerElement* as_element =
      static_cast<AnimationContainerElement*>(&animation);
  as_element->SetStartTime(base::TimeTicks());

  // Step to 150, which is half way through the second part.
  as_element->Step(base::TimeTicks() + base::Milliseconds(150));
  const double current_animation_value =
      Tween::CalculateValue(Tween::EASE_OUT, .5);
  EXPECT_DOUBLE_EQ(Tween::DoubleValueBetween(current_animation_value,
                                             kSecondPartStart, kSecondPartEnd),
                   animation.GetCurrentValue());

  // Step to 200 which is at the end. The final value should now be kPartEnd as
  // the animation is not continuous.
  as_element->Step(base::TimeTicks() + base::Milliseconds(200));
  EXPECT_DOUBLE_EQ(kSecondPartEnd, animation.GetCurrentValue());
}

}  // namespace gfx

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/lottie/animation.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/paint/skottie_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/canvas.h"
#include "ui/lottie/animation_observer.h"

namespace lottie {
namespace {

// A skottie animation with solid green color for the first 2.5 seconds and then
// a solid blue color for the next 2.5 seconds.
constexpr char kData[] =
    "{"
    "  \"v\" : \"4.12.0\","
    "  \"fr\": 30,"
    "  \"w\" : 400,"
    "  \"h\" : 200,"
    "  \"ip\": 0,"
    "  \"op\": 150,"
    "  \"assets\": [],"

    "  \"layers\": ["
    "    {"
    "      \"ty\": 1,"
    "      \"sw\": 400,"
    "      \"sh\": 200,"
    "      \"sc\": \"#00ff00\","
    "      \"ip\": 0,"
    "      \"op\": 75"
    "    },"
    "    {"
    "      \"ty\": 1,"
    "      \"sw\": 400,"
    "      \"sh\": 200,"
    "      \"sc\": \"#0000ff\","
    "      \"ip\": 76,"
    "      \"op\": 150"
    "    }"
    "  ]"
    "}";
constexpr float kAnimationWidth = 400.f;
constexpr float kAnimationHeight = 200.f;
constexpr auto kAnimationDuration = base::Seconds(5);

class TestAnimationObserver : public AnimationObserver {
 public:
  TestAnimationObserver() = default;
  ~TestAnimationObserver() override = default;
  TestAnimationObserver(const TestAnimationObserver&) = delete;
  TestAnimationObserver& operator=(const TestAnimationObserver&) = delete;

  void AnimationWillStartPlaying(const Animation* animation) override {
    animation_will_start_playing_ = true;
  }

  void AnimationCycleEnded(const Animation* animation) override {
    animation_cycle_ended_ = true;
  }

  void AnimationResuming(const Animation* animation) override {
    animation_resuming_ = true;
  }

  void Reset() {
    animation_cycle_ended_ = false;
    animation_will_start_playing_ = false;
    animation_resuming_ = false;
  }

  bool animation_cycle_ended() const { return animation_cycle_ended_; }
  bool animation_will_start_playing() const {
    return animation_will_start_playing_;
  }
  bool animation_resuming() const { return animation_resuming_; }

 private:
  bool animation_cycle_ended_ = false;
  bool animation_will_start_playing_ = false;
  bool animation_resuming_ = false;
};

}  // namespace

class AnimationTest : public testing::Test {
 public:
  AnimationTest() = default;
  ~AnimationTest() override = default;
  AnimationTest(const AnimationTest&) = delete;
  AnimationTest& operator=(const AnimationTest&) = delete;

  void SetUp() override {
    canvas_ = std::make_unique<gfx::Canvas>(
        gfx::Size(kAnimationWidth, kAnimationHeight), 1.f, false);
    skottie_ = cc::SkottieWrapper::CreateNonSerializable(
        base::as_bytes(base::make_span(kData, std::strlen(kData))));
    animation_ = std::make_unique<Animation>(skottie_);
  }

  void TearDown() override { animation_.reset(nullptr); }

  gfx::Canvas* canvas() { return canvas_.get(); }

  Animation::Style GetStyle() const { return animation_->style_; }

  Animation::PlayState GetState() const { return animation_->state_; }

  bool IsStopped() const {
    return GetState() == Animation::PlayState::kStopped;
  }

  bool IsScheduledToPlay() const {
    return GetState() == Animation::PlayState::kSchedulePlay;
  }

  bool IsPlaying() const {
    return GetState() == Animation::PlayState::kPlaying;
  }

  bool IsScheduledToResume() const {
    return GetState() == Animation::PlayState::kScheduleResume;
  }

  bool HasAnimationEnded() const {
    return GetState() == Animation::PlayState::kEnded;
  }

  bool IsPaused() const { return GetState() == Animation::PlayState::kPaused; }

  const Animation::TimerControl* GetTimerControl() const {
    return animation_->timer_control_.get();
  }

  const base::TickClock* test_clock() const { return &test_clock_; }

  void AdvanceClock(base::TimeDelta advance) { test_clock_.Advance(advance); }

  base::TimeDelta TimeDeltaSince(const base::TimeTicks& ticks) const {
    return test_clock_.NowTicks() - ticks;
  }

  base::TimeTicks NowTicks() const { return test_clock_.NowTicks(); }

  double GetTimerStartOffset() const {
    return animation_->timer_control_->GetNormalizedStartOffset();
  }

  double GetTimerEndOffset() const {
    return animation_->timer_control_->GetNormalizedEndOffset();
  }

  const base::TimeTicks& GetTimerPreviousTick() const {
    return animation_->timer_control_->previous_tick_;
  }

  base::TimeDelta GetTimerTotalDuration() const {
    return animation_->timer_control_->total_duration_;
  }

  int GetTimerCycles() const {
    return animation_->timer_control_->completed_cycles();
  }

  void IsAllSameColor(SkColor color, const SkBitmap& bitmap) const {
    if (bitmap.colorType() == kBGRA_8888_SkColorType) {
      const SkColor* pixels = reinterpret_cast<SkColor*>(bitmap.getPixels());
      const int num_pixels = bitmap.width() * bitmap.height();
      for (int i = 0; i < num_pixels; i++)
        EXPECT_EQ(pixels[i], color);
    } else {
      for (int x = 0; x < bitmap.width(); x++)
        for (int y = 0; y < bitmap.height(); y++)
          EXPECT_EQ(bitmap.getColor(x, y), color);
    }
  }

 protected:
  std::unique_ptr<Animation> animation_;
  scoped_refptr<cc::SkottieWrapper> skottie_;

 private:
  std::unique_ptr<gfx::Canvas> canvas_;
  base::SimpleTestTickClock test_clock_;
};

TEST_F(AnimationTest, InitializationAndLoadingData) {
  skottie_ = cc::SkottieWrapper::CreateNonSerializable(
      base::as_bytes(base::make_span(kData, std::strlen(kData))));
  animation_ = std::make_unique<Animation>(skottie_);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().width(), kAnimationWidth);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().height(), kAnimationHeight);
  EXPECT_EQ(animation_->GetAnimationDuration(), kAnimationDuration);
  EXPECT_TRUE(IsStopped());

  skottie_ = cc::SkottieWrapper::CreateNonSerializable(
      base::as_bytes(base::make_span(kData, std::strlen(kData))));
  animation_ = std::make_unique<Animation>(skottie_);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().width(), kAnimationWidth);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().height(), kAnimationHeight);
  EXPECT_EQ(animation_->GetAnimationDuration(), kAnimationDuration);
  EXPECT_TRUE(IsStopped());
}

TEST_F(AnimationTest, PlayLinearAnimation) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  EXPECT_TRUE(IsStopped());
  animation_->Start(Animation::Style::kLinear);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 0);
  EXPECT_FLOAT_EQ(GetTimerEndOffset(), 1.f);

  EXPECT_EQ(GetTimerTotalDuration(), kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(50);
  AdvanceClock(kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kAdvance / kAnimationDuration);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), base::TimeDelta());

  // Advance the clock to the end of the animation.
  constexpr auto kAdvanceToEnd =
      kAnimationDuration - kAdvance + base::Milliseconds(1);
  AdvanceClock(kAdvanceToEnd);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvanceToEnd);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 1.f);
  EXPECT_TRUE(HasAnimationEnded());
  EXPECT_TRUE(observer.animation_cycle_ended());
}

TEST_F(AnimationTest, StopLinearAnimation) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLinear);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);

  constexpr auto kAdvance = base::Milliseconds(50);
  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kAdvance / kAnimationDuration);

  animation_->Stop();
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0.0f);
  EXPECT_TRUE(IsStopped());
}

TEST_F(AnimationTest, PlaySubsectionOfLinearAnimation) {
  constexpr auto kStartTime = base::Milliseconds(400);
  constexpr auto kDuration = base::Milliseconds(1000);

  TestAnimationObserver observer;

  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  EXPECT_FALSE(observer.animation_cycle_ended());
  animation_->StartSubsection(kStartTime, kDuration, Animation::Style::kLinear);

  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_FLOAT_EQ(GetTimerEndOffset(),
                  (kStartTime + kDuration) / kAnimationDuration);

  EXPECT_EQ(GetTimerTotalDuration(), kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(100);
  AdvanceClock(kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), base::TimeDelta());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);
  EXPECT_FALSE(observer.animation_cycle_ended());

  // Advance clock another 300 ms.
  constexpr auto kAdvance2 = base::Milliseconds(300);
  AdvanceClock(kAdvance2);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance2);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance + kAdvance2) / kAnimationDuration);
  EXPECT_FALSE(observer.animation_cycle_ended());

  // Reach the end of animation.
  constexpr auto kAdvanceToEnd =
      kDuration - kAdvance - kAdvance2 + base::Milliseconds(1);
  AdvanceClock(kAdvanceToEnd);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvanceToEnd);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerEndOffset());
  EXPECT_TRUE(observer.animation_cycle_ended());
  EXPECT_TRUE(HasAnimationEnded());
}

TEST_F(AnimationTest, PausingLinearAnimation) {
  constexpr auto kStartTime = base::Milliseconds(400);
  constexpr auto kDuration = base::Milliseconds(1000);

  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(200));

  animation_->StartSubsection(kStartTime, kDuration, Animation::Style::kLinear);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  constexpr auto kAdvance = base::Milliseconds(100);
  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  AdvanceClock(kAdvance);
  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  // Advancing clock and stepping animation should have no effect when animation
  // is paused.
  AdvanceClock(kAnimationDuration);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  // There should be no progress, since we haven't advanced the clock yet.
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance * 2) / kAnimationDuration);

  AdvanceClock(kDuration - kAdvance * 2 + base::Milliseconds(1));
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
}

TEST_F(AnimationTest, PlayLoopAnimation) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  EXPECT_TRUE(IsStopped());
  animation_->Start(Animation::Style::kLoop);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 0);
  EXPECT_FLOAT_EQ(GetTimerEndOffset(), 1.0f);

  EXPECT_EQ(GetTimerTotalDuration(), kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(50);
  AdvanceClock(kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kAdvance / kAnimationDuration);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), base::TimeDelta());

  // Advance the clock to the end of the animation.
  AdvanceClock(kAnimationDuration - kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()),
            kAnimationDuration - kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(GetTimerCycles(), 1);
  EXPECT_TRUE(std::abs(animation_->GetCurrentProgress() - 0.f) < 0.0001f);
  EXPECT_TRUE(observer.animation_cycle_ended());
  EXPECT_TRUE(IsPlaying());
}

TEST_F(AnimationTest, PlaySubsectionOfLoopAnimation) {
  constexpr auto kStartTime = base::Milliseconds(400);
  constexpr auto kDuration = base::Milliseconds(1000);

  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  EXPECT_TRUE(IsStopped());
  animation_->StartSubsection(kStartTime, kDuration, Animation::Style::kLoop);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FALSE(observer.animation_cycle_ended());
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), kStartTime / kAnimationDuration);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_FLOAT_EQ(GetTimerEndOffset(),
                  (kStartTime + kDuration) / kAnimationDuration);

  EXPECT_EQ(GetTimerTotalDuration(), kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(100);
  AdvanceClock(kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance);
  EXPECT_FALSE(observer.animation_cycle_ended());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  constexpr auto kAdvance2 = base::Milliseconds(300);
  AdvanceClock(kAdvance2);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance2);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance + kAdvance2) / kAnimationDuration);
  EXPECT_FALSE(observer.animation_cycle_ended());

  // Reach the end of animation.
  AdvanceClock(kDuration - kAdvance - kAdvance2);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()),
            kDuration - kAdvance - kAdvance2);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(observer.animation_cycle_ended());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
}

TEST_F(AnimationTest, PausingLoopAnimation) {
  constexpr auto kStartTime = base::Milliseconds(400);
  constexpr auto kDuration = base::Milliseconds(1000);

  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(200));

  animation_->StartSubsection(kStartTime, kDuration, Animation::Style::kLoop);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kStartTime / kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(100);
  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  AdvanceClock(kAdvance);
  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  // Advancing clock and stepping animation should have no effect when animation
  // is paused.
  AdvanceClock(kAnimationDuration);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  // There should be no progress, since we haven't advanced the clock yet.
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance * 2) / kAnimationDuration);
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(kDuration - kAdvance * 2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_cycle_ended());
}

TEST_F(AnimationTest, PlayThrobbingAnimation) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kThrobbing);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 0);
  EXPECT_FLOAT_EQ(GetTimerEndOffset(), 1.0f);

  EXPECT_EQ(GetTimerTotalDuration(), kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(50);
  AdvanceClock(kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kAdvance / kAnimationDuration);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), base::TimeDelta());

  // Advance the clock to the end of the animation.
  AdvanceClock(kAnimationDuration - kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()),
            kAnimationDuration - kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 1.0f);
  EXPECT_TRUE(IsPlaying());
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(kAnimationDuration / 2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0.5f);
  EXPECT_TRUE(IsPlaying());
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(kAnimationDuration / 2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_cycle_ended());
}

TEST_F(AnimationTest, PlaySubsectionOfThrobbingAnimation) {
  constexpr auto kStartTime = base::Milliseconds(400);
  constexpr auto kDuration = base::Milliseconds(1000);

  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  animation_->StartSubsection(kStartTime, kDuration,
                              Animation::Style::kThrobbing);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(GetTimerStartOffset(), kStartTime / kAnimationDuration);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_FLOAT_EQ(GetTimerEndOffset(),
                  (kStartTime + kDuration) / kAnimationDuration);

  EXPECT_EQ(GetTimerTotalDuration(), kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(100);
  AdvanceClock(kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FALSE(observer.animation_cycle_ended());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  constexpr auto kAdvance2 = base::Milliseconds(300);
  AdvanceClock(kAdvance2);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance2);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FALSE(observer.animation_cycle_ended());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), base::TimeDelta());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance + kAdvance2) / kAnimationDuration);

  // Reach the end of animation.
  AdvanceClock(kDuration - kAdvance - kAdvance2);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()),
            kDuration - kAdvance - kAdvance2);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerEndOffset());
  EXPECT_FALSE(observer.animation_cycle_ended());

  constexpr auto kAdvance3 = base::Milliseconds(500);
  AdvanceClock(kAdvance3);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kDuration - kAdvance3) / kAnimationDuration);
  EXPECT_TRUE(IsPlaying());
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(kDuration - kAdvance3);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_cycle_ended());

  observer.Reset();

  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);
  EXPECT_TRUE(IsPlaying());
}

TEST_F(AnimationTest, PausingThrobbingAnimation) {
  constexpr auto kStartTime = base::Milliseconds(400);
  constexpr auto kDuration = base::Milliseconds(1000);

  AdvanceClock(base::Milliseconds(200));

  animation_->StartSubsection(kStartTime, kDuration,
                              Animation::Style::kThrobbing);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_TRUE(IsPlaying());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kStartTime / kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(100);
  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  AdvanceClock(kAdvance);
  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  // Advancing clock and stepping animation should have no effect when animation
  // is paused.
  AdvanceClock(kAnimationDuration);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  // There should be no progress, since we haven't advanced the clock yet.
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);

  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance * 2) / kAnimationDuration);

  AdvanceClock(kDuration - kAdvance * 2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerEndOffset());
  EXPECT_TRUE(IsPlaying());

  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kDuration - kAdvance) / kAnimationDuration);
  EXPECT_TRUE(IsPlaying());

  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  AdvanceClock(kAnimationDuration * 2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kDuration - kAdvance) / kAnimationDuration);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());

  constexpr auto kAdvance2 = base::Milliseconds(500);
  AdvanceClock(kAdvance2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(
      animation_->GetCurrentProgress(),
      (kStartTime + kDuration - kAdvance - kAdvance2) / kAnimationDuration);

  AdvanceClock(kDuration - kAdvance - kAdvance2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());

  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (kStartTime + kAdvance) / kAnimationDuration);
  EXPECT_TRUE(IsPlaying());
}

// Test to see if the race condition is handled correctly. It may happen that we
// pause the video before it even starts playing.
TEST_F(AnimationTest, PauseBeforePlay) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(base::Milliseconds(300));

  animation_->Start();
  EXPECT_TRUE(IsScheduledToPlay());

  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  AdvanceClock(base::Milliseconds(100));
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  AdvanceClock(base::Milliseconds(100));
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());

  constexpr auto kAdvance = base::Milliseconds(100);
  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kAdvance / kAnimationDuration);
}

TEST_F(AnimationTest, PaintTest) {
  gfx::Canvas canvas(gfx::Size(kAnimationWidth, kAnimationHeight), 1.f, false);

  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLinear);
  animation_->Paint(&canvas, NowTicks(), animation_->GetOriginalSize());

  AdvanceClock(base::Milliseconds(50));
  animation_->Paint(&canvas, NowTicks(), animation_->GetOriginalSize());
  IsAllSameColor(SK_ColorGREEN, canvas.GetBitmap());

  AdvanceClock(base::Milliseconds(2450));
  animation_->Paint(&canvas, NowTicks(), animation_->GetOriginalSize());
  IsAllSameColor(SK_ColorGREEN, canvas.GetBitmap());

  AdvanceClock(base::Milliseconds(50));
  animation_->Paint(&canvas, NowTicks(), animation_->GetOriginalSize());
  IsAllSameColor(SK_ColorBLUE, canvas.GetBitmap());

  AdvanceClock(base::Milliseconds(1000));
  animation_->Paint(&canvas, NowTicks(), animation_->GetOriginalSize());
  IsAllSameColor(SK_ColorBLUE, canvas.GetBitmap());

  AdvanceClock(base::Milliseconds(1400));
  animation_->Paint(&canvas, NowTicks(), animation_->GetOriginalSize());
  IsAllSameColor(SK_ColorBLUE, canvas.GetBitmap());
}

}  // namespace lottie

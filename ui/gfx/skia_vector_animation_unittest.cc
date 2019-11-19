// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_vector_animation.h"

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/paint/skottie_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_vector_animation_observer.h"

namespace gfx {
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
constexpr float kAnimationDuration = 5.f;

class TestAnimationObserver : public SkiaVectorAnimationObserver {
 public:
  TestAnimationObserver() = default;

  void AnimationWillStartPlaying(
      const SkiaVectorAnimation* animation) override {
    animation_will_start_playing_ = true;
  }

  void AnimationCycleEnded(const SkiaVectorAnimation* animation) override {
    animation_cycle_ended_ = true;
  }

  void AnimationResuming(const SkiaVectorAnimation* animation) override {
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

  DISALLOW_COPY_AND_ASSIGN(TestAnimationObserver);
};

}  // namespace

class SkiaVectorAnimationTest : public testing::Test {
 public:
  SkiaVectorAnimationTest() = default;
  ~SkiaVectorAnimationTest() override {}

  void SetUp() override {
    canvas_ = std::make_unique<gfx::Canvas>(
        gfx::Size(kAnimationWidth, kAnimationHeight), 1.f, false);
    skottie_ = base::MakeRefCounted<cc::SkottieWrapper>(
        std::make_unique<SkMemoryStream>(kData, std::strlen(kData)));
    animation_ = std::make_unique<SkiaVectorAnimation>(skottie_);
  }

  void TearDown() override { animation_.reset(nullptr); }

  Canvas* canvas() { return canvas_.get(); }

  SkiaVectorAnimation::Style GetStyle() const { return animation_->style_; }

  SkiaVectorAnimation::PlayState GetState() const { return animation_->state_; }

  bool IsStopped() const {
    return GetState() == SkiaVectorAnimation::PlayState::kStopped;
  }

  bool IsScheduledToPlay() const {
    return GetState() == SkiaVectorAnimation::PlayState::kSchedulePlay;
  }

  bool IsPlaying() const {
    return GetState() == SkiaVectorAnimation::PlayState::kPlaying;
  }

  bool IsScheduledToResume() const {
    return GetState() == SkiaVectorAnimation::PlayState::kScheduleResume;
  }

  bool HasAnimationEnded() const {
    return GetState() == SkiaVectorAnimation::PlayState::kEnded;
  }

  bool IsPaused() const {
    return GetState() == SkiaVectorAnimation::PlayState::kPaused;
  }

  const SkiaVectorAnimation::TimerControl* GetTimerControl() const {
    return animation_->timer_control_.get();
  }

  const base::TickClock* test_clock() const { return &test_clock_; }

  void AdvanceClock(int64_t ms) {
    test_clock_.Advance(base::TimeDelta::FromMilliseconds(ms));
  }

  base::TimeDelta TimeDeltaSince(const base::TimeTicks& ticks) const {
    return test_clock_.NowTicks() - ticks;
  }

  const base::TimeTicks NowTicks() const { return test_clock_.NowTicks(); }

  double GetTimerStartOffset() const {
    return animation_->timer_control_->GetNormalizedStartOffset();
  }

  double GetTimerEndOffset() const {
    return animation_->timer_control_->GetNormalizedEndOffset();
  }

  const base::TimeTicks& GetTimerPreviousTick() const {
    return animation_->timer_control_->previous_tick_;
  }

  double GetTimerProgressPerMs() const {
    return animation_->timer_control_->progress_per_millisecond_;
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
  std::unique_ptr<SkiaVectorAnimation> animation_;
  scoped_refptr<cc::SkottieWrapper> skottie_;

 private:
  std::unique_ptr<gfx::Canvas> canvas_;
  base::SimpleTestTickClock test_clock_;

  DISALLOW_COPY_AND_ASSIGN(SkiaVectorAnimationTest);
};

TEST_F(SkiaVectorAnimationTest, InitializationAndLoadingData) {
  auto bytes = base::MakeRefCounted<base::RefCountedBytes>(
      std::vector<unsigned char>(kData, kData + std::strlen(kData)));
  skottie_ = base::MakeRefCounted<cc::SkottieWrapper>(bytes.get());
  animation_ = std::make_unique<SkiaVectorAnimation>(skottie_);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().width(), kAnimationWidth);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().height(), kAnimationHeight);
  EXPECT_FLOAT_EQ(animation_->GetAnimationDuration().InSecondsF(),
                  kAnimationDuration);
  EXPECT_TRUE(IsStopped());

  skottie_ = base::MakeRefCounted<cc::SkottieWrapper>(
      std::make_unique<SkMemoryStream>(kData, std::strlen(kData)));
  animation_ = std::make_unique<SkiaVectorAnimation>(skottie_);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().width(), kAnimationWidth);
  EXPECT_FLOAT_EQ(animation_->GetOriginalSize().height(), kAnimationHeight);
  EXPECT_FLOAT_EQ(animation_->GetAnimationDuration().InSecondsF(),
                  kAnimationDuration);
  EXPECT_TRUE(IsStopped());
}

TEST_F(SkiaVectorAnimationTest, PlayLinearAnimation) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  // Advance clock by 300 milliseconds.
  AdvanceClock(300);

  EXPECT_TRUE(IsStopped());
  animation_->Start(SkiaVectorAnimation::Style::kLinear);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 0);
  EXPECT_FLOAT_EQ(GetTimerEndOffset(), 1.f);

  EXPECT_FLOAT_EQ(GetTimerProgressPerMs(), 1.f / 5000.f);

  AdvanceClock(50);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 50);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 50.f / 5000.f);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);

  // Advance the clock to the end of the animation.
  AdvanceClock(4951);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 4951);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 1.f);
  EXPECT_TRUE(HasAnimationEnded());
  EXPECT_TRUE(observer.animation_cycle_ended());
}

TEST_F(SkiaVectorAnimationTest, StopLinearAnimation) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  // Advance clock by 300 milliseconds.
  AdvanceClock(300);

  animation_->Start(SkiaVectorAnimation::Style::kLinear);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);

  AdvanceClock(50);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 50.f / 5000.f);

  animation_->Stop();
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0.f);
  EXPECT_TRUE(IsStopped());
}

TEST_F(SkiaVectorAnimationTest, PlaySubsectionOfLinearAnimation) {
  const int start_time_ms = 400;
  const int duration_ms = 1000;
  const float total_duration_ms = kAnimationDuration * 1000.f;

  TestAnimationObserver observer;

  animation_->SetAnimationObserver(&observer);

  // Advance clock by 300 milliseconds.
  AdvanceClock(300);

  EXPECT_FALSE(observer.animation_cycle_ended());
  animation_->StartSubsection(base::TimeDelta::FromMilliseconds(start_time_ms),
                              base::TimeDelta::FromMilliseconds(duration_ms),
                              SkiaVectorAnimation::Style::kLinear);

  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_FLOAT_EQ(GetTimerEndOffset(),
                  (start_time_ms + duration_ms) / total_duration_ms);

  EXPECT_FLOAT_EQ(GetTimerProgressPerMs(), 1.f / total_duration_ms);

  AdvanceClock(100);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 100);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (100.f + start_time_ms) / total_duration_ms);
  EXPECT_FALSE(observer.animation_cycle_ended());

  // Advance clock another 300 ms.
  AdvanceClock(300);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 300);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (100.f + 300.f + start_time_ms) / total_duration_ms);
  EXPECT_FALSE(observer.animation_cycle_ended());

  // Reach the end of animation.
  AdvanceClock(601);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 601);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerEndOffset());
  EXPECT_TRUE(observer.animation_cycle_ended());
  EXPECT_TRUE(HasAnimationEnded());
}

TEST_F(SkiaVectorAnimationTest, PausingLinearAnimation) {
  const int start_time_ms = 400;
  const int duration_ms = 1000;
  const float total_duration_ms = kAnimationDuration * 1000.f;
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(200);

  animation_->StartSubsection(base::TimeDelta::FromMilliseconds(start_time_ms),
                              base::TimeDelta::FromMilliseconds(duration_ms),
                              SkiaVectorAnimation::Style::kLinear);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 500.f / total_duration_ms);

  AdvanceClock(100);
  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  // Advancing clock and stepping animation should have no effect when animation
  // is paused.
  AdvanceClock(5000);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 500.f / total_duration_ms);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  // There should be no progress, since we haven't advanced the clock yet.
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 500.f / total_duration_ms);

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 600.f / total_duration_ms);

  AdvanceClock(801);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
}

TEST_F(SkiaVectorAnimationTest, PlayLoopAnimation) {
  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  // Advance clock by 300 milliseconds.
  AdvanceClock(300);

  EXPECT_TRUE(IsStopped());
  animation_->Start(SkiaVectorAnimation::Style::kLoop);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 0);
  EXPECT_FLOAT_EQ(GetTimerEndOffset(), 1.f);

  EXPECT_FLOAT_EQ(GetTimerProgressPerMs(), 1.f / 5000.f);

  AdvanceClock(50);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 50);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 50.f / 5000.f);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);

  // Advance the clock to the end of the animation.
  AdvanceClock(4950);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 4950);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(GetTimerCycles(), 1);
  EXPECT_TRUE(std::abs(animation_->GetCurrentProgress() - 0.f) < 0.0001f);
  EXPECT_TRUE(observer.animation_cycle_ended());
  EXPECT_TRUE(IsPlaying());
}

TEST_F(SkiaVectorAnimationTest, PlaySubsectionOfLoopAnimation) {
  const int start_time_ms = 400;
  const int duration_ms = 1000;
  const float total_duration_ms = kAnimationDuration * 1000.f;


  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  // Advance clock by 300 milliseconds.
  AdvanceClock(300);
  EXPECT_TRUE(IsStopped());
  animation_->StartSubsection(base::TimeDelta::FromMilliseconds(start_time_ms),
                              base::TimeDelta::FromMilliseconds(duration_ms),
                              SkiaVectorAnimation::Style::kLoop);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FALSE(observer.animation_cycle_ended());
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 400.f / total_duration_ms);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_FLOAT_EQ(GetTimerEndOffset(),
                  (start_time_ms + duration_ms) / total_duration_ms);

  EXPECT_FLOAT_EQ(GetTimerProgressPerMs(), 1.f / total_duration_ms);

  AdvanceClock(100);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 100);
  EXPECT_FALSE(observer.animation_cycle_ended());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (100.f + start_time_ms) / total_duration_ms);

  // Advance clock another 300 ms.
  AdvanceClock(300);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 300);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (100.f + 300.f + start_time_ms) / total_duration_ms);
  EXPECT_FALSE(observer.animation_cycle_ended());

  // Reach the end of animation.
  AdvanceClock(600);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 600);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(observer.animation_cycle_ended());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
}

TEST_F(SkiaVectorAnimationTest, PausingLoopAnimation) {
  const int start_time_ms = 400;
  const int duration_ms = 1000;
  const float total_duration_ms = kAnimationDuration * 1000.f;

  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(200);

  animation_->StartSubsection(base::TimeDelta::FromMilliseconds(start_time_ms),
                              base::TimeDelta::FromMilliseconds(duration_ms),
                              SkiaVectorAnimation::Style::kLoop);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 400.f / total_duration_ms);

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 500.f / total_duration_ms);

  AdvanceClock(100);
  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  // Advancing clock and stepping animation should have no effect when animation
  // is paused.
  AdvanceClock(5000);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 500.f / total_duration_ms);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  // There should be no progress, since we haven't advanced the clock yet.
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 500.f / total_duration_ms);

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 600.f / total_duration_ms);
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(800);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_cycle_ended());
}

TEST_F(SkiaVectorAnimationTest, PlayThrobbingAnimation) {

  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);
  // Advance clock by 300 milliseconds.
  AdvanceClock(300);

  animation_->Start(SkiaVectorAnimation::Style::kThrobbing);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 0);
  EXPECT_FLOAT_EQ(GetTimerEndOffset(), 1.f);

  EXPECT_FLOAT_EQ(GetTimerProgressPerMs(), 1.f / 5000.f);

  AdvanceClock(50);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 50);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 50.f / 5000.f);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);

  // Advance the clock to the end of the animation.
  AdvanceClock(4950);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 4950);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 1.f);
  EXPECT_TRUE(IsPlaying());
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(2500);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0.5f);
  EXPECT_TRUE(IsPlaying());
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(2500);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_cycle_ended());
}

TEST_F(SkiaVectorAnimationTest, PlaySubsectionOfThrobbingAnimation) {
  const int start_time_ms = 400;
  const int duration_ms = 1000;
  const float total_duration_ms = kAnimationDuration * 1000.f;


  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  // Advance clock by 300 milliseconds.
  AdvanceClock(300);

  animation_->StartSubsection(base::TimeDelta::FromMilliseconds(start_time_ms),
                              base::TimeDelta::FromMilliseconds(duration_ms),
                              SkiaVectorAnimation::Style::kThrobbing);
  EXPECT_TRUE(IsScheduledToPlay());
  EXPECT_FALSE(observer.animation_will_start_playing());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FALSE(IsScheduledToPlay());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_will_start_playing());

  EXPECT_FLOAT_EQ(GetTimerStartOffset(), 400.f / total_duration_ms);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_FLOAT_EQ(GetTimerEndOffset(),
                  (start_time_ms + duration_ms) / total_duration_ms);

  EXPECT_FLOAT_EQ(GetTimerProgressPerMs(), 1.f / total_duration_ms);

  AdvanceClock(100);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 100);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FALSE(observer.animation_cycle_ended());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (100.f + start_time_ms) / total_duration_ms);

  // Advance clock another 300 ms.
  AdvanceClock(300);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 300);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FALSE(observer.animation_cycle_ended());
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 0);
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (100.f + 300.f + start_time_ms) / total_duration_ms);

  // Reach the end of animation.
  AdvanceClock(600);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()).InMilliseconds(), 600);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerEndOffset());
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(500);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 900.f / total_duration_ms);
  EXPECT_TRUE(IsPlaying());
  EXPECT_FALSE(observer.animation_cycle_ended());

  AdvanceClock(500);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());
  EXPECT_TRUE(IsPlaying());
  EXPECT_TRUE(observer.animation_cycle_ended());

  observer.Reset();

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 100.f) / total_duration_ms);
  EXPECT_TRUE(IsPlaying());
}

TEST_F(SkiaVectorAnimationTest, PausingThrobbingAnimation) {
  const int start_time_ms = 400;
  const int duration_ms = 1000;
  const float total_duration_ms = kAnimationDuration * 1000.f;

  AdvanceClock(200);

  animation_->StartSubsection(base::TimeDelta::FromMilliseconds(start_time_ms),
                              base::TimeDelta::FromMilliseconds(duration_ms),
                              SkiaVectorAnimation::Style::kThrobbing);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_TRUE(IsPlaying());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  start_time_ms / total_duration_ms);

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 100.f) / total_duration_ms);

  AdvanceClock(100);
  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  // Advancing clock and stepping animation should have no effect when animation
  // is paused.
  AdvanceClock(5000);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 100.f) / total_duration_ms);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  // There should be no progress, since we haven't advanced the clock yet.
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 100.f) / total_duration_ms);

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 200.f) / total_duration_ms);

  AdvanceClock(800);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerEndOffset());
  EXPECT_TRUE(IsPlaying());

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 900.f) / total_duration_ms);
  EXPECT_TRUE(IsPlaying());

  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  AdvanceClock(10000);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 900.f) / total_duration_ms);

  // Resume playing the animation.
  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());

  AdvanceClock(500);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 400.f) / total_duration_ms);

  AdvanceClock(400);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), GetTimerStartOffset());

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  (start_time_ms + 100.f) / total_duration_ms);
  EXPECT_TRUE(IsPlaying());
}

TEST_F(SkiaVectorAnimationTest, PauseBeforePlay) {
  const float total_duration_ms = kAnimationDuration * 1000.f;

  // Test to see if the race condition is handled correctly. It may happen that
  // we pause the video before it even starts playing.

  TestAnimationObserver observer;
  animation_->SetAnimationObserver(&observer);

  AdvanceClock(300);

  animation_->Start();
  EXPECT_TRUE(IsScheduledToPlay());

  animation_->Pause();
  EXPECT_TRUE(IsPaused());

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  animation_->ResumePlaying();
  EXPECT_TRUE(IsScheduledToResume());

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_TRUE(IsPlaying());

  AdvanceClock(100);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 100.f / total_duration_ms);
}

TEST_F(SkiaVectorAnimationTest, PaintTest) {
  std::unique_ptr<gfx::Canvas> canvas(new gfx::Canvas(
      gfx::Size(kAnimationWidth, kAnimationHeight), 1.f, false));

  // Advance clock by 300 milliseconds.
  AdvanceClock(300);

  animation_->Start(SkiaVectorAnimation::Style::kLinear);
  animation_->Paint(canvas.get(), NowTicks(), animation_->GetOriginalSize());

  AdvanceClock(50);
  animation_->Paint(canvas.get(), NowTicks(), animation_->GetOriginalSize());
  SkBitmap bitmap = canvas->GetBitmap();
  IsAllSameColor(SK_ColorGREEN, bitmap);

  AdvanceClock(2450);
  animation_->Paint(canvas.get(), NowTicks(), animation_->GetOriginalSize());
  bitmap = canvas->GetBitmap();
  IsAllSameColor(SK_ColorGREEN, bitmap);

  AdvanceClock(50);
  animation_->Paint(canvas.get(), NowTicks(), animation_->GetOriginalSize());
  bitmap = canvas->GetBitmap();
  IsAllSameColor(SK_ColorBLUE, bitmap);

  AdvanceClock(1000);
  animation_->Paint(canvas.get(), NowTicks(), animation_->GetOriginalSize());
  bitmap = canvas->GetBitmap();
  IsAllSameColor(SK_ColorBLUE, bitmap);

  AdvanceClock(1400);
  animation_->Paint(canvas.get(), NowTicks(), animation_->GetOriginalSize());
  bitmap = canvas->GetBitmap();
  IsAllSameColor(SK_ColorBLUE, bitmap);
}

}  // namespace gfx

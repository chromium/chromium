// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/lottie/animation.h"

#include <map>
#include <string>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_frame_data_provider.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/lottie/animation_observer.h"

namespace lottie {
namespace {

using ::testing::Eq;
using ::testing::FloatEq;
using ::testing::FloatNear;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

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
constexpr float kCanvasImageScale = 2.f;
constexpr float kFrameTimestampToleranceSec = 0.1f;

class TestAnimationObserver : public AnimationObserver {
 public:
  explicit TestAnimationObserver(Animation* animation) {
    observation_.Observe(animation);
  }
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
  base::ScopedObservation<Animation, AnimationObserver> observation_{this};
  bool animation_cycle_ended_ = false;
  bool animation_will_start_playing_ = false;
  bool animation_resuming_ = false;
};

class TestSkottieFrameDataProvider : public cc::SkottieFrameDataProvider {
 public:
  class ImageAssetImpl : public cc::SkottieFrameDataProvider::ImageAsset {
   public:
    ImageAssetImpl() = default;
    ImageAssetImpl(const ImageAssetImpl& other) = delete;
    ImageAssetImpl& operator=(const ImageAssetImpl& other) = delete;

    cc::SkottieFrameData GetFrameData(float t, float scale_factor) override {
      last_frame_t_ = t;
      last_frame_scale_factor_ = scale_factor;
      return current_frame_data_;
    }

    void set_current_frame_data(cc::SkottieFrameData current_frame_data) {
      current_frame_data_ = std::move(current_frame_data);
    }

    const absl::optional<float>& last_frame_t() const { return last_frame_t_; }
    const absl::optional<float>& last_frame_scale_factor() const {
      return last_frame_scale_factor_;
    }

   private:
    friend class TestSkottieFrameDataProvider;

    ~ImageAssetImpl() override = default;

    cc::SkottieFrameData current_frame_data_;
    absl::optional<float> last_frame_t_;
    absl::optional<float> last_frame_scale_factor_;
  };

  TestSkottieFrameDataProvider() = default;
  TestSkottieFrameDataProvider(const TestSkottieFrameDataProvider&) = delete;
  TestSkottieFrameDataProvider& operator=(const TestSkottieFrameDataProvider&) =
      delete;
  ~TestSkottieFrameDataProvider() override = default;

  scoped_refptr<ImageAsset> LoadImageAsset(
      base::StringPiece resource_id,
      const base::FilePath& resource_path,
      const absl::optional<gfx::Size>& size) override {
    auto new_asset = base::MakeRefCounted<ImageAssetImpl>();
    CHECK(current_assets_.emplace(std::string(resource_id), new_asset).second);
    return new_asset;
  }

  ImageAssetImpl* GetLoadedImageAsset(const std::string& resource_id) {
    auto iter = current_assets_.find(resource_id);
    return iter == current_assets_.end() ? nullptr : iter->second.get();
  }

 private:
  std::map<std::string, scoped_refptr<ImageAssetImpl>> current_assets_;
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
  std::unique_ptr<gfx::Canvas> canvas_;
  std::unique_ptr<Animation> animation_;
  scoped_refptr<cc::SkottieWrapper> skottie_;

 private:
  base::SimpleTestTickClock test_clock_;
};

class AnimationWithImageAssetsTest : public AnimationTest {
 protected:
  AnimationWithImageAssetsTest()
      : display_list_(base::MakeRefCounted<cc::DisplayItemList>(
            cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)),
        record_canvas_(display_list_.get(),
                       SkRect::MakeIWH(cc::kLottieDataWith2AssetsWidth,
                                       cc::kLottieDataWith2AssetsHeight)) {}

  void SetUp() override {
    canvas_ = std::make_unique<gfx::Canvas>(&record_canvas_, kCanvasImageScale);
    skottie_ = cc::CreateSkottieFromString(cc::kLottieDataWith2Assets);
    animation_ = std::make_unique<Animation>(skottie_, cc::SkottieColorMap(),
                                             &frame_data_provider_);
    asset_0_ = frame_data_provider_.GetLoadedImageAsset("image_0");
    asset_1_ = frame_data_provider_.GetLoadedImageAsset("image_1");
    ASSERT_THAT(asset_0_, NotNull());
    ASSERT_THAT(asset_1_, NotNull());
  }

  cc::SkottieFrameData CreateHighQualityTestFrameData() {
    return {
        .image = cc::CreateDiscardablePaintImage(animation_->GetOriginalSize()),
        .quality = cc::PaintFlags::FilterQuality::kHigh};
  }

  const scoped_refptr<cc::DisplayItemList> display_list_;
  cc::RecordPaintCanvas record_canvas_;
  TestSkottieFrameDataProvider frame_data_provider_;
  raw_ptr<TestSkottieFrameDataProvider::ImageAssetImpl> asset_0_;
  raw_ptr<TestSkottieFrameDataProvider::ImageAssetImpl> asset_1_;
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
  TestAnimationObserver observer(animation_.get());

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
  IsAllSameColor(SK_ColorGREEN, canvas()->GetBitmap());

  EXPECT_EQ(GetTimerTotalDuration(), kAnimationDuration);

  constexpr auto kAdvance = base::Milliseconds(50);
  AdvanceClock(kAdvance);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvance);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kAdvance / kAnimationDuration);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), base::TimeDelta());
  IsAllSameColor(SK_ColorGREEN, canvas()->GetBitmap());

  // Advance the clock to the end of the animation.
  constexpr auto kAdvanceToEnd =
      kAnimationDuration - kAdvance + base::Milliseconds(1);
  AdvanceClock(kAdvanceToEnd);
  EXPECT_EQ(TimeDeltaSince(GetTimerPreviousTick()), kAdvanceToEnd);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 1.f);
  EXPECT_TRUE(HasAnimationEnded());
  EXPECT_TRUE(observer.animation_cycle_ended());
  IsAllSameColor(SK_ColorBLUE, canvas()->GetBitmap());
}

TEST_F(AnimationTest, StopLinearAnimation) {
  TestAnimationObserver observer(animation_.get());

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

  TestAnimationObserver observer(animation_.get());

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

  TestAnimationObserver observer(animation_.get());

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
  TestAnimationObserver observer(animation_.get());

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

  TestAnimationObserver observer(animation_.get());

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

  TestAnimationObserver observer(animation_.get());

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
  TestAnimationObserver observer(animation_.get());

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

  TestAnimationObserver observer(animation_.get());

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
  TestAnimationObserver observer(animation_.get());

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

TEST_F(AnimationWithImageAssetsTest, PaintsAnimationImagesToCanvas) {
  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLoop);

  TestSkottieFrameDataProvider::ImageAssetImpl* asset_0 =
      frame_data_provider_.GetLoadedImageAsset("image_0");
  TestSkottieFrameDataProvider::ImageAssetImpl* asset_1 =
      frame_data_provider_.GetLoadedImageAsset("image_1");
  ASSERT_THAT(asset_0, NotNull());
  ASSERT_THAT(asset_1, NotNull());

  cc::SkottieFrameData frame_0 = CreateHighQualityTestFrameData();
  cc::SkottieFrameData frame_1 = CreateHighQualityTestFrameData();
  asset_0->set_current_frame_data(frame_0);
  asset_1->set_current_frame_data(frame_1);

  display_list_->StartPaint();
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  display_list_->EndPaintOfUnpaired(gfx::Rect(animation_->GetOriginalSize()));

  sk_sp<cc::PaintRecord> paint_record = display_list_->ReleaseAsRecord();
  ASSERT_THAT(paint_record, NotNull());
  ASSERT_THAT(paint_record->size(), Eq(1u));
  const cc::DrawSkottieOp* op =
      paint_record->GetOpAtForTesting<cc::DrawSkottieOp>(0);
  ASSERT_THAT(op, NotNull());
  EXPECT_THAT(op->images, UnorderedElementsAre(Pair(
                              cc::HashSkottieResourceId("image_0"), frame_0)));

  AdvanceClock(animation_->GetAnimationDuration() * .75);

  display_list_->StartPaint();
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  display_list_->EndPaintOfUnpaired(gfx::Rect(animation_->GetOriginalSize()));

  paint_record = display_list_->ReleaseAsRecord();
  ASSERT_THAT(paint_record, NotNull());
  ASSERT_THAT(paint_record->size(), Eq(1u));
  op = paint_record->GetOpAtForTesting<cc::DrawSkottieOp>(0);
  ASSERT_THAT(op, NotNull());
  EXPECT_THAT(op->images, UnorderedElementsAre(Pair(
                              cc::HashSkottieResourceId("image_1"), frame_1)));
}

TEST_F(AnimationWithImageAssetsTest, GracefullyHandlesNullImages) {
  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLoop);

  display_list_->StartPaint();
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  display_list_->EndPaintOfUnpaired(gfx::Rect(animation_->GetOriginalSize()));

  sk_sp<cc::PaintRecord> paint_record = display_list_->ReleaseAsRecord();
  ASSERT_THAT(paint_record, NotNull());
  ASSERT_THAT(paint_record->size(), Eq(1u));
  const cc::DrawSkottieOp* op =
      paint_record->GetOpAtForTesting<cc::DrawSkottieOp>(0);
  ASSERT_THAT(op, NotNull());
  EXPECT_THAT(op->images,
              UnorderedElementsAre(Pair(cc::HashSkottieResourceId("image_0"),
                                        cc::SkottieFrameData())));
}

TEST_F(AnimationWithImageAssetsTest, LoadsCorrectFrameTimestamp) {
  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLoop);

  asset_0_->set_current_frame_data(CreateHighQualityTestFrameData());
  asset_1_->set_current_frame_data(CreateHighQualityTestFrameData());

  display_list_->StartPaint();
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  display_list_->EndPaintOfUnpaired(gfx::Rect(animation_->GetOriginalSize()));

  ASSERT_TRUE(asset_0_->last_frame_t().has_value());
  EXPECT_THAT(asset_0_->last_frame_t().value(), FloatEq(0));

  base::TimeDelta three_quarter_duration =
      animation_->GetAnimationDuration() * .75;
  AdvanceClock(three_quarter_duration);

  display_list_->StartPaint();
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  display_list_->EndPaintOfUnpaired(gfx::Rect(animation_->GetOriginalSize()));

  // The timestamp is "relative to the image layer timeline origin" (see
  // SkResources.h). The test animation used in this case has 2 layers for the
  // first and second halves of the animation. So the 3/4 point of the animation
  // is half way into the second layer, or 1/4 the duration of the whole
  // animation.
  base::TimeDelta half_duration = animation_->GetAnimationDuration() * .5;
  ASSERT_TRUE(asset_1_->last_frame_t().has_value());
  EXPECT_THAT(asset_1_->last_frame_t().value(),
              FloatNear((three_quarter_duration - half_duration).InSecondsF(),
                        kFrameTimestampToleranceSec));

  AdvanceClock(half_duration);

  display_list_->StartPaint();
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  display_list_->EndPaintOfUnpaired(gfx::Rect(animation_->GetOriginalSize()));

  base::TimeDelta quarter_duration = animation_->GetAnimationDuration() / 4;
  ASSERT_TRUE(asset_0_->last_frame_t().has_value());
  EXPECT_THAT(
      asset_0_->last_frame_t().value(),
      FloatNear(quarter_duration.InSecondsF(), kFrameTimestampToleranceSec));
}

TEST_F(AnimationWithImageAssetsTest, LoadsCorrectImageScale) {
  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLoop);

  asset_0_->set_current_frame_data(CreateHighQualityTestFrameData());

  display_list_->StartPaint();
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  display_list_->EndPaintOfUnpaired(gfx::Rect(animation_->GetOriginalSize()));

  ASSERT_TRUE(asset_0_->last_frame_scale_factor().has_value());
  EXPECT_THAT(asset_0_->last_frame_scale_factor().value(),
              FloatEq(kCanvasImageScale));
}

TEST_F(AnimationTest, HandlesTimeStepGreaterThanAnimationDuration) {
  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLoop);

  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  ASSERT_FLOAT_EQ(animation_->GetCurrentProgress(), 0);

  AdvanceClock(kAnimationDuration / 2);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0.5f);

  AdvanceClock(kAnimationDuration * 5);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0.5f);
}

class AnimationRestarter : public AnimationObserver {
 public:
  explicit AnimationRestarter(Animation* animation) : animation_(animation) {
    observation_.Observe(animation);
  }
  AnimationRestarter(const AnimationRestarter&) = delete;
  AnimationRestarter& operator=(const AnimationRestarter&) = delete;
  ~AnimationRestarter() override = default;

  void AnimationCycleEnded(const Animation* animation) override {
    animation_->Stop();
    animation_->Start(Animation::Style::kLinear);
  }

 private:
  const base::raw_ptr<Animation> animation_;
  base::ScopedObservation<Animation, AnimationObserver> observation_{this};
};

TEST_F(AnimationTest, HandlesChangingAnimationStateWithinObserverCall) {
  AnimationRestarter observer(animation_.get());

  AdvanceClock(base::Milliseconds(300));

  animation_->Start(Animation::Style::kLinear);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  // Advance the clock to the end of the animation.
  constexpr auto kAdvanceToEnd = kAnimationDuration + base::Milliseconds(1);
  AdvanceClock(kAdvanceToEnd);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());

  // The AnimationRestarter should have restarted the animation again from the
  // beginning.
  constexpr auto kAdvance = base::Milliseconds(50);
  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(), 0.f);

  AdvanceClock(kAdvance);
  animation_->Paint(canvas(), NowTicks(), animation_->GetOriginalSize());
  EXPECT_FLOAT_EQ(animation_->GetCurrentProgress(),
                  kAdvance / kAnimationDuration);
}

}  // namespace lottie

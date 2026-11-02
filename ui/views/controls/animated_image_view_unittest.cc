// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/animated_image_view.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/lottie/animation.h"
#include "ui/views/paint_info.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

using ::testing::FloatEq;
using ::testing::NotNull;

template <typename T>
const T* FindPaintOp(const cc::PaintRecord& paint_record,
                     cc::PaintOpType paint_op_type) {
  for (const cc::PaintOp& op : paint_record) {
    if (op.GetType() == paint_op_type) {
      return static_cast<const T*>(&op);
    }

    if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const T* record_op_result = FindPaintOp<T>(
          static_cast<const cc::DrawRecordOp&>(op).record, paint_op_type);
      if (record_op_result) {
        return static_cast<const T*>(record_op_result);
      }
    }
  }
  return nullptr;
}

class AnimatedImageViewTest : public ViewsTestBase {
 protected:
  static constexpr int kDefaultWidthAndHeight = 100;
  static constexpr gfx::Size kDefaultSize =
      gfx::Size(kDefaultWidthAndHeight, kDefaultWidthAndHeight);

  void SetUp() override {
    ViewsTestBase::SetUp();

    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(kDefaultSize);
    widget_.Init(std::move(params));

    widget_.SetContentsView(std::make_unique<AnimatedImageView>());
    view()->SetUseDefaultFillLayout(true);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<lottie::Animation> CreateAnimationWithSize(
      const gfx::Size& size) {
    return std::make_unique<lottie::Animation>(
        cc::CreateSkottie(size, /*duration_secs=*/1));
  }

  cc::PaintRecord Paint(const gfx::Rect& invalidation_rect) {
    auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
    ui::PaintContext paint_context(display_list.get(),
                                   /*device_scale_factor=*/1.f,
                                   invalidation_rect, /*is_pixel_canvas=*/true);
    view()->Paint(PaintInfo::CreateRootPaintInfo(paint_context,
                                                 invalidation_rect.size()));
    RunPendingMessages();
    return display_list->FinalizeAndReleaseAsRecordForTesting();
  }

  AnimatedImageView* view() {
    return static_cast<AnimatedImageView*>(widget_.GetContentsView());
  }

  Widget widget_;
};

TEST_F(AnimatedImageViewTest, PaintsWithAdditionalTranslation) {
  view()->SetAnimatedImage(CreateAnimationWithSize(gfx::Size(80, 80)));
  view()->SetVerticalAlignment(ImageViewBase::Alignment::kCenter);
  view()->SetHorizontalAlignment(ImageViewBase::Alignment::kCenter);
  views::test::RunScheduledLayout(view());
  view()->Play();

  static constexpr float kExpectedDefaultOrigin =
      (kDefaultWidthAndHeight - 80) / 2;

  // Default should be no extra translation.
  cc::PaintRecord paint_record = Paint(view()->bounds());
  const cc::TranslateOp* translate_op =
      FindPaintOp<cc::TranslateOp>(paint_record, cc::PaintOpType::kTranslate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin));

  view()->SetAdditionalTranslation(gfx::Vector2d(5, 5));
  paint_record = Paint(view()->bounds());
  translate_op =
      FindPaintOp<cc::TranslateOp>(paint_record, cc::PaintOpType::kTranslate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin + 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin + 5));

  view()->SetAdditionalTranslation(gfx::Vector2d(5, -5));
  paint_record = Paint(view()->bounds());
  translate_op =
      FindPaintOp<cc::TranslateOp>(paint_record, cc::PaintOpType::kTranslate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin + 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin - 5));

  view()->SetAdditionalTranslation(gfx::Vector2d(-5, 5));
  paint_record = Paint(view()->bounds());
  translate_op =
      FindPaintOp<cc::TranslateOp>(paint_record, cc::PaintOpType::kTranslate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin - 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin + 5));

  view()->SetAdditionalTranslation(gfx::Vector2d(-5, -5));
  paint_record = Paint(view()->bounds());
  translate_op =
      FindPaintOp<cc::TranslateOp>(paint_record, cc::PaintOpType::kTranslate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin - 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin - 5));
}

TEST_F(AnimatedImageViewTest, PlayBeforeWidget) {
  auto animated_view = std::make_unique<AnimatedImageView>();
  animated_view->SetAnimatedImage(CreateAnimationWithSize(gfx::Size(80, 80)));
  // It should be valid to call `Play` before `animated_view` has been added to
  // a widget.
  animated_view->Play();

  widget_.SetContentsView(std::move(animated_view));
  view()->SetUseDefaultFillLayout(true);
  widget_.Show();
}

// An AnimatedImageView that counts how many times a paint has been scheduled.
class PaintCountingAnimatedImageView : public AnimatedImageView {
 public:
  int paint_count() const { return paint_count_; }
  void reset_paint_count() { paint_count_ = 0; }

  // Simulates compositor ticks at |tick_interval|, starting at |start| and
  // returning the timestamp of the last tick.
  base::TimeTicks Tick(base::TimeTicks start,
                       base::TimeDelta tick_interval,
                       int num_ticks) {
    base::TimeTicks timestamp = start;
    for (int i = 0; i < num_ticks; ++i) {
      timestamp += tick_interval;
      static_cast<ui::CompositorAnimationObserver*>(this)->OnAnimationStep(
          timestamp);
    }
    return timestamp;
  }

 private:
  // AnimatedImageView:
  void OnDidSchedulePaint(const gfx::Rect& r) override {
    AnimatedImageView::OnDidSchedulePaint(r);
    ++paint_count_;
  }

  int paint_count_ = 0;
};

class AnimatedImageViewFrameIntervalTest : public AnimatedImageViewTest {
 protected:
  static constexpr base::TimeDelta kTickInterval = base::Hertz(60);

  void SetUp() override {
    AnimatedImageViewTest::SetUp();
    widget_.GetCompositor()->FrameIntervalUpdated(kTickInterval);
    widget_.SetContentsView(std::make_unique<PaintCountingAnimatedImageView>());
    view()->SetAnimatedImage(CreateAnimationWithSize(kDefaultSize));
    view()->Play();
    view()->reset_paint_count();
  }

  PaintCountingAnimatedImageView* view() {
    return static_cast<PaintCountingAnimatedImageView*>(
        widget_.GetContentsView());
  }
};

TEST_F(AnimatedImageViewFrameIntervalTest, PaintsEveryTickByDefault) {
  EXPECT_FALSE(view()->GetFrameInterval().has_value());
  view()->Tick(base::TimeTicks() + base::Seconds(1), kTickInterval, 30);
  EXPECT_EQ(30, view()->paint_count());
}

TEST_F(AnimatedImageViewFrameIntervalTest, ThrottlesToFrameInterval) {
  // 20 fps at 60 Hz ticks should paint on every third tick. The first tick
  // after Play() always paints.
  view()->SetFrameInterval(base::Hertz(20));
  view()->Tick(base::TimeTicks() + base::Seconds(1), kTickInterval, 31);
  EXPECT_EQ(11, view()->paint_count());
}

TEST_F(AnimatedImageViewFrameIntervalTest, ToleratesTickJitter) {
  view()->SetFrameInterval(base::Hertz(20));
  // Ticks slightly shorter than 1/60s must still paint on every third tick
  // instead of every fourth.
  view()->Tick(base::TimeTicks() + base::Seconds(1),
               kTickInterval - base::Microseconds(100), 31);
  EXPECT_EQ(11, view()->paint_count());
}

TEST_F(AnimatedImageViewFrameIntervalTest, ToleranceUsesCompositorRefreshRate) {
  view()->SetFrameInterval(base::Hertz(20));

  // At 120Hz (~8.33ms per frame, ~4.17ms tolerance), a single delayed tick 44ms
  // after the last paint is still closer to 5 frames than 6 frames (50ms) and
  // should not schedule a paint yet, whereas the next 120Hz tick does.
  widget_.GetCompositor()->FrameIntervalUpdated(base::Hertz(120));
  base::TimeTicks timestamp =
      view()->Tick(base::TimeTicks() + base::Seconds(1), base::Hertz(120), 1);
  ASSERT_EQ(1, view()->paint_count());
  view()->reset_paint_count();
  timestamp = view()->Tick(timestamp, base::Milliseconds(44), 1);
  EXPECT_EQ(0, view()->paint_count());
  view()->Tick(timestamp, base::Hertz(120), 1);
  EXPECT_EQ(1, view()->paint_count());

  // At 60Hz (~16.67ms per frame, ~8.33ms tolerance), 44ms elapsed is within
  // half a compositor frame of 50ms and should schedule a paint even if
  // individual ticks were shorter.
  view()->Stop();
  view()->Play();
  view()->reset_paint_count();
  widget_.GetCompositor()->FrameIntervalUpdated(base::Hertz(60));
  timestamp =
      view()->Tick(base::TimeTicks() + base::Seconds(2), base::Hertz(60), 1);
  ASSERT_EQ(1, view()->paint_count());
  view()->reset_paint_count();
  view()->Tick(timestamp, base::Microseconds(8800), 5);
  EXPECT_EQ(1, view()->paint_count());
}

TEST_F(AnimatedImageViewFrameIntervalTest, FrameIntervalChangeWhilePlaying) {
  view()->SetFrameInterval(base::Hertz(20));
  base::TimeTicks timestamp =
      view()->Tick(base::TimeTicks() + base::Seconds(1), kTickInterval, 31);
  EXPECT_EQ(11, view()->paint_count());

  view()->reset_paint_count();
  view()->SetFrameInterval(base::Hertz(30));
  timestamp = view()->Tick(timestamp, kTickInterval, 30);
  EXPECT_EQ(15, view()->paint_count());

  view()->reset_paint_count();
  view()->SetFrameInterval(std::nullopt);
  view()->Tick(timestamp, kTickInterval, 30);
  EXPECT_EQ(30, view()->paint_count());
}

TEST_F(AnimatedImageViewFrameIntervalTest, RejectsNonPositiveFrameInterval) {
  EXPECT_CHECK_DEATH(view()->SetFrameInterval(base::TimeDelta()));
  EXPECT_CHECK_DEATH(view()->SetFrameInterval(base::Milliseconds(-1)));
}

TEST_F(AnimatedImageViewFrameIntervalTest, FirstTickAfterPlayPaints) {
  view()->SetFrameInterval(base::Hertz(20));
  base::TimeTicks timestamp =
      view()->Tick(base::TimeTicks() + base::Seconds(1), kTickInterval, 1);
  EXPECT_EQ(1, view()->paint_count());

  view()->Stop();
  view()->Play();
  view()->reset_paint_count();
  // Restarting resets the cadence, so the next tick paints even though it is
  // less than a frame interval after the previous paint.
  view()->Tick(timestamp, kTickInterval, 1);
  EXPECT_EQ(1, view()->paint_count());
}

}  // namespace
}  // namespace views

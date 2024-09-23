// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/animated_image_view.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
    if (op.GetType() == paint_op_type)
      return static_cast<const T*>(&op);

    if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const T* record_op_result = FindPaintOp<T>(
          static_cast<const cc::DrawRecordOp&>(op).record, paint_op_type);
      if (record_op_result)
        return static_cast<const T*>(record_op_result);
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

}  // namespace
}  // namespace views

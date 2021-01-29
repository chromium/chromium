// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "cc/paint/paint_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/painter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace {

class MockCanvas : public SkCanvas {
 public:
  struct DrawRectCall {
    DrawRectCall(const SkRect& rect, const SkPaint& paint)
        : rect(rect), paint(paint) {}

    bool operator<(const DrawRectCall& other) const {
      return std::tie(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom) <
             std::tie(other.rect.fLeft, other.rect.fTop, other.rect.fRight,
                      other.rect.fBottom);
    }

    SkRect rect;
    SkPaint paint;
  };

  struct DrawRRectCall {
    DrawRRectCall(const SkRRect& rrect, const SkPaint& paint)
        : rrect(rrect), paint(paint) {}

    bool operator<(const DrawRRectCall& other) const {
      SkRect rect = rrect.rect();
      SkRect other_rect = other.rrect.rect();
      return std::tie(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom) <
             std::tie(other_rect.fLeft, other_rect.fTop, other_rect.fRight,
                      other_rect.fBottom);
    }

    SkRRect rrect;
    SkPaint paint;
  };

  MockCanvas(int width, int height) : SkCanvas(width, height) {}

  // Return calls in sorted order.
  std::vector<DrawRectCall> draw_rect_calls() {
    return std::vector<DrawRectCall>(draw_rect_calls_.begin(),
                                     draw_rect_calls_.end());
  }

  // Return calls in sorted order.
  std::vector<DrawRRectCall> draw_rrect_calls() {
    return std::vector<DrawRRectCall>(draw_rrect_calls_.begin(),
                                      draw_rrect_calls_.end());
  }

  const std::vector<SkPaint>& draw_paint_calls() const {
    return draw_paint_calls_;
  }

  const SkRect& last_clip_bounds() const { return last_clip_bounds_; }

  // SkCanvas overrides:
  void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
    draw_rect_calls_.insert(DrawRectCall(rect, paint));
  }

  void onDrawRRect(const SkRRect& rrect, const SkPaint& paint) override {
    draw_rrect_calls_.insert(DrawRRectCall(rrect, paint));
  }

  void onDrawPaint(const SkPaint& paint) override {
    draw_paint_calls_.push_back(paint);
  }

  void onClipRect(const SkRect& rect,
                  SkClipOp op,
                  ClipEdgeStyle edge_style) override {
    last_clip_bounds_ = rect;
  }

 private:
  // Stores all the calls for querying by the test, in sorted order.
  std::set<DrawRectCall> draw_rect_calls_;
  std::set<DrawRRectCall> draw_rrect_calls_;

  // Stores the onDrawPaint calls in chronological order.
  std::vector<SkPaint> draw_paint_calls_;
  SkRect last_clip_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MockCanvas);
};

// Simple Painter that will be used to test BorderPainter.
class MockPainter : public views::Painter {
 public:
  MockPainter() = default;

  // Gets the canvas given to the last call to Paint().
  gfx::Canvas* given_canvas() const { return given_canvas_; }

  // Gets the size given to the last call to Paint().
  const gfx::Size& given_size() const { return given_size_; }

  // Painter overrides:
  gfx::Size GetMinimumSize() const override {
    // Just return some arbitrary size.
    return gfx::Size(60, 40);
  }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    // Just record the arguments.
    given_canvas_ = canvas;
    given_size_ = size;
  }

 private:
  gfx::Canvas* given_canvas_ = nullptr;
  gfx::Size given_size_;

  DISALLOW_COPY_AND_ASSIGN(MockPainter);
};

}  // namespace

namespace views {

class BorderTest : public ViewsTestBase {
 public:
  enum {
    // The canvas should be much bigger than the view.
    kCanvasWidth = 1000,
    kCanvasHeight = 500,
  };

  void SetUp() override {
    ViewsTestBase::SetUp();

    view_ = std::make_unique<views::View>();
    view_->SetSize(gfx::Size(100, 50));
    recorder_ = std::make_unique<cc::PaintRecorder>();
    canvas_ = std::make_unique<gfx::Canvas>(
        recorder_->beginRecording(SkRect::MakeWH(kCanvasWidth, kCanvasHeight)),
        1.0f);
  }

  void TearDown() override {
    ViewsTestBase::TearDown();

    canvas_.reset();
    recorder_.reset();
    view_.reset();
  }

  std::unique_ptr<MockCanvas> DrawIntoMockCanvas() {
    sk_sp<cc::PaintRecord> record = recorder_->finishRecordingAsPicture();
    std::unique_ptr<MockCanvas> mock(
        new MockCanvas(kCanvasWidth, kCanvasHeight));
    record->Playback(mock.get());
    return mock;
  }

 protected:
  std::unique_ptr<cc::PaintRecorder> recorder_;
  std::unique_ptr<views::View> view_;
  std::unique_ptr<gfx::Canvas> canvas_;
};

TEST_F(BorderTest, NullBorder) {
  std::unique_ptr<Border> border(NullBorder());
  EXPECT_FALSE(border);
}

TEST_F(BorderTest, SolidBorder) {
  const SkColor kBorderColor = SK_ColorMAGENTA;
  std::unique_ptr<Border> border(CreateSolidBorder(3, kBorderColor));
  EXPECT_EQ(gfx::Size(6, 6), border->GetMinimumSize());
  EXPECT_EQ(gfx::Insets(3, 3, 3, 3), border->GetInsets());
  border->Paint(*view_, canvas_.get());

  std::unique_ptr<MockCanvas> mock = DrawIntoMockCanvas();
  std::vector<MockCanvas::DrawRectCall> draw_rect_calls =
      mock->draw_rect_calls();

  gfx::Rect bounds = view_->GetLocalBounds();
  bounds.Inset(border->GetInsets());

  ASSERT_EQ(1u, mock->draw_paint_calls().size());
  EXPECT_EQ(kBorderColor, mock->draw_paint_calls()[0].getColor());
  EXPECT_EQ(gfx::RectF(bounds), gfx::SkRectToRectF(mock->last_clip_bounds()));
}

TEST_F(BorderTest, RoundedRectBorder) {
  std::unique_ptr<Border> border(CreateRoundedRectBorder(
      3, LayoutProvider::Get()->GetCornerRadiusMetric(EMPHASIS_LOW),
      SK_ColorBLUE));
  EXPECT_EQ(gfx::Size(6, 6), border->GetMinimumSize());
  EXPECT_EQ(gfx::Insets(3, 3, 3, 3), border->GetInsets());
  border->Paint(*view_, canvas_.get());

  std::unique_ptr<MockCanvas> mock = DrawIntoMockCanvas();
  SkRRect expected_rrect;
  expected_rrect.setRectXY(SkRect::MakeLTRB(1.5, 1.5, 98.5, 48.5), 4, 4);
  EXPECT_TRUE(mock->draw_rect_calls().empty());
  std::vector<MockCanvas::DrawRRectCall> draw_rrect_calls =
      mock->draw_rrect_calls();
  ASSERT_EQ(1u, draw_rrect_calls.size());
  EXPECT_EQ(expected_rrect, draw_rrect_calls[0].rrect);
  EXPECT_EQ(3, draw_rrect_calls[0].paint.getStrokeWidth());
  EXPECT_EQ(SK_ColorBLUE, draw_rrect_calls[0].paint.getColor());
  EXPECT_EQ(SkPaint::kStroke_Style, draw_rrect_calls[0].paint.getStyle());
  EXPECT_TRUE(draw_rrect_calls[0].paint.isAntiAlias());
}

TEST_F(BorderTest, EmptyBorder) {
  constexpr gfx::Insets kInsets(1, 2, 3, 4);

  std::unique_ptr<Border> border(CreateEmptyBorder(
      kInsets.top(), kInsets.left(), kInsets.bottom(), kInsets.right()));
  // The EmptyBorder has no minimum size despite nonzero insets.
  EXPECT_EQ(gfx::Size(), border->GetMinimumSize());
  EXPECT_EQ(kInsets, border->GetInsets());
  // Should have no effect.
  border->Paint(*view_, canvas_.get());

  std::unique_ptr<Border> border2(CreateEmptyBorder(kInsets));
  EXPECT_EQ(kInsets, border2->GetInsets());
}

TEST_F(BorderTest, SolidSidedBorder) {
  constexpr SkColor kBorderColor = SK_ColorMAGENTA;
  constexpr gfx::Insets kInsets(1, 2, 3, 4);

  std::unique_ptr<Border> border(
      CreateSolidSidedBorder(kInsets.top(), kInsets.left(), kInsets.bottom(),
                             kInsets.right(), kBorderColor));
  EXPECT_EQ(gfx::Size(6, 4), border->GetMinimumSize());
  EXPECT_EQ(kInsets, border->GetInsets());
  border->Paint(*view_, canvas_.get());

  std::unique_ptr<MockCanvas> mock = DrawIntoMockCanvas();
  std::vector<MockCanvas::DrawRectCall> draw_rect_calls =
      mock->draw_rect_calls();

  gfx::Rect bounds = view_->GetLocalBounds();
  bounds.Inset(border->GetInsets());

  ASSERT_EQ(1u, mock->draw_paint_calls().size());
  EXPECT_EQ(kBorderColor, mock->draw_paint_calls().front().getColor());
  EXPECT_EQ(gfx::RectF(bounds), gfx::SkRectToRectF(mock->last_clip_bounds()));
}

TEST_F(BorderTest, BorderPainter) {
  constexpr gfx::Insets kInsets(1, 2, 3, 4);

  std::unique_ptr<MockPainter> painter(new MockPainter());
  MockPainter* painter_ptr = painter.get();
  std::unique_ptr<Border> border(
      CreateBorderPainter(std::move(painter), kInsets));
  EXPECT_EQ(gfx::Size(60, 40), border->GetMinimumSize());
  EXPECT_EQ(kInsets, border->GetInsets());

  border->Paint(*view_, canvas_.get());

  // Expect that the Painter was called with our canvas and the view's size.
  EXPECT_EQ(canvas_.get(), painter_ptr->given_canvas());
  EXPECT_EQ(view_->size(), painter_ptr->given_size());
}

TEST_F(BorderTest, ExtraInsetsBorder) {
  constexpr SkColor kBorderColor = SK_ColorMAGENTA;
  constexpr int kOriginalInset = 3;
  std::unique_ptr<Border> border =
      CreateSolidBorder(kOriginalInset, kBorderColor);
  constexpr gfx::Insets kOriginalInsets(kOriginalInset);
  EXPECT_EQ(kOriginalInsets.size(), border->GetMinimumSize());
  EXPECT_EQ(kOriginalInsets, border->GetInsets());
  EXPECT_EQ(kBorderColor, border->color());

  constexpr int kExtraInset = 2;
  constexpr gfx::Insets kExtraInsets(kExtraInset);
  std::unique_ptr<Border> extra_insets_border =
      CreatePaddedBorder(std::move(border), kExtraInsets);
  constexpr gfx::Insets kTotalInsets(kOriginalInset + kExtraInset);
  EXPECT_EQ(kTotalInsets.size(), extra_insets_border->GetMinimumSize());
  EXPECT_EQ(kTotalInsets, extra_insets_border->GetInsets());
  EXPECT_EQ(kBorderColor, extra_insets_border->color());

  extra_insets_border->Paint(*view_, canvas_.get());

  std::unique_ptr<MockCanvas> mock = DrawIntoMockCanvas();
  std::vector<MockCanvas::DrawRectCall> draw_rect_calls =
      mock->draw_rect_calls();

  gfx::Rect bounds = view_->GetLocalBounds();
  // We only use the wrapped border's insets for painting the border. The extra
  // insets of the ExtraInsetsBorder are applied within the wrapped border.
  bounds.Inset(extra_insets_border->GetInsets() - gfx::Insets(kExtraInset));

  ASSERT_EQ(1u, mock->draw_paint_calls().size());
  EXPECT_EQ(kBorderColor, mock->draw_paint_calls().front().getColor());
  EXPECT_EQ(gfx::RectF(bounds), gfx::SkRectToRectF(mock->last_clip_bounds()));
}

}  // namespace views

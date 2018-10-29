// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/square_ink_drop_ripple.h"

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/animation/ink_drop_ripple_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/square_ink_drop_ripple_test_api.h"
#include "ui/views/animation/test/test_ink_drop_ripple_observer.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {

namespace {

using PaintedShape = views::test::SquareInkDropRippleTestApi::PaintedShape;

// Transforms a copy of |point| with |transform| and returns it.
gfx::Point TransformPoint(const gfx::Transform& transform,
                          const gfx::Point& point) {
  gfx::Point transformed_point = point;
  transform.TransformPoint(&transformed_point);
  return transformed_point;
}

class SquareInkDropRippleCalculateTransformsTest : public WidgetTest {
 public:
  SquareInkDropRippleCalculateTransformsTest();
  ~SquareInkDropRippleCalculateTransformsTest() override;

 protected:
  // Half the width/height of the drawn ink drop.
  static const int kHalfDrawnSize;

  // The full width/height of the drawn ink drop.
  static const int kDrawnSize;

  // The radius of the rounded rectangle corners.
  static const int kTransformedRadius;

  // Half the width/height of the transformed ink drop.
  static const int kHalfTransformedSize;

  // The full width/height of the transformed ink drop.
  static const int kTransformedSize;

  // Constant points in the drawn space that will be transformed.
  static const gfx::Point kDrawnCenterPoint;
  static const gfx::Point kDrawnMidLeftPoint;
  static const gfx::Point kDrawnMidRightPoint;
  static const gfx::Point kDrawnTopMidPoint;
  static const gfx::Point kDrawnBottomMidPoint;

  // The test target.
  SquareInkDropRipple ink_drop_ripple_;

  // Provides internal access to the test target.
  SquareInkDropRippleTestApi test_api_;

  // The gfx::Transforms collection that is populated via the
  // Calculate*Transforms() calls.
  SquareInkDropRippleTestApi::InkDropTransforms transforms_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SquareInkDropRippleCalculateTransformsTest);
};

const int SquareInkDropRippleCalculateTransformsTest::kHalfDrawnSize = 5;
const int SquareInkDropRippleCalculateTransformsTest::kDrawnSize =
    2 * kHalfDrawnSize;

const int SquareInkDropRippleCalculateTransformsTest::kTransformedRadius = 10;
const int SquareInkDropRippleCalculateTransformsTest::kHalfTransformedSize =
    100;
const int SquareInkDropRippleCalculateTransformsTest::kTransformedSize =
    2 * kHalfTransformedSize;

const gfx::Point SquareInkDropRippleCalculateTransformsTest::kDrawnCenterPoint =
    gfx::Point(kHalfDrawnSize, kHalfDrawnSize);

const gfx::Point
    SquareInkDropRippleCalculateTransformsTest::kDrawnMidLeftPoint =
        gfx::Point(0, kHalfDrawnSize);

const gfx::Point
    SquareInkDropRippleCalculateTransformsTest::kDrawnMidRightPoint =
        gfx::Point(kDrawnSize, kHalfDrawnSize);

const gfx::Point SquareInkDropRippleCalculateTransformsTest::kDrawnTopMidPoint =
    gfx::Point(kHalfDrawnSize, 0);

const gfx::Point
    SquareInkDropRippleCalculateTransformsTest::kDrawnBottomMidPoint =
        gfx::Point(kHalfDrawnSize, kDrawnSize);

SquareInkDropRippleCalculateTransformsTest::
    SquareInkDropRippleCalculateTransformsTest()
    : ink_drop_ripple_(gfx::Size(kDrawnSize, kDrawnSize),
                       2,
                       gfx::Size(kHalfDrawnSize, kHalfDrawnSize),
                       1,
                       gfx::Point(),
                       SK_ColorBLACK,
                       0.175f),
      test_api_(&ink_drop_ripple_) {}

SquareInkDropRippleCalculateTransformsTest::
    ~SquareInkDropRippleCalculateTransformsTest() {}

}  // namespace

TEST_F(SquareInkDropRippleCalculateTransformsTest,
       TransformedPointsUsingTransformsFromCalculateCircleTransforms) {
  test_api_.CalculateCircleTransforms(
      gfx::Size(kTransformedSize, kTransformedSize), &transforms_);

  struct {
    PaintedShape shape;
    gfx::Point center_point;
    gfx::Point mid_left_point;
    gfx::Point mid_right_point;
    gfx::Point top_mid_point;
    gfx::Point bottom_mid_point;
  } test_cases[] = {
      {PaintedShape::TOP_LEFT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::TOP_RIGHT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::BOTTOM_RIGHT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::BOTTOM_LEFT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::HORIZONTAL_RECT, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0), gfx::Point(0, 0), gfx::Point(0, 0)},
      {PaintedShape::VERTICAL_RECT, gfx::Point(0, 0), gfx::Point(0, 0),
       gfx::Point(0, 0), gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)}};

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    PaintedShape shape = test_cases[i].shape;
    SCOPED_TRACE(testing::Message() << "i=" << i << " shape=" << shape);
    gfx::Transform transform = transforms_[shape];

    EXPECT_EQ(test_cases[i].center_point,
              TransformPoint(transform, kDrawnCenterPoint));
    EXPECT_EQ(test_cases[i].mid_left_point,
              TransformPoint(transform, kDrawnMidLeftPoint));
    EXPECT_EQ(test_cases[i].mid_right_point,
              TransformPoint(transform, kDrawnMidRightPoint));
    EXPECT_EQ(test_cases[i].top_mid_point,
              TransformPoint(transform, kDrawnTopMidPoint));
    EXPECT_EQ(test_cases[i].bottom_mid_point,
              TransformPoint(transform, kDrawnBottomMidPoint));
  }
}

TEST_F(SquareInkDropRippleCalculateTransformsTest,
       TransformedPointsUsingTransformsFromCalculateRectTransforms) {
  test_api_.CalculateRectTransforms(
      gfx::Size(kTransformedSize, kTransformedSize), kTransformedRadius,
      &transforms_);

  const int x_offset = kHalfTransformedSize - kTransformedRadius;
  const int y_offset = kHalfTransformedSize - kTransformedRadius;

  struct {
    PaintedShape shape;
    gfx::Point center_point;
    gfx::Point mid_left_point;
    gfx::Point mid_right_point;
    gfx::Point top_mid_point;
    gfx::Point bottom_mid_point;
  } test_cases[] = {
      {PaintedShape::TOP_LEFT_CIRCLE, gfx::Point(-x_offset, -y_offset),
       gfx::Point(-kHalfTransformedSize, -y_offset),
       gfx::Point(-x_offset + kTransformedRadius, -y_offset),
       gfx::Point(-x_offset, -kHalfTransformedSize),
       gfx::Point(-x_offset, -y_offset + kTransformedRadius)},
      {PaintedShape::TOP_RIGHT_CIRCLE, gfx::Point(x_offset, -y_offset),
       gfx::Point(x_offset - kTransformedRadius, -y_offset),
       gfx::Point(kHalfTransformedSize, -y_offset),
       gfx::Point(x_offset, -kHalfTransformedSize),
       gfx::Point(x_offset, -y_offset + kTransformedRadius)},
      {PaintedShape::BOTTOM_RIGHT_CIRCLE, gfx::Point(x_offset, y_offset),
       gfx::Point(x_offset - kTransformedRadius, y_offset),
       gfx::Point(kHalfTransformedSize, y_offset),
       gfx::Point(x_offset, y_offset - kTransformedRadius),
       gfx::Point(x_offset, kHalfTransformedSize)},
      {PaintedShape::BOTTOM_LEFT_CIRCLE, gfx::Point(-x_offset, y_offset),
       gfx::Point(-kHalfTransformedSize, y_offset),
       gfx::Point(-x_offset + kTransformedRadius, y_offset),
       gfx::Point(-x_offset, y_offset - kTransformedRadius),
       gfx::Point(-x_offset, kHalfTransformedSize)},
      {PaintedShape::HORIZONTAL_RECT, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0), gfx::Point(0, -y_offset),
       gfx::Point(0, y_offset)},
      {PaintedShape::VERTICAL_RECT, gfx::Point(0, 0), gfx::Point(-x_offset, 0),
       gfx::Point(x_offset, 0), gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)}};

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    PaintedShape shape = test_cases[i].shape;
    SCOPED_TRACE(testing::Message() << "i=" << i << " shape=" << shape);
    gfx::Transform transform = transforms_[shape];

    EXPECT_EQ(test_cases[i].center_point,
              TransformPoint(transform, kDrawnCenterPoint));
    EXPECT_EQ(test_cases[i].mid_left_point,
              TransformPoint(transform, kDrawnMidLeftPoint));
    EXPECT_EQ(test_cases[i].mid_right_point,
              TransformPoint(transform, kDrawnMidRightPoint));
    EXPECT_EQ(test_cases[i].top_mid_point,
              TransformPoint(transform, kDrawnTopMidPoint));
    EXPECT_EQ(test_cases[i].bottom_mid_point,
              TransformPoint(transform, kDrawnBottomMidPoint));
  }
}

TEST_F(SquareInkDropRippleCalculateTransformsTest, RippleIsPixelAligned) {
  // Create a ripple that would not naturally be pixel aligned at a fractional
  // scale factor.
  const gfx::Point center(14, 14);
  const gfx::Rect drawn_rect_bounds(0, 0, 10, 10);
  SquareInkDropRipple ink_drop_ripple(drawn_rect_bounds.size(), 2,
                                      gfx::Size(1, 1),  // unimportant
                                      1, center, SK_ColorBLACK, 0.175f);
  SquareInkDropRippleTestApi test_api(&ink_drop_ripple);

  // Add to a widget so we can control the DSF.
  auto* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  auto* host_view = new View();
  host_view->SetPaintToLayer();
  widget->GetContentsView()->AddChildView(host_view);
  host_view->layer()->Add(ink_drop_ripple.GetRootLayer());

  // Test a variety of scale factors and target transform sizes.
  std::vector<float> dsfs({1.0f, 1.25f, 1.5f, 2.0f, 3.0f});
  std::vector<int> target_sizes({5, 7, 11, 13, 31});

  for (float dsf : dsfs) {
    for (int target_size : target_sizes) {
      SCOPED_TRACE(testing::Message()
                   << "target_size=" << target_size << " dsf=" << dsf);
      host_view->layer()->GetCompositor()->SetScaleAndSize(
          dsf, gfx::Size(100, 100), viz::LocalSurfaceId(), base::TimeTicks());

      SquareInkDropRippleTestApi::InkDropTransforms transforms;
      test_api.CalculateRectTransforms(gfx::Size(target_size, target_size), 0,
                                       &transforms);

      // Checks that a rectangle is integer-aligned modulo floating point error.
      auto verify_bounds = [](const gfx::RectF& rect) {
        float float_min_x = rect.x();
        float float_min_y = rect.y();
        float float_max_x = rect.right();
        float float_max_y = rect.bottom();

        int min_x = gfx::ToRoundedInt(float_min_x);
        int min_y = gfx::ToRoundedInt(float_min_y);
        int max_x = gfx::ToRoundedInt(float_max_x);
        int max_y = gfx::ToRoundedInt(float_max_y);

        EXPECT_LT(std::abs(min_x - float_min_x), 0.01f);
        EXPECT_LT(std::abs(min_y - float_min_y), 0.01f);
        EXPECT_LT(std::abs(max_x - float_max_x), 0.01f);
        EXPECT_LT(std::abs(max_y - float_max_y), 0.01f);
      };

      // When you feed in the bounds of the rectangle layer delegate, no matter
      // what the target size was you should get an integer aligned bounding
      // box.
      gfx::Transform transform = transforms[PaintedShape::HORIZONTAL_RECT];
      gfx::RectF horizontal_rect(drawn_rect_bounds);
      transform.TransformRect(&horizontal_rect);
      horizontal_rect.Scale(dsf);
      verify_bounds(horizontal_rect);

      transform = transforms[PaintedShape::VERTICAL_RECT];
      gfx::RectF vertical_rect(drawn_rect_bounds);
      transform.TransformRect(&vertical_rect);
      vertical_rect.Scale(dsf);
      verify_bounds(vertical_rect);
    }
  }

  widget->CloseNow();
}

}  // namespace test
}  // namespace views

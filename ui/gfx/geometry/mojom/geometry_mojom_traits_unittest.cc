// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {

TEST(GeometryStructTraitsTest, Point) {
  const int32_t x = 1234;
  const int32_t y = -5678;
  gfx::Point input(x, y);
  gfx::Point output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Point>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST(GeometryStructTraitsTest, PointF) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  gfx::PointF input(x, y);
  gfx::PointF output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::PointF>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST(GeometryStructTraitsTest, Point3F) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  const float z = 5432.1f;
  gfx::Point3F input(x, y, z);
  gfx::Point3F output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Point3F>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(z, output.z());
}

TEST(GeometryStructTraitsTest, Size) {
  const int32_t width = 1234;
  const int32_t height = 5678;
  gfx::Size input(width, height);
  gfx::Size output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Size>(input, output);
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST(GeometryStructTraitsTest, SizeF) {
  const float width = 1234.5f;
  const float height = 6789.6f;
  gfx::SizeF input(width, height);
  gfx::SizeF output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::SizeF>(input, output);
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST(GeometryStructTraitsTest, Rect) {
  const int32_t x = 1234;
  const int32_t y = 5678;
  const int32_t width = 4321;
  const int32_t height = 8765;
  gfx::Rect input(x, y, width, height);
  gfx::Rect output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Rect>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST(GeometryStructTraitsTest, RectF) {
  const float x = 1234.1f;
  const float y = 5678.2f;
  const float width = 4321.3f;
  const float height = 8765.4f;
  gfx::RectF input(x, y, width, height);
  gfx::RectF output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::RectF>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST(GeometryStructTraitsTest, Insets) {
  const int32_t top = 1234;
  const int32_t left = 5678;
  const int32_t bottom = 4321;
  const int32_t right = 8765;
  auto input = gfx::Insets::TLBR(top, left, bottom, right);
  gfx::Insets output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Insets>(input, output);
  EXPECT_EQ(top, output.top());
  EXPECT_EQ(left, output.left());
  EXPECT_EQ(bottom, output.bottom());
  EXPECT_EQ(right, output.right());
}

TEST(GeometryStructTraitsTest, InsetsF) {
  const float top = 1234.1f;
  const float left = 5678.2f;
  const float bottom = 4321.3f;
  const float right = 8765.4f;
  auto input = gfx::InsetsF::TLBR(top, left, bottom, right);
  gfx::InsetsF output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::InsetsF>(input, output);
  EXPECT_EQ(top, output.top());
  EXPECT_EQ(left, output.left());
  EXPECT_EQ(bottom, output.bottom());
  EXPECT_EQ(right, output.right());
}

TEST(GeometryStructTraitsTest, Vector2d) {
  const int32_t x = 1234;
  const int32_t y = -5678;
  gfx::Vector2d input(x, y);
  gfx::Vector2d output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Vector2d>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST(GeometryStructTraitsTest, Vector2dF) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  gfx::Vector2dF input(x, y);
  gfx::Vector2dF output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Vector2dF>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST(GeometryStructTraitsTest, Vector3dF) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  const float z = 5432.1f;
  gfx::Vector3dF input(x, y, z);
  gfx::Vector3dF output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Vector3dF>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(z, output.z());
}

TEST(GeometryStructTraitsTest, Quaternion) {
  const double x = 1234.5;
  const double y = 6789.6;
  const double z = 31415.9;
  const double w = 27182.8;
  gfx::Quaternion input(x, y, z, w);
  gfx::Quaternion output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::Quaternion>(input, output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(z, output.z());
  EXPECT_EQ(w, output.w());
}

TEST(GeometryStructTraitsTest, QuadF) {
  const PointF p1(1234.5, 6789.6);
  const PointF p2(-31415.9, 27182.8);
  const PointF p3(5432.1, -5678);
  const PointF p4(-2468.0, -3579.1);
  gfx::QuadF input(p1, p2, p3, p4);
  gfx::QuadF output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::QuadF>(input, output);
  EXPECT_EQ(p1, output.p1());
  EXPECT_EQ(p2, output.p2());
  EXPECT_EQ(p3, output.p3());
  EXPECT_EQ(p4, output.p4());
  EXPECT_EQ(input, output);
}

TEST(GeometryStructTraitsTest, AxisTransform2d) {
  const gfx::Vector2dF scale(2.f, 3.f);
  const gfx::Vector2dF translate(10.f, 20.f);
  gfx::AxisTransform2d input =
      gfx::AxisTransform2d::FromScaleAndTranslation(scale, translate);
  gfx::AxisTransform2d output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::AxisTransform2d>(input,
                                                                   output);
  EXPECT_EQ(input, output);
}

}  // namespace gfx

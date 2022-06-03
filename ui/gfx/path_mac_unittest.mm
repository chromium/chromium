// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/path_mac.h"

#include <cmath>
#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace gfx {

namespace {

// Returns the point at a distance of |radius| from the point (|centre_x|,
// |centre_y|), and angle |degrees| from the positive horizontal axis, measured
// anti-clockwise.
NSPoint GetRadialPoint(double radius,
                       double degrees,
                       double centre_x,
                       double centre_y) {
  const double radian = (degrees * SK_ScalarPI) / 180;
  return NSMakePoint(centre_x + radius * std::cos(radian),
                     centre_y + radius * std::sin(radian));
}

// Returns the area of a circle with the given |radius|.
double CalculateCircleArea(double radius) {
  return SK_ScalarPI * radius * radius;
}

// Returns the area of a simple polygon. |path| should represent a simple
// polygon.
double CalculatePolygonArea(NSBezierPath* path) {
  // If path represents a single polygon, it will have MoveTo, followed by
  // multiple LineTo, followed By ClosePath, followed by another MoveTo
  // NSBezierPathElement.
  const size_t element_count = [path elementCount];
  NSPoint points[3];
  std::vector<NSPoint> poly;

  for (size_t i = 0; i < element_count - 1; i++) {
    NSBezierPathElement element =
        [path elementAtIndex:i associatedPoints:points];
    poly.push_back(points[0]);
    DCHECK_EQ(element,
              i ? (i == element_count - 2 ? NSClosePathBezierPathElement
                                          : NSLineToBezierPathElement)
                : NSMoveToBezierPathElement);
  }
  DCHECK_EQ([path elementAtIndex:element_count - 1], NSMoveToBezierPathElement);

  // Shoelace Algorithm to find the area of a simple polygon.
  DCHECK(NSEqualPoints(poly.front(), poly.back()));
  double area = 0;
  for (size_t i = 0; i < poly.size() - 1; i++)
    area += poly[i].x * poly[i + 1].y - poly[i].y * poly[i + 1].x;

  return std::fabs(area) / 2.0;
}

// Returns the area of a rounded rectangle with the given |width|, |height| and
// |radius|.
double CalculateRoundedRectangleArea(double width,
                                     double height,
                                     double radius) {
  const double inside_width = width - 2 * radius;
  const double inside_height = height - 2 * radius;
  return inside_width * inside_height +
         2 * radius * (inside_width + inside_height) +
         CalculateCircleArea(radius);
}

// Returns the bounding box of |path| as a Rect.
Rect GetBoundingBox(NSBezierPath* path) {
  const NSRect bounds = [path bounds];
  return ToNearestRect(RectF(bounds.origin.x, bounds.origin.y,
                             bounds.size.width, bounds.size.height));
}

}  // namespace

// Check that empty NSBezierPath is returned for empty SkPath.
TEST(CreateNSBezierPathFromSkPathTest, EmptyPath) {
  NSBezierPath* result = CreateNSBezierPathFromSkPath(SkPath());
  EXPECT_TRUE([result isEmpty]);
}

// Check that the returned NSBezierPath has the correct winding rule.
TEST(CreateNSBezierPathFromSkPathTest, FillType) {
  SkPath path;
  path.setFillType(SkPathFillType::kWinding);
  NSBezierPath* result = CreateNSBezierPathFromSkPath(path);
  EXPECT_EQ(NSNonZeroWindingRule, [result windingRule]);

  path.setFillType(SkPathFillType::kEvenOdd);
  result = CreateNSBezierPathFromSkPath(path);
  EXPECT_EQ(NSEvenOddWindingRule, [result windingRule]);
}

// Check that a path containing multiple subpaths, in this case two rectangles,
// is correctly converted to a NSBezierPath.
TEST(CreateNSBezierPathFromSkPathTest, TwoRectanglesPath) {
  const SkRect rects[] = {
      {0, 0, 50, 50}, {100, 100, 150, 150},
  };
  const NSPoint inside_points[] = {
      {1, 1},     {1, 49},    {49, 49},   {49, 1},    {25, 25},
      {101, 101}, {101, 149}, {149, 149}, {149, 101}, {125, 125}};
  const NSPoint outside_points[] = {{-1, -1}, {-1, 51},  {51, 51},   {51, -1},
                                    {99, 99}, {99, 151}, {151, 151}, {151, 99},
                                    {75, 75}, {-5, -5}};
  ASSERT_EQ(base::size(inside_points), base::size(outside_points));
  const Rect expected_bounds(0, 0, 150, 150);

  SkPath path;
  path.addRect(rects[0]);
  path.addRect(rects[1]);
  NSBezierPath* result = CreateNSBezierPathFromSkPath(path);

  // Check points near the boundary of the path and verify that they are
  // reported correctly as being inside/outside the path.
  for (size_t i = 0; i < base::size(inside_points); i++) {
    EXPECT_TRUE([result containsPoint:inside_points[i]]);
    EXPECT_FALSE([result containsPoint:outside_points[i]]);
  }

  // Check that the returned result has the correct bounding box. GetBoundingBox
  // rounds the coordinates to nearest integer values.
  EXPECT_EQ(expected_bounds, GetBoundingBox(result));
}

// Test that an SKPath containing a circle is converted correctly to a
// NSBezierPath.
TEST(CreateNSBezierPathFromSkPathTest, CirclePath) {
  const int kRadius = 5;
  const int kCentreX = 10;
  const int kCentreY = 15;
  const double kCushion = 0.1;
  // Expected bounding box of the circle.
  const Rect expected_bounds(kCentreX - kRadius, kCentreY - kRadius,
                             2 * kRadius, 2 * kRadius);

  SkPath path;
  path.addCircle(SkIntToScalar(kCentreX), SkIntToScalar(kCentreY),
                 SkIntToScalar(kRadius));
  NSBezierPath* result = CreateNSBezierPathFromSkPath(path);

  // Check points near the boundary of the circle and verify that they are
  // reported correctly as being inside/outside the path.
  for (size_t deg = 0; deg < 360; deg++) {
    NSPoint inside_point =
        GetRadialPoint(kRadius - kCushion, deg, kCentreX, kCentreY);
    NSPoint outside_point =
        GetRadialPoint(kRadius + kCushion, deg, kCentreX, kCentreY);
    EXPECT_TRUE([result containsPoint:inside_point]);
    EXPECT_FALSE([result containsPoint:outside_point]);
  }

  // Check that the returned result has the correct bounding box. GetBoundingBox
  // rounds the coordinates to nearest integer values.
  EXPECT_EQ(expected_bounds, GetBoundingBox(result));

  // Check area of converted path is correct up to a certain tolerance value. To
  // find the area of the NSBezierPath returned, flatten it i.e. convert it to a
  // polygon.
  [NSBezierPath setDefaultFlatness:0.01];
  NSBezierPath* polygon = [result bezierPathByFlatteningPath];
  const double kTolerance = 0.14;
  EXPECT_NEAR(CalculateCircleArea(kRadius), CalculatePolygonArea(polygon),
              kTolerance);
}

// Test that an SKPath containing a rounded rectangle is converted correctly to
// a NSBezierPath.
TEST(CreateNSBezierPathFromSkPathTest, RoundedRectanglePath) {
  const int kRectangleWidth = 50;
  const int kRectangleHeight = 100;
  const int kCornerRadius = 5;
  const double kCushion = 0.1;
  // Expected bounding box of the rounded rectangle.
  const Rect expected_bounds(kRectangleWidth, kRectangleHeight);

  SkRRect rrect;
  rrect.setRectXY(SkRect::MakeWH(kRectangleWidth, kRectangleHeight),
                  kCornerRadius, kCornerRadius);

  const NSPoint inside_points[] = {
      // Bottom left corner.
      {kCornerRadius / 2.0, kCornerRadius / 2.0},
      // Bottom right corner.
      {kRectangleWidth - kCornerRadius / 2.0, kCornerRadius / 2.0},
      // Top Right corner.
      {kRectangleWidth - kCornerRadius / 2.0,
       kRectangleHeight - kCornerRadius / 2.0},
      // Top left corner.
      {kCornerRadius / 2.0, kRectangleHeight - kCornerRadius / 2.0},
      // Bottom middle.
      {kRectangleWidth / 2.0, kCushion},
      // Right middle.
      {kRectangleWidth - kCushion, kRectangleHeight / 2.0},
      // Top middle.
      {kRectangleWidth / 2.0, kRectangleHeight - kCushion},
      // Left middle.
      {kCushion, kRectangleHeight / 2.0}};
  const NSPoint outside_points[] = {
      // Bottom left corner.
      {0, 0},
      // Bottom right corner.
      {kRectangleWidth, 0},
      // Top right corner.
      {kRectangleWidth, kRectangleHeight},
      // Top left corner.
      {0, kRectangleHeight},
      // Bottom middle.
      {kRectangleWidth / 2.0, -kCushion},
      // Right middle.
      {kRectangleWidth + kCushion, kRectangleHeight / 2.0},
      // Top middle.
      {kRectangleWidth / 2.0, kRectangleHeight + kCushion},
      // Left middle.
      {-kCushion, kRectangleHeight / 2.0}};
  ASSERT_EQ(base::size(inside_points), base::size(outside_points));

  SkPath path;
  path.addRRect(rrect);
  NSBezierPath* result = CreateNSBezierPathFromSkPath(path);

  // Check points near the boundary of the path and verify that they are
  // reported correctly as being inside/outside the path.
  for (size_t i = 0; i < base::size(inside_points); i++) {
    EXPECT_TRUE([result containsPoint:inside_points[i]]);
    EXPECT_FALSE([result containsPoint:outside_points[i]]);
  }

  // Check that the returned result has the correct bounding box. GetBoundingBox
  // rounds the coordinates to nearest integer values.
  EXPECT_EQ(expected_bounds, GetBoundingBox(result));

  // Check area of converted path is correct up to a certain tolerance value. To
  // find the area of the NSBezierPath returned, flatten it i.e. convert it to a
  // polygon.
  [NSBezierPath setDefaultFlatness:0.01];
  NSBezierPath* polygon = [result bezierPathByFlatteningPath];
  const double kTolerance = 0.14;
  EXPECT_NEAR(CalculateRoundedRectangleArea(kRectangleWidth, kRectangleHeight,
                                            kCornerRadius),
              CalculatePolygonArea(polygon), kTolerance);
}

}  // namespace gfx

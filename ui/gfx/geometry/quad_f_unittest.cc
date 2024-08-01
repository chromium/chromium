// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <algorithm>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

TEST(QuadFTest, Construction) {
  // Verify constructors.
  PointF a(1, 1);
  PointF b(2, 1);
  PointF c(2, 2);
  PointF d(1, 2);
  PointF e;
  QuadF q1;
  QuadF q2(e, e, e, e);
  QuadF q3(a, b, c, d);
  QuadF q4(BoundingRect(a, c));
  EXPECT_EQ(q1, q2);
  EXPECT_EQ(q3, q4);

  // Verify getters.
  EXPECT_EQ(q3.p1(), a);
  EXPECT_EQ(q3.p2(), b);
  EXPECT_EQ(q3.p3(), c);
  EXPECT_EQ(q3.p4(), d);

  // Verify setters.
  q3.set_p1(b);
  q3.set_p2(c);
  q3.set_p3(d);
  q3.set_p4(a);
  EXPECT_EQ(q3.p1(), b);
  EXPECT_EQ(q3.p2(), c);
  EXPECT_EQ(q3.p3(), d);
  EXPECT_EQ(q3.p4(), a);

  // Verify operator=(Rect)
  EXPECT_NE(q1, q4);
  q1 = BoundingRect(a, c);
  EXPECT_EQ(q1, q4);

  // Verify operator=(Quad)
  EXPECT_NE(q1, q3);
  q1 = q3;
  EXPECT_EQ(q1, q3);
}

TEST(QuadFTest, AddingVectors) {
  PointF a(1, 1);
  PointF b(2, 1);
  PointF c(2, 2);
  PointF d(1, 2);
  Vector2dF v(3.5f, -2.5f);

  QuadF q1(a, b, c, d);
  QuadF added = q1 + v;
  q1 += v;
  QuadF expected1(PointF(4.5f, -1.5f), PointF(5.5f, -1.5f), PointF(5.5f, -0.5f),
                  PointF(4.5f, -0.5f));
  EXPECT_EQ(expected1, added);
  EXPECT_EQ(expected1, q1);

  QuadF q2(a, b, c, d);
  QuadF subtracted = q2 - v;
  q2 -= v;
  QuadF expected2(PointF(-2.5f, 3.5f), PointF(-1.5f, 3.5f), PointF(-1.5f, 4.5f),
                  PointF(-2.5f, 4.5f));
  EXPECT_EQ(expected2, subtracted);
  EXPECT_EQ(expected2, q2);

  QuadF q3(a, b, c, d);
  q3 += v;
  q3 -= v;
  EXPECT_EQ(QuadF(a, b, c, d), q3);
  EXPECT_EQ(q3, (q3 + v - v));
}

TEST(QuadFTest, IsRectilinear) {
  PointF a(1, 1);
  PointF b(2, 1);
  PointF c(2, 2);
  PointF d(1, 2);
  Vector2dF v(3.5f, -2.5f);

  EXPECT_TRUE(QuadF().IsRectilinear());
  EXPECT_TRUE(QuadF(a, b, c, d).IsRectilinear());
  EXPECT_TRUE((QuadF(a, b, c, d) + v).IsRectilinear());

  float epsilon = std::numeric_limits<float>::epsilon();
  PointF a2(1 + epsilon / 2, 1 + epsilon / 2);
  PointF b2(2 + epsilon / 2, 1 + epsilon / 2);
  PointF c2(2 + epsilon / 2, 2 + epsilon / 2);
  PointF d2(1 + epsilon / 2, 2 + epsilon / 2);
  EXPECT_TRUE(QuadF(a2, b, c, d).IsRectilinear());
  EXPECT_TRUE((QuadF(a2, b, c, d) + v).IsRectilinear());
  EXPECT_TRUE(QuadF(a, b2, c, d).IsRectilinear());
  EXPECT_TRUE((QuadF(a, b2, c, d) + v).IsRectilinear());
  EXPECT_TRUE(QuadF(a, b, c2, d).IsRectilinear());
  EXPECT_TRUE((QuadF(a, b, c2, d) + v).IsRectilinear());
  EXPECT_TRUE(QuadF(a, b, c, d2).IsRectilinear());
  EXPECT_TRUE((QuadF(a, b, c, d2) + v).IsRectilinear());

  struct {
    PointF a_off, b_off, c_off, d_off;
  } tests[] = {{PointF(1, 1.00001f), PointF(2, 1.00001f), PointF(2, 2.00001f),
                PointF(1, 2.00001f)},
               {PointF(1.00001f, 1), PointF(2.00001f, 1), PointF(2.00001f, 2),
                PointF(1.00001f, 2)},
               {PointF(1.00001f, 1.00001f), PointF(2.00001f, 1.00001f),
                PointF(2.00001f, 2.00001f), PointF(1.00001f, 2.00001f)},
               {PointF(1, 0.99999f), PointF(2, 0.99999f), PointF(2, 1.99999f),
                PointF(1, 1.99999f)},
               {PointF(0.99999f, 1), PointF(1.99999f, 1), PointF(1.99999f, 2),
                PointF(0.99999f, 2)},
               {PointF(0.99999f, 0.99999f), PointF(1.99999f, 0.99999f),
                PointF(1.99999f, 1.99999f), PointF(0.99999f, 1.99999f)}};

  for (const auto& test : tests) {
    PointF a_off = test.a_off;
    PointF b_off = test.b_off;
    PointF c_off = test.c_off;
    PointF d_off = test.d_off;

    EXPECT_FALSE(QuadF(a_off, b, c, d).IsRectilinear());
    EXPECT_FALSE((QuadF(a_off, b, c, d) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a, b_off, c, d).IsRectilinear());
    EXPECT_FALSE((QuadF(a, b_off, c, d) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a, b, c_off, d).IsRectilinear());
    EXPECT_FALSE((QuadF(a, b, c_off, d) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a, b, c, d_off).IsRectilinear());
    EXPECT_FALSE((QuadF(a, b, c, d_off) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a_off, b, c_off, d).IsRectilinear());
    EXPECT_FALSE((QuadF(a_off, b, c_off, d) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a, b_off, c, d_off).IsRectilinear());
    EXPECT_FALSE((QuadF(a, b_off, c, d_off) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a, b_off, c_off, d_off).IsRectilinear());
    EXPECT_FALSE((QuadF(a, b_off, c_off, d_off) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a_off, b, c_off, d_off).IsRectilinear());
    EXPECT_FALSE((QuadF(a_off, b, c_off, d_off) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a_off, b_off, c, d_off).IsRectilinear());
    EXPECT_FALSE((QuadF(a_off, b_off, c, d_off) + v).IsRectilinear());
    EXPECT_FALSE(QuadF(a_off, b_off, c_off, d).IsRectilinear());
    EXPECT_FALSE((QuadF(a_off, b_off, c_off, d) + v).IsRectilinear());
    EXPECT_TRUE(QuadF(a_off, b_off, c_off, d_off).IsRectilinear());
    EXPECT_TRUE((QuadF(a_off, b_off, c_off, d_off) + v).IsRectilinear());
  }
}

TEST(QuadFTest, IsRectilinearForMappedQuad) {
  const int kNumRectilinear = 8;
  Transform rectilinear_trans[kNumRectilinear];
  rectilinear_trans[1].Rotate(90.f);
  rectilinear_trans[2].Rotate(180.f);
  rectilinear_trans[3].Rotate(270.f);
  rectilinear_trans[4].Skew(0.00000000001f, 0.0f);
  rectilinear_trans[5].Skew(0.0f, 0.00000000001f);
  rectilinear_trans[6].Scale(0.00001f, 0.00001f);
  rectilinear_trans[6].Rotate(180.f);
  rectilinear_trans[7].Scale(100000.f, 100000.f);
  rectilinear_trans[7].Rotate(180.f);

  gfx::QuadF original(
      gfx::RectF(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f));

  for (int i = 0; i < kNumRectilinear; ++i) {
    gfx::QuadF quad = rectilinear_trans[i].MapQuad(original);
    EXPECT_TRUE(quad.IsRectilinear()) << "case " << i;
  }

  const int kNumNonRectilinear = 10;
  gfx::Transform non_rectilinear_trans[kNumNonRectilinear];
  non_rectilinear_trans[0].Rotate(359.9999f);
  non_rectilinear_trans[1].Rotate(0.0000001f);
  non_rectilinear_trans[2].Rotate(89.9999f);
  non_rectilinear_trans[3].Rotate(90.00001f);
  non_rectilinear_trans[4].Rotate(179.9999f);
  non_rectilinear_trans[5].Rotate(180.00001f);
  non_rectilinear_trans[6].Rotate(269.9999f);
  non_rectilinear_trans[7].Rotate(270.0001f);
  non_rectilinear_trans[8].Skew(0.00001f, 0.0f);
  non_rectilinear_trans[9].Skew(0.0f, 0.00001f);

  for (int i = 0; i < kNumNonRectilinear; ++i) {
    gfx::QuadF quad = non_rectilinear_trans[i].MapQuad(original);
    EXPECT_FALSE(quad.IsRectilinear()) << "case " << i;
  }
}

TEST(QuadFTest, IsCounterClockwise) {
  PointF a1(1, 1);
  PointF b1(2, 1);
  PointF c1(2, 2);
  PointF d1(1, 2);
  EXPECT_FALSE(QuadF(a1, b1, c1, d1).IsCounterClockwise());
  EXPECT_FALSE(QuadF(b1, c1, d1, a1).IsCounterClockwise());
  EXPECT_TRUE(QuadF(a1, d1, c1, b1).IsCounterClockwise());
  EXPECT_TRUE(QuadF(c1, b1, a1, d1).IsCounterClockwise());

  // Slightly more complicated quads should work just as easily.
  PointF a2(1.3f, 1.4f);
  PointF b2(-0.7f, 4.9f);
  PointF c2(1.8f, 6.2f);
  PointF d2(2.1f, 1.6f);
  EXPECT_TRUE(QuadF(a2, b2, c2, d2).IsCounterClockwise());
  EXPECT_TRUE(QuadF(b2, c2, d2, a2).IsCounterClockwise());
  EXPECT_FALSE(QuadF(a2, d2, c2, b2).IsCounterClockwise());
  EXPECT_FALSE(QuadF(c2, b2, a2, d2).IsCounterClockwise());

  // Quads with 3 collinear points should work correctly, too.
  PointF a3(0, 0);
  PointF b3(1, 0);
  PointF c3(2, 0);
  PointF d3(1, 1);
  EXPECT_FALSE(QuadF(a3, b3, c3, d3).IsCounterClockwise());
  EXPECT_FALSE(QuadF(b3, c3, d3, a3).IsCounterClockwise());
  EXPECT_TRUE(QuadF(a3, d3, c3, b3).IsCounterClockwise());
  // The next expectation in particular would fail for an implementation
  // that incorrectly uses only a cross product of the first 3 vertices.
  EXPECT_TRUE(QuadF(c3, b3, a3, d3).IsCounterClockwise());

  // Non-convex quads should work correctly, too.
  PointF a4(0, 0);
  PointF b4(1, 1);
  PointF c4(2, 0);
  PointF d4(1, 3);
  EXPECT_FALSE(QuadF(a4, b4, c4, d4).IsCounterClockwise());
  EXPECT_FALSE(QuadF(b4, c4, d4, a4).IsCounterClockwise());
  EXPECT_TRUE(QuadF(a4, d4, c4, b4).IsCounterClockwise());
  EXPECT_TRUE(QuadF(c4, b4, a4, d4).IsCounterClockwise());

  // A quad with huge coordinates should not fail this check due to
  // single-precision overflow.
  PointF a5(1e30f, 1e30f);
  PointF b5(1e35f, 1e30f);
  PointF c5(1e35f, 1e35f);
  PointF d5(1e30f, 1e35f);
  EXPECT_FALSE(QuadF(a5, b5, c5, d5).IsCounterClockwise());
  EXPECT_FALSE(QuadF(b5, c5, d5, a5).IsCounterClockwise());
  EXPECT_TRUE(QuadF(a5, d5, c5, b5).IsCounterClockwise());
  EXPECT_TRUE(QuadF(c5, b5, a5, d5).IsCounterClockwise());
}

TEST(QuadFTest, BoundingBox) {
  RectF r(3.2f, 5.4f, 7.007f, 12.01f);
  EXPECT_EQ(r, QuadF(r).BoundingBox());

  PointF a(1.3f, 1.4f);
  PointF b(-0.7f, 4.9f);
  PointF c(1.8f, 6.2f);
  PointF d(2.1f, 1.6f);
  float left = -0.7f;
  float top = 1.4f;
  float right = 2.1f;
  float bottom = 6.2f;
  EXPECT_EQ(RectF(left, top, right - left, bottom - top),
            QuadF(a, b, c, d).BoundingBox());
}

TEST(QuadFTest, ContainsPoint) {
  PointF a(1.3f, 1.4f);
  PointF b(-0.8f, 4.4f);
  PointF c(1.8f, 6.1f);
  PointF d(2.1f, 1.6f);

  Vector2dF epsilon_x(2 * std::numeric_limits<float>::epsilon(), 0);
  Vector2dF epsilon_y(0, 2 * std::numeric_limits<float>::epsilon());

  Vector2dF ac_center = c - a;
  ac_center.Scale(0.5f);
  Vector2dF bd_center = d - b;
  bd_center.Scale(0.5f);

  EXPECT_TRUE(QuadF(a, b, c, d).Contains(a + ac_center));
  EXPECT_TRUE(QuadF(a, b, c, d).Contains(b + bd_center));
  EXPECT_TRUE(QuadF(a, b, c, d).Contains(c - ac_center));
  EXPECT_TRUE(QuadF(a, b, c, d).Contains(d - bd_center));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(a - ac_center));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(b - bd_center));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(c + ac_center));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(d + bd_center));

  EXPECT_TRUE(QuadF(a, b, c, d).Contains(a));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(a - epsilon_x));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(a - epsilon_y));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(a + epsilon_x));
  EXPECT_TRUE(QuadF(a, b, c, d).Contains(a + epsilon_y));

  EXPECT_TRUE(QuadF(a, b, c, d).Contains(b));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(b - epsilon_x));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(b - epsilon_y));
  EXPECT_TRUE(QuadF(a, b, c, d).Contains(b + epsilon_x));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(b + epsilon_y));

  EXPECT_TRUE(QuadF(a, b, c, d).Contains(c));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(c - epsilon_x));
  EXPECT_TRUE(QuadF(a, b, c, d).Contains(c - epsilon_y));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(c + epsilon_x));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(c + epsilon_y));

  EXPECT_TRUE(QuadF(a, b, c, d).Contains(d));
  EXPECT_TRUE(QuadF(a, b, c, d).Contains(d - epsilon_x));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(d - epsilon_y));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(d + epsilon_x));
  EXPECT_FALSE(QuadF(a, b, c, d).Contains(d + epsilon_y));

  // Test a simple square.
  PointF s1(-1, -1);
  PointF s2(1, -1);
  PointF s3(1, 1);
  PointF s4(-1, 1);
  // Top edge.
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.1f, -1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.0f, -1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(0.0f, -1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(1.0f, -1.0f)));
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(1.1f, -1.0f)));
  // Bottom edge.
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.1f, 1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.0f, 1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(0.0f, 1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(1.0f, 1.0f)));
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(1.1f, 1.0f)));
  // Left edge.
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.0f, -1.1f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.0f, -1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.0f, 0.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.0f, 1.0f)));
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.0f, 1.1f)));
  // Right edge.
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(1.0f, -1.1f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(1.0f, -1.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(1.0f, 0.0f)));
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(1.0f, 1.0f)));
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(1.0f, 1.1f)));
  // Centered inside.
  EXPECT_TRUE(QuadF(s1, s2, s3, s4).Contains(PointF(0, 0)));
  // Centered outside.
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(-1.1f, 0)));
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(1.1f, 0)));
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(0, -1.1f)));
  EXPECT_FALSE(QuadF(s1, s2, s3, s4).Contains(PointF(0, 1.1f)));
}

TEST(QuadFTest, ContainsQuad) {
  QuadF quad(PointF(10, 0), PointF(20, 10), PointF(10, 20), PointF(0, 10));
  EXPECT_TRUE(quad.ContainsQuad(quad));
  EXPECT_TRUE(QuadF(quad.BoundingBox()).ContainsQuad(quad));
  EXPECT_FALSE(quad.ContainsQuad(QuadF(quad.BoundingBox())));

  EXPECT_FALSE(quad.ContainsQuad(QuadF(RectF(4.9, 4.9, 9.8, 9.8))));
  EXPECT_TRUE(quad.ContainsQuad(QuadF(RectF(5.1, 5.1, 9.8, 9.8))));
  EXPECT_FALSE(quad.ContainsQuad(QuadF(RectF(5.1, 5.1, 11, 9.8))));
  EXPECT_FALSE(quad.ContainsQuad(QuadF(RectF(5.1, 5.1, 9.8, 11))));

  EXPECT_FALSE(quad.ContainsQuad(quad + gfx::Vector2dF(0.1, 0)));
  EXPECT_FALSE(quad.ContainsQuad(quad + gfx::Vector2dF(0, 0.1)));
  EXPECT_FALSE(quad.ContainsQuad(quad - gfx::Vector2dF(0.1, 0)));
  EXPECT_FALSE(quad.ContainsQuad(quad - gfx::Vector2dF(0, 0.1)));
}

TEST(QuadFTest, Scale) {
  PointF a(1.3f, 1.4f);
  PointF b(-0.8f, 4.4f);
  PointF c(1.8f, 6.1f);
  PointF d(2.1f, 1.6f);
  QuadF q1(a, b, c, d);
  q1.Scale(1.5f);

  PointF a_scaled = ScalePoint(a, 1.5f);
  PointF b_scaled = ScalePoint(b, 1.5f);
  PointF c_scaled = ScalePoint(c, 1.5f);
  PointF d_scaled = ScalePoint(d, 1.5f);
  EXPECT_EQ(q1, QuadF(a_scaled, b_scaled, c_scaled, d_scaled));

  QuadF q2;
  q2.Scale(1.5f);
  EXPECT_EQ(q2, q2);
}

TEST(QuadFTest, IntersectsRectClockwise) {
  QuadF quad(PointF(10, 0), PointF(20, 10), PointF(10, 20), PointF(0, 10));

  // Top-left.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 0, 4.9, 4.9)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 4.9, 6)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 5.1, 5.1)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 6, 4.9)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 0, 2, 6)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 0, 6, 2)));

  // Top.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, -30, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, -5, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, -5, 20, 4.9)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, -5, 20, 5.1)));

  // Top-right.
  EXPECT_FALSE(quad.IntersectsRect(RectF(15.1, 0, 10, 4.9)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(15.1, 0, 10, 6)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14.9, 0, 10, 5.1)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14, 0, 10, 4.9)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(18, 0, 10, 6)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(14, 0, 10, 2)));

  // Right.
  EXPECT_FALSE(quad.IntersectsRect(RectF(50, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(22, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(20.1, 0, 2, 20)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(19.9, 0, 2, 20)));

  // Bottom-right.
  EXPECT_FALSE(quad.IntersectsRect(RectF(15.1, 15.1, 10, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(15.1, 14, 10, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14.9, 14.9, 10, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14, 15.1, 10, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(18, 14, 10, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(14, 18, 10, 10)));

  // Bottom.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 50, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 22, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 20.1, 20, 2)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 19.9, 20, 2)));

  // Bottom-left.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 15.1, 4.9, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 15.1, 6, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 14.9, 5.1, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 14, 4.9, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 18, 6, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 14, 2, 10)));

  // Left.
  EXPECT_FALSE(quad.IntersectsRect(RectF(-30, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(-5, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(-5, 0, 4.9, 20)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(-5, 0, 5.1, 20)));

  // Cover.
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 20, 20)));
}

TEST(QuadFTest, IntersectsRectCounterClockwise) {
  QuadF quad(PointF(10, 0), PointF(0, 10), PointF(10, 20), PointF(20, 10));

  // Top-left.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 0, 4.9, 4.9)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 4.9, 6)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 5.1, 5.1)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 6, 4.9)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 0, 2, 6)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 0, 6, 2)));

  // Top.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, -30, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, -5, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, -5, 20, 4.9)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, -5, 20, 5.1)));

  // Top-right.
  EXPECT_FALSE(quad.IntersectsRect(RectF(15.1, 0, 10, 4.9)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(15.1, 0, 10, 6)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14.9, 0, 10, 5.1)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14, 0, 10, 4.9)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(18, 0, 10, 6)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(14, 0, 10, 2)));

  // Right.
  EXPECT_FALSE(quad.IntersectsRect(RectF(50, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(22, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(20.1, 0, 2, 20)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(19.9, 0, 2, 20)));

  // Bottom-right.
  EXPECT_FALSE(quad.IntersectsRect(RectF(15.1, 15.1, 10, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(15.1, 14, 10, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14.9, 14.9, 10, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(14, 15.1, 10, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(18, 14, 10, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(14, 18, 10, 10)));

  // Bottom.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 50, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 22, 20, 2)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 20.1, 20, 2)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 19.9, 20, 2)));

  // Bottom-left.
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 15.1, 4.9, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 15.1, 6, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 14.9, 5.1, 10)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 14, 4.9, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 18, 6, 10)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(0, 14, 2, 10)));

  // Left.
  EXPECT_FALSE(quad.IntersectsRect(RectF(-30, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(-5, 0, 2, 20)));
  EXPECT_FALSE(quad.IntersectsRect(RectF(-5, 0, 4.9, 20)));
  EXPECT_TRUE(quad.IntersectsRect(RectF(-5, 0, 5.1, 20)));

  // Cover.
  EXPECT_TRUE(quad.IntersectsRect(RectF(0, 0, 20, 20)));
}

TEST(QuadFTest, RectIntersectionIsInclusive) {
  // A rectilinear quad at (10, 10) with dimensions 10x10.
  QuadF quad(RectF(10, 10, 10, 10));

  // A rect fully contained in the quad should intersect.
  EXPECT_TRUE(quad.IntersectsRect(RectF(11, 11, 8, 8)));
  EXPECT_TRUE(quad.IntersectsRectPartial(RectF(11, 11, 8, 8)));

  // A point fully contained in the quad should intersect.
  EXPECT_TRUE(quad.IntersectsRect(RectF(11, 11, 0, 0)));
  EXPECT_TRUE(quad.IntersectsRectPartial(RectF(11, 11, 0, 0)));

  // A rect that touches the quad only at the point (10, 10) should intersect.
  EXPECT_TRUE(quad.IntersectsRect(RectF(9, 9, 1, 1)));
  EXPECT_TRUE(quad.IntersectsRectPartial(RectF(9, 9, 1, 1)));

  // A rect that touches the quad only on the left edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(RectF(9, 11, 1, 1)));
  EXPECT_TRUE(quad.IntersectsRectPartial(RectF(9, 11, 1, 1)));

  // A rect that touches the quad only on the top edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(RectF(11, 9, 1, 1)));
  EXPECT_TRUE(quad.IntersectsRectPartial(RectF(11, 9, 1, 1)));

  // A rect that touches the quad only on the right edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(RectF(20, 11, 1, 1)));
  EXPECT_TRUE(quad.IntersectsRectPartial(RectF(20, 11, 1, 1)));

  // A rect that touches the quad only on the bottom edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(RectF(11, 20, 1, 1)));
  EXPECT_TRUE(quad.IntersectsRectPartial(RectF(11, 20, 1, 1)));

  // A rect that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsRect(RectF(8, 8, 1, 1)));
  EXPECT_FALSE(quad.IntersectsRectPartial(RectF(8, 8, 1, 1)));

  // A point that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsRect(RectF(9, 9, 0, 0)));
  EXPECT_FALSE(quad.IntersectsRectPartial(RectF(9, 9, 0, 0)));

  // A rect that is not fully to the left of any of the quad's edges.
  QuadF quad2(PointF(100, 100), PointF(80, 120), PointF(50, 80),
              PointF(60, 40));
  EXPECT_FALSE(quad2.IntersectsRect(RectF(60, 130, 30, 10)));
}

TEST(FloatQuadTest, QuadIntersection) {
  // Clockwise (convex) quad.
  // (Centroid = { 72.5, 85 }, Min = { 50, 40 }, Max = { 100, 120 })
  QuadF cw_quad(PointF(100, 100), PointF(80, 120), PointF(50, 80),
                PointF(60, 40));

  // All points contained.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(72.5, 85, 1, 1))));

  // All points contained (degenerate).
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(72.5, 85, 0, 0))));

  // One point contained.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(72.5, 85, 100, 100))));

  // Quad contained by other quad.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(40, 30, 70, 100))));

  // Touching a single point.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(80, 120, 100, 100))));

  // Touching p1 - p2 edge.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(90, 110, 10, 10))));

  // Touching p2 - p3 edge.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(55, 100, 10, 10))));

  // Touching p3 - p4 edge.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(45, 50, 10, 10))));

  // Touching p4 - p1 edge.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(80, 60, 10, 10))));

  // Edge crossing (but no points in quad.)
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(RectF(50, 30, 50, 15))));

  // Co-linear edges.
  EXPECT_TRUE(cw_quad.IntersectsQuad(QuadF(PointF(20, 40), PointF(110, 160),
                                           PointF(70, 160), PointF(10, 40))));

  // Fully outside.
  // L = { 80, 120 } + t * { -30, -40 }
  EXPECT_FALSE(cw_quad.IntersectsQuad(
      QuadF(PointF(10, 40), PointF(100, 160), PointF(70, 160), PointF(0, 40))));
  // L = { 100, 100 } + t * { -20, 20 }
  EXPECT_FALSE(cw_quad.IntersectsQuad(QuadF(PointF(120, 90), PointF(80, 130),
                                            PointF(90, 130), PointF(130, 90))));
  // L = { 50, 80 } + t * { 10, -40 }
  EXPECT_FALSE(cw_quad.IntersectsQuad(
      QuadF(PointF(35, 100), PointF(55, 20), PointF(45, 20), PointF(25, 100))));
  // L = { 60, 40 } + t * { 40, 60 }
  EXPECT_FALSE(cw_quad.IntersectsQuad(QuadF(PointF(50, 10), PointF(130, 130),
                                            PointF(150, 130), PointF(80, 10))));
  // BBox = { { 50, 40 }, { 100, 120 } }; w = 50, h = 80
  EXPECT_FALSE(cw_quad.IntersectsQuad(QuadF(RectF(110, 50, 10, 60))));
  EXPECT_FALSE(cw_quad.IntersectsQuad(QuadF(RectF(60, 130, 30, 10))));
  EXPECT_FALSE(cw_quad.IntersectsQuad(QuadF(RectF(30, 50, 10, 60))));
  EXPECT_FALSE(cw_quad.IntersectsQuad(QuadF(RectF(60, 20, 30, 10))));

  // Same quad, but counter-clockwise.
  // (Centroid = { 72.5, 85 }, Min = { 50, 40 }, Max = { 100, 120 })
  QuadF ccw_quad(PointF(100, 100), PointF(60, 40), PointF(50, 80),
                 PointF(80, 120));

  // All points contained.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(72.5, 85, 1, 1))));

  // One point contained.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(72.5, 85, 100, 100))));

  // Quad contained by other quad.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(40, 30, 70, 100))));

  // Touching a single point.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(80, 120, 100, 100))));

  // Touching p1 - p2 edge.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(90, 110, 10, 10))));

  // Touching p2 - p3 edge.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(55, 100, 10, 10))));

  // Touching p3 - p4 edge.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(45, 50, 10, 10))));

  // Touching p4 - p1 edge.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(80, 60, 10, 10))));

  // Edge crossing (but no points in quad.)
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(RectF(50, 30, 50, 15))));

  // Co-linear edges.
  EXPECT_TRUE(ccw_quad.IntersectsQuad(QuadF(PointF(20, 40), PointF(110, 160),
                                            PointF(70, 160), PointF(10, 40))));

  // Fully outside.
  // L = { 80, 120 } + t * { -30, -40 }
  EXPECT_FALSE(ccw_quad.IntersectsQuad(
      QuadF(PointF(10, 40), PointF(100, 160), PointF(70, 160), PointF(0, 40))));
  // L = { 100, 100 } + t * { -20, 20 }
  EXPECT_FALSE(ccw_quad.IntersectsQuad(QuadF(
      PointF(120, 90), PointF(80, 130), PointF(90, 130), PointF(130, 90))));
  // L = { 50, 80 } + t * { 10, -40 }
  EXPECT_FALSE(ccw_quad.IntersectsQuad(
      QuadF(PointF(35, 100), PointF(55, 20), PointF(45, 20), PointF(25, 100))));
  // L = { 60, 40 } + t * { 40, 60 }
  EXPECT_FALSE(ccw_quad.IntersectsQuad(QuadF(
      PointF(50, 10), PointF(130, 130), PointF(150, 130), PointF(80, 10))));
  // BBox = { { 50, 40 }, { 100, 120 } }; w = 50, h = 80
  EXPECT_FALSE(ccw_quad.IntersectsQuad(QuadF(RectF(110, 50, 10, 60))));
  EXPECT_FALSE(ccw_quad.IntersectsQuad(QuadF(RectF(60, 130, 30, 10))));
  EXPECT_FALSE(ccw_quad.IntersectsQuad(QuadF(RectF(30, 50, 10, 60))));
  EXPECT_FALSE(ccw_quad.IntersectsQuad(QuadF(RectF(60, 20, 30, 10))));
}

TEST(QuadFTest, IntersectsEllipseClockWise) {
  QuadF quad(PointF(10, 0), PointF(20, 10), PointF(10, 20), PointF(0, 10));

  // Top-left.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 0), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 0), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 0), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 0), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 0), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 0), SizeF(8, 2)));

  // Top.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, -5), SizeF(20, 2)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, -5), SizeF(20, 4.9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(10, -5), SizeF(20, 5.1)));

  // Top-right.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 0), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 0), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 0), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 0), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 0), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 0), SizeF(8, 2)));

  // Right.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(25, 10), SizeF(2, 20)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(25, 10), SizeF(4.9, 20)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(25, 10), SizeF(5.1, 20)));

  // Bottom-right.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 20), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 20), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 20), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 20), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 20), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 20), SizeF(8, 2)));

  // Bottom.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, 25), SizeF(20, 2)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, 25), SizeF(20, 4.9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(10, 25), SizeF(20, 5.1)));

  // Bottom-left.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 20), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 20), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 20), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 20), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 20), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 20), SizeF(8, 2)));

  // Left.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(-5, 10), SizeF(2, 20)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(-5, 10), SizeF(4.9, 20)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(-5, 10), SizeF(5.1, 20)));
}

TEST(QuadFTest, IntersectsEllipseCounterClockwise) {
  QuadF quad(PointF(10, 0), PointF(0, 10), PointF(10, 20), PointF(20, 10));

  // Top-left.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 0), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 0), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 0), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 0), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 0), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 0), SizeF(8, 2)));

  // Top.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, -5), SizeF(20, 2)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, -5), SizeF(20, 4.9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(10, -5), SizeF(20, 5.1)));

  // Top-right.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 0), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 0), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 0), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 0), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 0), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 0), SizeF(8, 2)));

  // Right.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(25, 10), SizeF(2, 20)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(25, 10), SizeF(4.9, 20)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(25, 10), SizeF(5.1, 20)));

  // Bottom-right.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 20), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 20), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 20), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(20, 20), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 20), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(20, 20), SizeF(8, 2)));

  // Bottom.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, 25), SizeF(20, 2)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(10, 25), SizeF(20, 4.9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(10, 25), SizeF(20, 5.1)));

  // Bottom-left.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 20), SizeF(7, 7)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 20), SizeF(6, 9)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 20), SizeF(7.1, 7.1)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(0, 20), SizeF(9, 6)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 20), SizeF(2, 8)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(0, 20), SizeF(8, 2)));

  // Left.
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(-5, 10), SizeF(2, 20)));
  EXPECT_FALSE(quad.IntersectsEllipse(PointF(-5, 10), SizeF(4.9, 20)));
  EXPECT_TRUE(quad.IntersectsEllipse(PointF(-5, 10), SizeF(5.1, 20)));
}

TEST(QuadFTest, CircleIntersectionIsInclusive) {
  // A rectilinear quad at (10, 10) with dimensions 10x10.
  QuadF quad(RectF(10, 10, 10, 10));

  // A circle fully contained in the top-left of the quad should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(12, 12), 1));

  // A point fully contained in the top-left of the quad should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(12, 12), 0));

  // A circle that touches the left edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(9, 11), 1));

  // A circle that touches the top edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(11, 9), 1));

  // A circle that touches the right edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(21, 11), 1));

  // A circle that touches the bottom edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(11, 21), 1));

  // A point that touches the left edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(10, 11), 0));

  // A point that touches the top edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(11, 10), 0));

  // A point that touches the right edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(20, 11), 0));

  // A point that touches the bottom edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(PointF(11, 20), 0));

  // A circle that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsCircle(PointF(9, 9), 1));

  // A point that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsCircle(PointF(9, 9), 0));
}

TEST(QuadFTest, CenterPoint) {
  EXPECT_EQ(PointF(), QuadF().CenterPoint());
  EXPECT_EQ(PointF(25.75f, 40.75f),
            QuadF(RectF(10.5f, 20.5f, 30.5f, 40.5f)).CenterPoint());
  EXPECT_EQ(PointF(10, 10),
            QuadF(PointF(10, 0), PointF(20, 10), PointF(10, 20), PointF(0, 10))
                .CenterPoint());
}

}  // namespace gfx

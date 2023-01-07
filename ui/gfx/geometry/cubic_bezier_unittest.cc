// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/cubic_bezier.h"

#include <cmath>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace {

TEST(CubicBezierTest, Basic) {
  CubicBezier function(0.25, 0.0, 0.75, 1.0);

  double epsilon = 0.00015;

  EXPECT_NEAR(function.Solve(0), 0, epsilon);
  EXPECT_NEAR(function.Solve(0.05), 0.01136, epsilon);
  EXPECT_NEAR(function.Solve(0.1), 0.03978, epsilon);
  EXPECT_NEAR(function.Solve(0.15), 0.079780, epsilon);
  EXPECT_NEAR(function.Solve(0.2), 0.12803, epsilon);
  EXPECT_NEAR(function.Solve(0.25), 0.18235, epsilon);
  EXPECT_NEAR(function.Solve(0.3), 0.24115, epsilon);
  EXPECT_NEAR(function.Solve(0.35), 0.30323, epsilon);
  EXPECT_NEAR(function.Solve(0.4), 0.36761, epsilon);
  EXPECT_NEAR(function.Solve(0.45), 0.43345, epsilon);
  EXPECT_NEAR(function.Solve(0.5), 0.5, epsilon);
  EXPECT_NEAR(function.Solve(0.6), 0.63238, epsilon);
  EXPECT_NEAR(function.Solve(0.65), 0.69676, epsilon);
  EXPECT_NEAR(function.Solve(0.7), 0.75884, epsilon);
  EXPECT_NEAR(function.Solve(0.75), 0.81764, epsilon);
  EXPECT_NEAR(function.Solve(0.8), 0.87196, epsilon);
  EXPECT_NEAR(function.Solve(0.85), 0.92021, epsilon);
  EXPECT_NEAR(function.Solve(0.9), 0.96021, epsilon);
  EXPECT_NEAR(function.Solve(0.95), 0.98863, epsilon);
  EXPECT_NEAR(function.Solve(1), 1, epsilon);

  CubicBezier basic_use(0.5, 1.0, 0.5, 1.0);
  EXPECT_EQ(0.875, basic_use.Solve(0.5));

  CubicBezier overshoot(0.5, 2.0, 0.5, 2.0);
  EXPECT_EQ(1.625, overshoot.Solve(0.5));

  CubicBezier undershoot(0.5, -1.0, 0.5, -1.0);
  EXPECT_EQ(-0.625, undershoot.Solve(0.5));
}

// Tests that solving the bezier works with knots with y not in (0, 1).
TEST(CubicBezierTest, UnclampedYValues) {
  CubicBezier function(0.5, -1.0, 0.5, 2.0);

  double epsilon = 0.00015;

  EXPECT_NEAR(function.Solve(0.0), 0.0, epsilon);
  EXPECT_NEAR(function.Solve(0.05), -0.08954, epsilon);
  EXPECT_NEAR(function.Solve(0.1), -0.15613, epsilon);
  EXPECT_NEAR(function.Solve(0.15), -0.19641, epsilon);
  EXPECT_NEAR(function.Solve(0.2), -0.20651, epsilon);
  EXPECT_NEAR(function.Solve(0.25), -0.18232, epsilon);
  EXPECT_NEAR(function.Solve(0.3), -0.11992, epsilon);
  EXPECT_NEAR(function.Solve(0.35), -0.01672, epsilon);
  EXPECT_NEAR(function.Solve(0.4), 0.12660, epsilon);
  EXPECT_NEAR(function.Solve(0.45), 0.30349, epsilon);
  EXPECT_NEAR(function.Solve(0.5), 0.50000, epsilon);
  EXPECT_NEAR(function.Solve(0.55), 0.69651, epsilon);
  EXPECT_NEAR(function.Solve(0.6), 0.87340, epsilon);
  EXPECT_NEAR(function.Solve(0.65), 1.01672, epsilon);
  EXPECT_NEAR(function.Solve(0.7), 1.11992, epsilon);
  EXPECT_NEAR(function.Solve(0.75), 1.18232, epsilon);
  EXPECT_NEAR(function.Solve(0.8), 1.20651, epsilon);
  EXPECT_NEAR(function.Solve(0.85), 1.19641, epsilon);
  EXPECT_NEAR(function.Solve(0.9), 1.15613, epsilon);
  EXPECT_NEAR(function.Solve(0.95), 1.08954, epsilon);
  EXPECT_NEAR(function.Solve(1.0), 1.0, epsilon);
}

static void TestBezierFiniteRange(CubicBezier& function) {
  for (double i = 0; i <= 1.01; i += 0.05) {
    EXPECT_TRUE(std::isfinite(function.Solve(i)));
    EXPECT_TRUE(std::isfinite(function.Slope(i)));
    EXPECT_TRUE(std::isfinite(function.GetX2()));
    EXPECT_TRUE(std::isfinite(function.GetY2()));
    EXPECT_TRUE(std::isfinite(function.SampleCurveX(i)));
    EXPECT_TRUE(std::isfinite(function.SampleCurveY(i)));
    EXPECT_TRUE(std::isfinite(function.SampleCurveDerivativeX(i)));
    EXPECT_TRUE(std::isfinite(function.SampleCurveDerivativeY(i)));
  }
}

// Tests that solving the bezier works with huge value infinity evaluation
TEST(CubicBezierTest, ClampInfinityEvaluation) {
  auto test_cases = {
      CubicBezier(0.5, std::numeric_limits<double>::max(), 0.5,
                  std::numeric_limits<double>::max()),
      CubicBezier(0.5, std::numeric_limits<double>::lowest(), 0.5,
                  std::numeric_limits<double>::max()),
      CubicBezier(0.5, std::numeric_limits<double>::max(), 0.5,
                  std::numeric_limits<double>::lowest()),
      CubicBezier(0.5, std::numeric_limits<double>::lowest(), 0.5,
                  std::numeric_limits<double>::lowest()),

      CubicBezier(0, std::numeric_limits<double>::max(), 0,
                  std::numeric_limits<double>::max()),
      CubicBezier(0, std::numeric_limits<double>::lowest(), 0,
                  std::numeric_limits<double>::max()),
      CubicBezier(0, std::numeric_limits<double>::max(), 0,
                  std::numeric_limits<double>::lowest()),
      CubicBezier(0, std::numeric_limits<double>::lowest(), 0,
                  std::numeric_limits<double>::lowest()),

      CubicBezier(1, std::numeric_limits<double>::max(), 1,
                  std::numeric_limits<double>::max()),
      CubicBezier(1, std::numeric_limits<double>::lowest(), 1,
                  std::numeric_limits<double>::max()),
      CubicBezier(1, std::numeric_limits<double>::max(), 1,
                  std::numeric_limits<double>::lowest()),
      CubicBezier(1, std::numeric_limits<double>::lowest(), 1,
                  std::numeric_limits<double>::lowest()),

      CubicBezier(0, 0, 0, std::numeric_limits<double>::max()),
      CubicBezier(0, std::numeric_limits<double>::lowest(), 0, 0),
      CubicBezier(1, 0, 0, std::numeric_limits<double>::lowest()),
      CubicBezier(0, std::numeric_limits<double>::lowest(), 1, 1),

  };

  for (auto tc : test_cases) {
    TestBezierFiniteRange(tc);
  }
}

TEST(CubicBezierTest, Range) {
  double epsilon = 0.00015;

  // Derivative is a constant.
  std::unique_ptr<CubicBezier> function =
      std::make_unique<CubicBezier>(0.25, (1.0 / 3.0), 0.75, (2.0 / 3.0));
  EXPECT_EQ(0, function->range_min());
  EXPECT_EQ(1, function->range_max());

  // Derivative is linear.
  function = std::make_unique<CubicBezier>(0.25, -0.5, 0.75, (-1.0 / 6.0));
  EXPECT_NEAR(function->range_min(), -0.225, epsilon);
  EXPECT_EQ(1, function->range_max());

  // Derivative has no real roots.
  function = std::make_unique<CubicBezier>(0.25, 0.25, 0.75, 0.5);
  EXPECT_EQ(0, function->range_min());
  EXPECT_EQ(1, function->range_max());

  // Derivative has exactly one real root.
  function = std::make_unique<CubicBezier>(0.0, 1.0, 1.0, 0.0);
  EXPECT_EQ(0, function->range_min());
  EXPECT_EQ(1, function->range_max());

  // Derivative has one root < 0 and one root > 1.
  function = std::make_unique<CubicBezier>(0.25, 0.1, 0.75, 0.9);
  EXPECT_EQ(0, function->range_min());
  EXPECT_EQ(1, function->range_max());

  // Derivative has two roots in [0,1].
  function = std::make_unique<CubicBezier>(0.25, 2.5, 0.75, 0.5);
  EXPECT_EQ(0, function->range_min());
  EXPECT_NEAR(function->range_max(), 1.28818, epsilon);
  function = std::make_unique<CubicBezier>(0.25, 0.5, 0.75, -1.5);
  EXPECT_NEAR(function->range_min(), -0.28818, epsilon);
  EXPECT_EQ(1, function->range_max());

  // Derivative has one root < 0 and one root in [0,1].
  function = std::make_unique<CubicBezier>(0.25, 0.1, 0.75, 1.5);
  EXPECT_EQ(0, function->range_min());
  EXPECT_NEAR(function->range_max(), 1.10755, epsilon);

  // Derivative has one root in [0,1] and one root > 1.
  function = std::make_unique<CubicBezier>(0.25, -0.5, 0.75, 0.9);
  EXPECT_NEAR(function->range_min(), -0.10755, epsilon);
  EXPECT_EQ(1, function->range_max());

  // Derivative has two roots < 0.
  function = std::make_unique<CubicBezier>(0.25, 0.3, 0.75, 0.633);
  EXPECT_EQ(0, function->range_min());
  EXPECT_EQ(1, function->range_max());

  // Derivative has two roots > 1.
  function = std::make_unique<CubicBezier>(0.25, 0.367, 0.75, 0.7);
  EXPECT_EQ(0.f, function->range_min());
  EXPECT_EQ(1.f, function->range_max());
}

TEST(CubicBezierTest, Slope) {
  CubicBezier function(0.25, 0.0, 0.75, 1.0);

  double epsilon = 0.00015;

  EXPECT_NEAR(function.Slope(-0.1), 0, epsilon);
  EXPECT_NEAR(function.Slope(0), 0, epsilon);
  EXPECT_NEAR(function.Slope(0.05), 0.42170, epsilon);
  EXPECT_NEAR(function.Slope(0.1), 0.69778, epsilon);
  EXPECT_NEAR(function.Slope(0.15), 0.89121, epsilon);
  EXPECT_NEAR(function.Slope(0.2), 1.03184, epsilon);
  EXPECT_NEAR(function.Slope(0.25), 1.13576, epsilon);
  EXPECT_NEAR(function.Slope(0.3), 1.21239, epsilon);
  EXPECT_NEAR(function.Slope(0.35), 1.26751, epsilon);
  EXPECT_NEAR(function.Slope(0.4), 1.30474, epsilon);
  EXPECT_NEAR(function.Slope(0.45), 1.32628, epsilon);
  EXPECT_NEAR(function.Slope(0.5), 1.33333, epsilon);
  EXPECT_NEAR(function.Slope(0.55), 1.32628, epsilon);
  EXPECT_NEAR(function.Slope(0.6), 1.30474, epsilon);
  EXPECT_NEAR(function.Slope(0.65), 1.26751, epsilon);
  EXPECT_NEAR(function.Slope(0.7), 1.21239, epsilon);
  EXPECT_NEAR(function.Slope(0.75), 1.13576, epsilon);
  EXPECT_NEAR(function.Slope(0.8), 1.03184, epsilon);
  EXPECT_NEAR(function.Slope(0.85), 0.89121, epsilon);
  EXPECT_NEAR(function.Slope(0.9), 0.69778, epsilon);
  EXPECT_NEAR(function.Slope(0.95), 0.42170, epsilon);
  EXPECT_NEAR(function.Slope(1), 0, epsilon);
  EXPECT_NEAR(function.Slope(1.1), 0, epsilon);
}

TEST(CubicBezierTest, InputOutOfRange) {
  CubicBezier simple(0.5, 1.0, 0.5, 1.0);
  EXPECT_EQ(-2.0, simple.Solve(-1.0));
  EXPECT_EQ(1.0, simple.Solve(2.0));

  CubicBezier at_edge_of_range(0.5, 1.0, 0.5, 1.0);
  EXPECT_EQ(0.0, at_edge_of_range.Solve(0.0));
  EXPECT_EQ(1.0, at_edge_of_range.Solve(1.0));

  CubicBezier large_epsilon(0.5, 1.0, 0.5, 1.0);
  EXPECT_EQ(-2.0, large_epsilon.SolveWithEpsilon(-1.0, 1.0));
  EXPECT_EQ(1.0, large_epsilon.SolveWithEpsilon(2.0, 1.0));

  CubicBezier coincident_endpoints(0.0, 0.0, 1.0, 1.0);
  EXPECT_EQ(-1.0, coincident_endpoints.Solve(-1.0));
  EXPECT_EQ(2.0, coincident_endpoints.Solve(2.0));

  CubicBezier vertical_gradient(0.0, 1.0, 1.0, 0.0);
  EXPECT_EQ(0.0, vertical_gradient.Solve(-1.0));
  EXPECT_EQ(1.0, vertical_gradient.Solve(2.0));

  CubicBezier vertical_trailing_gradient(0.5, 0.0, 1.0, 0.5);
  EXPECT_EQ(0.0, vertical_trailing_gradient.Solve(-1.0));
  EXPECT_EQ(1.0, vertical_trailing_gradient.Solve(2.0));

  CubicBezier distinct_endpoints(0.1, 0.2, 0.8, 0.8);
  EXPECT_EQ(-2.0, distinct_endpoints.Solve(-1.0));
  EXPECT_EQ(2.0, distinct_endpoints.Solve(2.0));

  CubicBezier coincident_leading_endpoint(0.0, 0.0, 0.5, 1.0);
  EXPECT_EQ(-2.0, coincident_leading_endpoint.Solve(-1.0));
  EXPECT_EQ(1.0, coincident_leading_endpoint.Solve(2.0));

  CubicBezier coincident_trailing_endpoint(1.0, 0.5, 1.0, 1.0);
  EXPECT_EQ(-0.5, coincident_trailing_endpoint.Solve(-1.0));
  EXPECT_EQ(1.0, coincident_trailing_endpoint.Solve(2.0));

  // Two special cases with three coincident points. Both are equivalent to
  // linear.
  CubicBezier all_zeros(0.0, 0.0, 0.0, 0.0);
  EXPECT_EQ(-1.0, all_zeros.Solve(-1.0));
  EXPECT_EQ(2.0, all_zeros.Solve(2.0));

  CubicBezier all_ones(1.0, 1.0, 1.0, 1.0);
  EXPECT_EQ(-1.0, all_ones.Solve(-1.0));
  EXPECT_EQ(2.0, all_ones.Solve(2.0));
}

TEST(CubicBezierTest, GetPoints) {
  double epsilon = 0.00015;

  CubicBezier cubic1(0.1, 0.2, 0.8, 0.9);
  EXPECT_NEAR(0.1, cubic1.GetX1(), epsilon);
  EXPECT_NEAR(0.2, cubic1.GetY1(), epsilon);
  EXPECT_NEAR(0.8, cubic1.GetX2(), epsilon);
  EXPECT_NEAR(0.9, cubic1.GetY2(), epsilon);

  CubicBezier cubic_zero(0, 0, 0, 0);
  EXPECT_NEAR(0, cubic_zero.GetX1(), epsilon);
  EXPECT_NEAR(0, cubic_zero.GetY1(), epsilon);
  EXPECT_NEAR(0, cubic_zero.GetX2(), epsilon);
  EXPECT_NEAR(0, cubic_zero.GetY2(), epsilon);

  CubicBezier cubic_one(1, 1, 1, 1);
  EXPECT_NEAR(1, cubic_one.GetX1(), epsilon);
  EXPECT_NEAR(1, cubic_one.GetY1(), epsilon);
  EXPECT_NEAR(1, cubic_one.GetX2(), epsilon);
  EXPECT_NEAR(1, cubic_one.GetY2(), epsilon);

  CubicBezier cubic_oor(-0.5, -1.5, 1.5, -1.6);
  EXPECT_NEAR(-0.5, cubic_oor.GetX1(), epsilon);
  EXPECT_NEAR(-1.5, cubic_oor.GetY1(), epsilon);
  EXPECT_NEAR(1.5, cubic_oor.GetX2(), epsilon);
  EXPECT_NEAR(-1.6, cubic_oor.GetY2(), epsilon);
}

void validateSolver(const CubicBezier& cubic_bezier) {
  const double epsilon = 1e-7;
  const double precision = 1e-5;
  for (double t = 0; t <= 1; t += 0.05) {
    double x = cubic_bezier.SampleCurveX(t);
    double root = cubic_bezier.SolveCurveX(x, epsilon);
    EXPECT_NEAR(t, root, precision);
  }
}

TEST(CubicBezierTest, CommonEasingFunctions) {
  validateSolver(CubicBezier(0.25, 0.1, 0.25, 1));  // ease
  validateSolver(CubicBezier(0.42, 0, 1, 1));       // ease-in
  validateSolver(CubicBezier(0, 0, 0.58, 1));       // ease-out
  validateSolver(CubicBezier(0.42, 0, 0.58, 1));    // ease-in-out
}

TEST(CubicBezierTest, LinearEquivalentBeziers) {
  validateSolver(CubicBezier(0.0, 0.0, 0.0, 0.0));
  validateSolver(CubicBezier(1.0, 1.0, 1.0, 1.0));
}

TEST(CubicBezierTest, ControlPointsOutsideUnitSquare) {
  validateSolver(CubicBezier(0.3, 1.5, 0.8, 1.5));
  validateSolver(CubicBezier(0.4, -0.8, 0.7, 1.7));
  validateSolver(CubicBezier(0.7, -2.0, 1.0, -1.5));
  validateSolver(CubicBezier(0, 4, 1, -3));
}

}  // namespace
}  // namespace gfx

/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/nelder_mead_searcher.h"

#include "audio/dsp/testing_util.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {

template <typename Scalar>
using Vector = typename NelderMeadSearcher<Scalar>::Vector;
template <typename Scalar>
using Matrix = typename NelderMeadSearcher<Scalar>::Matrix;

// The N-dimensional Rosenbrock function is a standard test case for non-linear
// optimization and is defined as
//   f(x1,x2,...,xN) = \sum_{i=1}({N-1} 100 * (x_{i+1} - x_i^2)^2 - (1 - x_i)^2
template <typename Scalar>
typename Eigen::NumTraits<Scalar>::Real Rosenbrock(
    const Eigen::Ref<const Vector<Scalar>>& v) {
  const int n = v.size();
  auto v0 = v.head(n - 1);
  auto v1 = v.tail(n - 1);
  return (100 * (v1 - v0.abs2()).abs2() + (1 - v0).abs2()).sum();
}

TEST(NelderMeadSearcherTest, Construct) {
  const int kDims = 10;
  NelderMeadSearcher<double> searcher(kDims);
}

TEST(NelderMeadSearcherTest, SetGetSimplex) {
  const int kDims = 2;
  NelderMeadSearcher<double> searcher(kDims);
  Matrix<double> simplex;
  simplex.setOnes(kDims, kDims + 1);
  searcher.SetSimplex(simplex);
  EXPECT_THAT(searcher.GetSimplex(), EigenArrayNear(simplex, 0));

  Vector<double> starting_guess;
  starting_guess.setZero(kDims);
  // This should set the first column to the starting guess and the
  // right-most kDim-by-kDim to the identity.
  searcher.SetSimplexFromStartingGuess(starting_guess, 1.0);
  simplex.setZero();
  simplex.rightCols(kDims) = Eigen::MatrixXd::Identity(kDims, kDims);
  EXPECT_THAT(searcher.GetSimplex(), EigenArrayNear(simplex, 0));

  // Test Span interface.
  searcher.SetSimplexFromStartingGuessSpan({1.0, 2.0}, 1.0);
  EXPECT_THAT(searcher.GetSimplex(),
              EigenArrayNear<double>({{1.0, 2.0, 1.0}, {2.0, 2.0, 3.0}}, 0));
}

TEST(NelderMeadSearcherTest, Rosenbrock_2D) {
  const int kDims = 2;
  const int kMaxEvaluations = 200;
  const double kStdDevTolerance = 0.00005;
  const double kArgMinTolerance = 0.02;
  const double kMinTolerance = 0.001;
  const double kObjectiveGoal = -std::numeric_limits<double>::infinity();
  NelderMeadSearcher<double> searcher(kDims);
  searcher.SetObjectiveFunction(Rosenbrock<double>);
  Vector<double> starting_guess;
  starting_guess.setZero(kDims);
  searcher.SetSimplexFromStartingGuess(starting_guess, 1.0);
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  EXPECT_THAT(searcher.GetArgMin(),
              EigenArrayNear<double>({1.0, 1.0}, kArgMinTolerance));
  EXPECT_NEAR(searcher.GetMin(), 0, kMinTolerance);
  EXPECT_EQ(searcher.num_evaluations(), 81);

  // Try a different starting guess.
  searcher.SetSimplexFromStartingGuessSpan({-1.0, 1.0}, 1.0);
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  EXPECT_THAT(searcher.GetArgMin(),
              EigenArrayNear<double>({1.0, 1.0}, kArgMinTolerance));
  EXPECT_NEAR(searcher.GetMin(), 0, kMinTolerance);
  EXPECT_EQ(searcher.num_evaluations(), 112);

  // Set a non-trivial objective goal.
  starting_guess.setZero(kDims);
  searcher.SetSimplexFromStartingGuess(starting_guess, 1.0);
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, 0.1);
  EXPECT_LE(searcher.GetMin(), 0.1);
  EXPECT_EQ(searcher.num_evaluations(), 50);
}

TEST(NelderMeadSearcherTest, Rosenbrock_10D) {
  const int kDims = 10;
  const int kMaxEvaluations = 5000;
  const double kStdDevTolerance = 0.0;
  const double kArgMinTolerance = 0.03;
  const double kMinTolerance = 0.001;
  const double kObjectiveGoal = kMinTolerance;
  NelderMeadSearcher<double> searcher(kDims);
  searcher.SetObjectiveFunction(Rosenbrock<double>);
  Vector<double> starting_guess;
  const Vector<double> minimum = Vector<double>::Ones(10);
  starting_guess.setZero(kDims);
  searcher.SetSimplexFromStartingGuess(starting_guess, 0.1);
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  EXPECT_THAT(searcher.GetArgMin(),
              EigenArrayNear<double>(minimum, kArgMinTolerance));
  EXPECT_NEAR(searcher.GetMin(), 0, kMinTolerance);
  EXPECT_EQ(searcher.num_evaluations(), 2570);

  // Try a different starting guess.
  searcher.SetSimplexFromStartingGuessSpan(
      {-1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0}, 0.1);
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  EXPECT_THAT(searcher.GetArgMin(),
              EigenArrayNear<double>(minimum, kArgMinTolerance));
  EXPECT_NEAR(searcher.GetMin(), 0, kMinTolerance);
  EXPECT_EQ(searcher.num_evaluations(), 4415);

  // Set a non-trivial objective goal.
  starting_guess.setZero(kDims);
  searcher.SetSimplexFromStartingGuess(starting_guess, 0.1);
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, 0.1);
  EXPECT_LE(searcher.GetMin(), 0.1);
  EXPECT_EQ(searcher.num_evaluations(), 2266);
}

TEST(NelderMeadSearcherTest, Rosenbrock_2D_Constrained) {
  const int kDims = 2;
  const int kMaxEvaluations = 200;
  const double kStdDevTolerance = 0.0001;
  const double kArgMinTolerance = 0.01;
  const double kMinTolerance = 0.002;
  const double kObjectiveGoal = -std::numeric_limits<double>::infinity();
  const double kInfinity = std::numeric_limits<double>::infinity();
  NelderMeadSearcher<double> searcher(kDims);
  searcher.SetObjectiveFunction(Rosenbrock<double>);
  const Vector<double> starting_guess = Vector<double>::Zero(kDims);
  searcher.SetSimplexFromStartingGuess(starting_guess, 1.0);
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  const double unconstrained_min = searcher.GetMin();
  const Vector<double> unconstrained_argmin = searcher.GetArgMin();

  // Test that lower (upper) bounds of -infinite (+infinite) give the
  // unconstrained solution.
  searcher.SetSimplexFromStartingGuess(starting_guess, 1.0);
  searcher.SetLowerBounds({-kInfinity, -kInfinity});
  searcher.SetUpperBounds({+kInfinity, +kInfinity});
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  EXPECT_EQ(searcher.GetMin(), unconstrained_min);
  EXPECT_THAT(searcher.GetArgMin(), EigenArrayNear(unconstrained_argmin, 0));

  // Constrain the first coordinate to be with [-1, 0.5].
  searcher.SetSimplexFromStartingGuess(starting_guess, 1.0);
  searcher.SetLowerBounds({-1, -kInfinity});
  searcher.SetUpperBounds({0.5, +kInfinity});
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  EXPECT_GT(searcher.GetMin(), unconstrained_min);
  // The constrained solution is (x0, x1) = (0.5, 0.25), f(x0, x1) = 0.25
  EXPECT_NEAR(searcher.GetMin(), 0.25, kMinTolerance);
  EXPECT_THAT(searcher.GetArgMin(),
              EigenArrayNear<double>({0.5, 0.25}, kArgMinTolerance));

  // Constrain the solution to be within the rectangle [-0.5:0.5] x [0.5:1.5].
  searcher.SetSimplexFromStartingGuessSpan({-0.25, 0.75}, 1.0);
  searcher.SetLowerBounds({-0.5, 0.5});
  searcher.SetUpperBounds({0.5, 1.5});
  searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance, kObjectiveGoal);
  EXPECT_GT(searcher.GetMin(), unconstrained_min);
  // The constrained solution is (x0, x1) = (0.5, 0.5), f(x0, x1) = 6.5
  EXPECT_NEAR(searcher.GetMin(), 6.5, kMinTolerance);
  EXPECT_THAT(searcher.GetArgMin(),
              EigenArrayNear<double>({0.5, 0.5}, kArgMinTolerance));
}

template <int Dims>
static void BM_NelderMeadSearcher_Rosenbrock(benchmark::State& state) {
  const int kMaxEvaluations = 200 * Dims;
  const double kStdDevTolerance = 1e-7;
  const double kObjectiveGoal = -std::numeric_limits<double>::infinity();
  NelderMeadSearcher<double> searcher(Dims);
  searcher.SetObjectiveFunction(Rosenbrock<double>);
  Vector<double> starting_guess;
  starting_guess.setZero(Dims);
  while (state.KeepRunning()) {
    searcher.SetSimplexFromStartingGuess(starting_guess, 1.0);
    searcher.MinimizeObjective(kMaxEvaluations, kStdDevTolerance,
                               kObjectiveGoal);
    double min = searcher.GetMin();
    Vector<double> argmin = searcher.GetArgMin();
    benchmark::DoNotOptimize(min);
    benchmark::DoNotOptimize(argmin);
  }
}

// Old-style template benchmarking needed for open sourcing. External
// google/benchmark repo doesn't have functionality from cl/118676616 enabling
// BENCHMARK(TemplatedFunction<2>) syntax.
BENCHMARK_TEMPLATE(BM_NelderMeadSearcher_Rosenbrock, 2);
BENCHMARK_TEMPLATE(BM_NelderMeadSearcher_Rosenbrock, 4);
BENCHMARK_TEMPLATE(BM_NelderMeadSearcher_Rosenbrock, 8);
BENCHMARK_TEMPLATE(BM_NelderMeadSearcher_Rosenbrock, 128);

}  // namespace audio_dsp

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_1EURO_ONE_EURO_FILTER_UNITTEST_H_
#define THIRD_PARTY_1EURO_ONE_EURO_FILTER_UNITTEST_H_

#include "third_party/one_euro_filter/src/one_euro_filter.h"

#include <stdlib.h>
#include <time.h>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace one_euro_filter {
namespace test {

constexpr double kEpsilon = 0.0001;

class OneEuroFilterTest : public testing::Test {
 public:
  OneEuroFilterTest() {}

  void SetUp() { filter_ = std::make_unique<OneEuroFilter>(120); }

  void SetUpFilter(double freq,
                   double mincutoff = 1.0,
                   double beta = 0.0,
                   double dcutoff = 1.0) {
    filter_ = std::make_unique<OneEuroFilter>(freq, mincutoff, beta, dcutoff);
  }

  bool CheckBeta(double beta) { return filter_->beta_ == beta; }

  bool CheckFrequency(double frequency) { return filter_->freq_ == frequency; }
  bool CheckMinCutoff(double mincutoff) {
    return filter_->mincutoff_ == mincutoff;
  }
  bool CheckDerivateCutoff(double dcutoff) {
    return filter_->dcutoff_ == dcutoff;
  }

 protected:
  std::unique_ptr<OneEuroFilter> filter_;
};

// Check default values when wrong parameters are sent to the filter
TEST_F(OneEuroFilterTest, ParametersTest) {
  SetUpFilter(100, 100, 100, 100);
  EXPECT_TRUE(CheckBeta(100));
  EXPECT_TRUE(CheckFrequency(100));
  EXPECT_TRUE(CheckMinCutoff(100));
  EXPECT_TRUE(CheckDerivateCutoff(100));

  SetUpFilter(-100, -100, -100, -100);
  EXPECT_TRUE(CheckBeta(-100));
  EXPECT_TRUE(CheckFrequency(120));
  EXPECT_TRUE(CheckMinCutoff(1.0));
  EXPECT_TRUE(CheckDerivateCutoff(1.0));
}

// Check the filter is working when sending random numbers inside the
// interval [0,1] at random interval. Each filtered number should be
// inside this interval
TEST_F(OneEuroFilterTest, RandomValuesRandomTimestampTest) {
  srand(time(0));
  double x, xf;
  double sum_x = 0, sum_xf = 0;
  TimeStamp ts = 1;
  double delta = 0.008;  // Every 8 ms
  for (int i = 0; i < 100; i++) {
    x = ((double)rand()) / RAND_MAX;
    xf = filter_->Filter(x, ts);
    ts += delta * x;  // Randon delta time between events
    EXPECT_GT(x, 0);
    EXPECT_LT(x, 1);
    EXPECT_GT(xf, 0);
    EXPECT_LT(xf, 1);
    sum_x += x;
    sum_xf += xf;
  }
  EXPECT_NE(sum_x, sum_xf);
}

// Check the filter is working when sending random numbers inside the
// interval [0,1] at constant interval. Each filtered number should be
// inside this interval
TEST_F(OneEuroFilterTest, RandomValuesConstantTimestampTest) {
  srand(time(0));
  double x, xf;
  double sum_x = 0, sum_xf = 0;
  TimeStamp ts = 1;
  double delta = 0.008;  // Every 8 ms
  for (int i = 0; i < 100; i++) {
    x = ((double)rand()) / RAND_MAX;
    xf = filter_->Filter(x, ts);
    ts += delta;
    EXPECT_GT(x, 0);
    EXPECT_LT(x, 1);
    EXPECT_GT(xf, 0);
    EXPECT_LT(xf, 1);
    sum_x += x;
    sum_xf += xf;
  }
  EXPECT_NE(sum_x, sum_xf);
}

// Check the filter is working when sending the same number at random interval.
// Each filtered value should be the same
TEST_F(OneEuroFilterTest, SameValuesRandomTimestampTest) {
  srand(time(0));
  double x = 0.5, xf;
  TimeStamp ts = 1;
  double delta = 0.008;  // Every 8 ms
  for (int i = 0; i < 100; i++) {
    xf = filter_->Filter(x, ts);
    ts += delta * ((double)rand()) /
          RAND_MAX;  // Randon delta time between events
    EXPECT_EQ(x, xf);
  }
}

// Check the filter is working when sending the same number at constant
// interval. Each filtered value should be the same
TEST_F(OneEuroFilterTest, SameValuesConstantTimestampTest) {
  double x = 0.5, xf;
  TimeStamp ts = 0;
  double delta = 0.008;  // Every 8 ms
  for (int i = 0; i < 100; i++) {
    xf = filter_->Filter(x, ts);
    ts += delta;
    EXPECT_EQ(x, xf);
  }
}

// Check if the filter is well cloned. We create a first filter, send random
// values, and then we clone it. If we send the same new random values to both
// filters, we should have the same filtered results
TEST_F(OneEuroFilterTest, CloneTest) {
  srand(time(0));
  double x;
  TimeStamp ts = 1;
  double delta = 0.008;  // Every 8 ms
  for (int i = 0; i < 100; i++) {
    x = rand();
    filter_->Filter(x, ts);
    ts += delta;
  }

  std::unique_ptr<OneEuroFilter> fork_filter;
  fork_filter.reset(filter_->Clone());

  double xf1, xf2;
  for (int i = 0; i < 100; i++) {
    x = rand();
    xf1 = filter_->Filter(x, ts);
    xf2 = fork_filter->Filter(x, ts);
    EXPECT_NEAR(xf1, xf2, kEpsilon);
    ts += delta;
  }
}

// Check if the filter is well reset. We send random values, save the values and
// results, then we reset the filter. We send again the same values and see if
// we have the same results, which would be statistically impossible with 100
// random wihtout a proper resetting.
TEST_F(OneEuroFilterTest, TestResetting) {
  std::vector<double> random_values;
  std::vector<double> timestamps;
  std::vector<double> results;
  srand(time(0));
  double x, r;
  TimeStamp ts = 1;
  double delta = 0.008;  // Every 8 ms
  for (int i = 0; i < 100; i++) {
    x = ((double)rand()) / RAND_MAX;  // betwwen 0 and 1
    random_values.push_back(x);
    timestamps.push_back(ts);
    r = filter_->Filter(x, ts);
    results.push_back(r);
    ts += delta * x;  // Randon delta time between events
  }

  filter_->Reset();

  EXPECT_EQ((int)random_values.size(), 100);
  EXPECT_EQ((int)timestamps.size(), 100);
  EXPECT_EQ((int)results.size(), 100);

  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(results[i], filter_->Filter(random_values[i], timestamps[i]));
  }
}

}  // namespace test
}  // namespace one_euro_filter

#endif  // THIRD_PARTY_1EURO_ONE_EURO_FILTER_UNITTEST_H_
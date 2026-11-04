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

#include "audio/dsp/batch_top_n.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {
namespace {

using ::Eigen::ArrayXd;
using ::Eigen::ArrayXf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pointee;

TEST(BatchTopNTest, Basic) {
  BatchTopN<int> topn(3);
  EXPECT_EQ(topn.limit(), 3);
  EXPECT_THAT(topn.GetTopElements(), IsEmpty());

  topn.Push(0);
  topn.Push(99);
  topn.Push(1);
  topn.Push(2);
  topn.Push(3);
  topn.Push(4);
  topn.Push(5);
  topn.Push(6);
  BatchTopN<int>::Result result = topn.GetTopElements();
  EXPECT_THAT(result, ElementsAre(99, 6, 5));

  // Check BatchTopN::Result methods.
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], 99);
  EXPECT_EQ(result[1], 6);
  EXPECT_EQ(result[2], 5);
  std::vector<int> copied_to_vector;
  result.CopyTo(&copied_to_vector);
  EXPECT_THAT(copied_to_vector, ElementsAre(99, 6, 5));
  std::vector<int> copied_by_iteration(result.begin(), result.end());
  EXPECT_THAT(copied_to_vector, ElementsAre(99, 6, 5));

  topn.Push(-99);
  topn.Push(6);
  EXPECT_THAT(topn.GetTopElements(), ElementsAre(99, 6, 6));

  topn.Reset();
  EXPECT_THAT(topn.GetTopElements(), IsEmpty());
}

TEST(BatchTopNTest, RandomInts) {
  std::mt19937 rng(0 /* seed */);
  std::vector<int> input(250);
  std::uniform_int_distribution<int> dist(-99, 99);

  for (int limit : {1, 2, 5, 10}) {
    SCOPED_TRACE("limit: " + testing::PrintToString(limit));
    for (int& value : input) {
      value = dist(rng);
    }

    BatchTopN<int> topn(limit);
    EXPECT_EQ(topn.limit(), limit);
    for (const int& value : input) {
      topn.Push(value);
    }

    std::sort(input.begin(), input.end(), std::greater<int>());
    EXPECT_THAT(topn.GetTopElements(), ElementsAreArray(
        std::vector<int>(input.cbegin(), input.cbegin() + limit)));
  }
}

TEST(BatchTopNTest, Strings) {
  BatchTopN<std::string> topn(3);
  topn.Push("apple");
  topn.Push("grape");
  topn.Push("banana");
  topn.Push("mango");
  topn.Push("kiwi");
  EXPECT_THAT(topn.GetTopElements(), ElementsAre("mango", "kiwi", "grape"));
}

TEST(BatchTopNTest, MoveOnly) {
  using StringPtr = std::unique_ptr<std::string>;
  struct PointeeGreater {
    bool operator()(const StringPtr& a, const StringPtr& b) const {
      return *a > *b;
    }
  };
  BatchTopN<StringPtr, PointeeGreater> topn(3);
  topn.Push(StringPtr(new std::string("apple")));
  topn.Push(StringPtr(new std::string("grape")));
  topn.Push(StringPtr(new std::string("banana")));
  topn.Push(StringPtr(new std::string("mango")));
  topn.Push(StringPtr(new std::string("kiwi")));
  EXPECT_THAT(topn.GetTopElements(), ElementsAre(
      Pointee(Eq("mango")), Pointee(Eq("kiwi")), Pointee(Eq("grape"))));
}

TEST(BatchTopNTest, ComparatorInstance) {
  std::map<std::string, double> mass_kg({{"Sun", 1.989e30},
                                         {"Mercury", 3.285e23},
                                         {"Earth", 5.972e24},
                                         {"Mars", 6.39e23},
                                         {"Jupiter", 1.898e27},
                                         {"Saturn", 5.683e26}});
  struct OrderByMass {
    explicit OrderByMass(const std::map<std::string, double>& table_in)
        : table(table_in) {}
    bool operator()(const std::string& a, const std::string& b) const {
      return table.at(a) > table.at(b);
    }
    const std::map<std::string, double>& table;
  };
  BatchTopN<std::string, OrderByMass> topn(3, OrderByMass(mass_kg));
  topn.Push("Mars");
  topn.Push("Earth");
  topn.Push("Jupiter");
  topn.Push("Saturn");
  topn.Push("Mercury");
  topn.Push("Sun");
  EXPECT_THAT(topn.GetTopElements(), ElementsAre("Sun", "Jupiter", "Saturn"));
}

TEST(BatchTopNTest, FindNPeaksTest) {
  ArrayXd arr = ArrayXd::Random(180);
  std::vector<int> peak_locations = {1, 4, 56, 129, 8, 42, 93};
  for (int i = 0; i < peak_locations.size(); ++i) {
    // Peaks get smaller as i increases, all are bigger than elements generated
    // from Random().
    arr[peak_locations[i]] = 3 + peak_locations.size() - i;
  }
  for (int N = 1; N < peak_locations.size(); ++N) {
    { // Test for aperiodic conditions.
      std::vector<int> indices = TopNPeaksNonperiodic(N, arr);
      ASSERT_THAT(indices, testing::ElementsAreArray(std::vector<int>(
          peak_locations.begin(), peak_locations.begin() + N)));
    }
    { // Test for periodic conditions. Should be the same.
      std::vector<int> indices = TopNPeaksPeriodic(N, arr);
      ASSERT_THAT(indices, testing::ElementsAreArray(std::vector<int>(
          peak_locations.begin(), peak_locations.begin() + N)));
    }
  }
}

TEST(BatchTopNTest, FindNPeaksEndConditionsTest) {
  ArrayXf arr(6);
  arr << 2.0f, 1.0f, 1.0f, 4.0f, 25.0f, 12.0f;
  std::vector<int> aperiodic_peak_locations = {4, 0};
  std::vector<int> periodic_peak_locations = {4};

  // Neither of these will find the requested number of peaks (3). This is fine.
  { // Test for aperiodic conditions.
    std::vector<int> indices = TopNPeaksNonperiodic(3, arr);
    EXPECT_EQ(indices, aperiodic_peak_locations);
  }
  { // Test for periodic conditions. Left side is no longer a peak.
    std::vector<int> indices = TopNPeaksPeriodic(3, arr);
    EXPECT_EQ(indices, periodic_peak_locations);
  }
}

TEST(BatchTopNTest, FindNPeaksEndConditionsVectorTest) {
  // Same as above, but for vectors.
  std::vector<float> arr = {2.0f, 1.0f, 1.0f, 4.0f, 25.0f, 12.0f};
  std::vector<int> aperiodic_peak_locations = {4, 0};
  std::vector<int> periodic_peak_locations = {4};
  { // Test for aperiodic conditions.
    std::vector<int> indices = TopNPeaksNonperiodic(3, arr);
    EXPECT_EQ(indices, aperiodic_peak_locations);
  }
  { // Test for periodic conditions. Left side is no longer a peak.
    std::vector<int> indices = TopNPeaksPeriodic(3, arr);
    EXPECT_EQ(indices, periodic_peak_locations);
  }
}

TEST(BatchTopNTest, FlatSignalHasNoPeaksTest) {
  ArrayXf arr = ArrayXf::Zero(6);

  // Neither of these will find the requested number of peaks (3). This is fine.
  { // Test for aperiodic conditions.
    std::vector<int> indices = TopNPeaksNonperiodic(3, arr);
    EXPECT_THAT(indices, testing::IsEmpty());
  }
  { // Test for periodic conditions. Left side is no longer a peak.
    std::vector<int> indices = TopNPeaksPeriodic(3, arr);
    EXPECT_THAT(indices, testing::IsEmpty());
  }
}

}  // namespace
}  // namespace audio_dsp

/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow_lite_support/scann_ondevice/cc/core/searcher.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include <glog/logging.h>
#include "absl/synchronization/mutex.h"  // from @com_google_absl
#include "Eigen/Core"  // from @eigen
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/port/proto2.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/partitioner.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/processor.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"
using TextFormat = ::tflite::support::proto::TextFormat;

using Eigen::MatrixXf;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::TestWithParam;
using ::testing::Values;
using Matrix8u =
    Eigen::Matrix<uint8_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;
using tflite::scann_ondevice::core::TopN;

const char kExampleAsymmetricHashingProtoString[] =
    R"(
      subspace: {
        entry {
          dimension: 0.1;
          dimension: 0.2;
        }
        entry: {
          dimension: 0.2;
          dimension: 0.1;
        }
        entry: {
          dimension: 0.9;
          dimension: 0.8;
        }
      }
      subspace: {
        entry {
          dimension: -0.1;
          dimension: -0.2;
          dimension: -0.3;
        }
        entry: {
          dimension: -0.3;
          dimension: -0.2;
          dimension: -0.1;
        }
        entry: {
          dimension: -0.9;
          dimension: -0.8;
          dimension: -0.7;
        }
      })";

const char kExamplePartitionerProtoString[] =
    R"(
      leaf: {
        dimension: 0.1;
        dimension: 0.2;
      }
      leaf: {
        dimension: 0.2;
        dimension: 0.1;
      }
      leaf: {
        dimension: 0.9;
        dimension: 0.7;
      }
      leaf: {
        dimension: 0.3;
        dimension: 0.3;
      })";
namespace tflite {
namespace scann_ondevice {
namespace core {
namespace {
TEST(PartitionerTest, Partition) {
  PartitionerProto proto;
  TextFormat::ParseFromString(kExamplePartitionerProtoString, &proto);
  proto.set_query_distance(SQUARED_L2_DISTANCE);
  auto partitioner = Partitioner::Create(proto);
  MatrixXf query(2, 3);
  query << 0.3, 0.9, -1, 0.2, 0.9, -1;

  std::vector<std::vector<int>> tokens(3, std::vector<int>(2, -1));
  ASSERT_TRUE(partitioner->Partition(query, &tokens));
  for (int i = 0; i < 3; ++i) {
    std::sort(tokens[i].begin(), tokens[i].end());
  }
  EXPECT_EQ((std::vector<int>{1, 3}), tokens[0]);
  EXPECT_EQ((std::vector<int>{2, 3}), tokens[1]);
  EXPECT_EQ((std::vector<int>{0, 1}), tokens[2]);
}

TEST(PartitionerTest, PartitionDotProductDistance) {
  PartitionerProto proto;
  TextFormat::ParseFromString(kExamplePartitionerProtoString, &proto);
  proto.set_query_distance(DOT_PRODUCT);
  auto partitioner = Partitioner::Create(proto);
  MatrixXf query(2, 3);
  query << 0.3, 0.9, -1, 0.2, 0.9, -1;

  std::vector<std::vector<int>> tokens(3, std::vector<int>(2, -1));
  ASSERT_TRUE(partitioner->Partition(query, &tokens));
  for (int i = 0; i < 3; ++i) {
    std::sort(tokens[i].begin(), tokens[i].end());
  }
  EXPECT_EQ((std::vector<int>{2, 3}), tokens[0]);
  EXPECT_EQ((std::vector<int>{2, 3}), tokens[1]);
  EXPECT_EQ((std::vector<int>{0, 1}), tokens[2]);
}

TEST(ProcessorTest, AsymmetricHashQuerierNonSimd) {
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  auto querier = AsymmetricHashQuerier::Create(proto);
  CHECK(querier);

  MatrixXf query(5, 2);
  query << 0, 1, 0, 1, 0, 1, 0, 1, 0, 1;
  QueryInfo query_info;
  ASSERT_TRUE(querier->Process(query, &query_info));
  MatrixXf expected_lut(6, 2);
  expected_lut << 0.05, 1.45, 0.05, 1.45, 1.45, 0.05, 0.14, 4.34, 0.14, 4.34,
      1.94, 9.74;
  ASSERT_TRUE(query_info.query_lut->isApprox(expected_lut, 1e-5));
}

TEST(ProcessorTest, AsymmetricHashQuerierNonSimdDotProduct) {
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  proto.set_query_distance(DOT_PRODUCT);
  auto querier = AsymmetricHashQuerier::Create(proto);
  ASSERT_NE(querier, nullptr);

  MatrixXf query(5, 2);
  query << 0, 1, 0, 1, 0, 1, 0, 1, 0, 1;
  QueryInfo query_info;
  ASSERT_TRUE(querier->Process(query, &query_info));

  const auto& query_lut = query_info.query_lut;
  const float* lut_raw = query_lut->data();
  EXPECT_THAT(std::vector<float>(lut_raw, lut_raw + query_lut->rows()),
              ElementsAre(0, 0, 0, 0, 0, 0));
  EXPECT_THAT(std::vector<float>(lut_raw + query_lut->rows(),
                                 lut_raw + query_lut->rows() * 2),
              ElementsAre(-0.3, -0.3, -1.7, 0.6, 0.6, 2.4));
}

TEST(ProcessorTest, AsymmetricHashQuerierSimd) {
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  auto querier = AsymmetricHashQuerier::Create(proto);
  MatrixXf query(5, 6);
  query << 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0,
      1, 0, 1, 0, 0, 1, 1;
  QueryInfo query_info;
  ASSERT_TRUE(querier->Process(query, &query_info));
  MatrixXf expected_lut(6, 6);
  expected_lut << 0.05, 0.05, 1.45, 0.65, 0.85, 1.45, 0.05, 0.05, 1.45, 0.85,
      0.65, 1.45, 1.45, 1.45, 0.05, 0.85, 0.65, 0.05, 0.14, 4.34, 0.14, 1.54,
      2.94, 4.34, 0.14, 4.34, 0.14, 1.54, 2.94, 4.34, 1.94, 9.74, 1.94, 4.54,
      7.14, 9.74;
  ASSERT_TRUE(query_info.query_lut->isApprox(expected_lut, 1e-5));
  expected_lut << 0.05, 1.45, 0.14, 0.14, 0.85, 1.45, 0.05, 0.85, 4.34, 1.54,
      0.65, 1.45, 1.45, 1.45, 0.14, 1.94, 0.65, 0.05, 0.65, 1.45, 1.54, 9.74,
      2.94, 4.34, 0.05, 0.05, 0.14, 1.94, 2.94, 4.34, 0.05, 0.85, 4.34, 4.54,
      7.14, 9.74;
  ASSERT_TRUE(query_info.transposed_query_lut->isApprox(expected_lut, 1e-5));
}

TEST(ProcessorTest, AsymmetricHashPreprocessingLazyMemoryAllocation) {
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  auto querier = AsymmetricHashQuerier::Create(proto);
  QueryInfo query_info;
  {
    MatrixXf query(5, 2);
    query << 0, 0, 0, 0, 0, 1, 0, 1, 0, 1;
    ASSERT_TRUE(querier->Process(query, &query_info));
    MatrixXf expected_lut(6, 2);
    expected_lut << 0.05, 0.05, 0.05, 0.05, 1.45, 1.45, 0.14, 4.34, 0.14, 4.34,
        1.94, 9.74;
    EXPECT_TRUE(query_info.query_lut->isApprox(expected_lut, 1e-5));
    EXPECT_TRUE(query_info.transposed_query_lut->isApprox(expected_lut, 1e-5));
  }
  {
    MatrixXf query(5, 6);
    query << 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1,
        0, 1, 0, 1, 0, 0, 1, 1;
    ASSERT_TRUE(querier->Process(query, &query_info));
    MatrixXf expected_lut(6, 6);
    expected_lut << 0.05, 0.05, 1.45, 0.65, 0.85, 1.45, 0.05, 0.05, 1.45, 0.85,
        0.65, 1.45, 1.45, 1.45, 0.05, 0.85, 0.65, 0.05, 0.14, 4.34, 0.14, 1.54,
        2.94, 4.34, 0.14, 4.34, 0.14, 1.54, 2.94, 4.34, 1.94, 9.74, 1.94, 4.54,
        7.14, 9.74;
    EXPECT_TRUE(query_info.query_lut->isApprox(expected_lut, 1e-5));
    expected_lut << 0.05, 1.45, 0.14, 0.14, 0.85, 1.45, 0.05, 0.85, 4.34, 1.54,
        0.65, 1.45, 1.45, 1.45, 0.14, 1.94, 0.65, 0.05, 0.65, 1.45, 1.54, 9.74,
        2.94, 4.34, 0.05, 0.05, 0.14, 1.94, 2.94, 4.34, 0.05, 0.85, 4.34, 4.54,
        7.14, 9.74;
    EXPECT_TRUE(query_info.transposed_query_lut->isApprox(expected_lut, 1e-5));
  }
  {
    MatrixXf query(5, 4);
    query << 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1;
    ASSERT_TRUE(querier->Process(query, &query_info));
    MatrixXf expected_lut(6, 6);
    expected_lut << 1.45, 0.65, 0.85, 1.45, 0.85, 1.45, 1.45, 0.85, 0.65, 1.45,
        0.65, 1.45, 0.05, 0.85, 0.65, 0.05, 0.65, 0.05, 0.14, 1.54, 2.94, 4.34,
        2.94, 4.34, 0.14, 1.54, 2.94, 4.34, 2.94, 4.34, 1.94, 4.54, 7.14, 9.74,
        7.14, 9.74;
    EXPECT_TRUE(query_info.query_lut->isApprox(expected_lut, 1e-5));
    expected_lut << 1.45, 0.65, 0.14, 2.94, 0.85, 1.45, 0.65, 1.45, 1.54, 4.34,
        0.65, 1.45, 0.85, 0.05, 2.94, 1.94, 0.65, 0.05, 1.45, 0.85, 4.34, 4.54,
        2.94, 4.34, 1.45, 0.65, 0.14, 7.14, 2.94, 4.34, 0.85, 0.05, 1.54, 9.74,
        7.14, 9.74;
    EXPECT_TRUE(query_info.transposed_query_lut->isApprox(expected_lut, 1e-5));
  }
}

TEST(ProcessorTest, AsymmetricHashQuerierUint16) {
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  proto.set_lookup_type(AsymmetricHashingProto::INT16);
  auto querier = AsymmetricHashQuerier::Create(proto);
  CHECK(querier);

  MatrixXf query(5, 2);
  query << 0, 1, 0, 1, 0, 1, 0, 1, 0, 1;
  QueryInfo query_info;
  ASSERT_TRUE(querier->Process(query, &query_info));
  QueryInfo::Matrix<uint16_t> expected_lut(6, 2);
  expected_lut << 0, 295, 0, 295, 295, 0, 19, 906, 19, 906, 399, 2047;

  LOG(INFO) << *(query_info.query_lut_uint16);

  ASSERT_EQ(*(query_info.query_lut_uint16), expected_lut);
  EXPECT_NEAR(query_info.fixed_point_min, 0.05, 1e-4);
  EXPECT_NEAR(query_info.fixed_point_max, 9.74, 1e-4);
}

TEST(ProcessorTest, AsymmetricHashQuerierUint8) {
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  proto.set_lookup_type(AsymmetricHashingProto::INT8);
  auto querier = AsymmetricHashQuerier::Create(proto);
  CHECK(querier);

  MatrixXf query(5, 2);
  query << 0, 1, 0, 1, 0, 1, 0, 1, 0, 1;
  QueryInfo query_info;
  ASSERT_TRUE(querier->Process(query, &query_info));
  QueryInfo::Matrix<uint8_t> expected_lut(6, 2);
  expected_lut << 0, 36, 0, 36, 36, 0, 2, 112, 2, 112, 49, 255;
  ASSERT_EQ(*(query_info.query_lut_uint8), expected_lut);
  EXPECT_NEAR(query_info.fixed_point_min, 0.05, 1e-4);
  EXPECT_NEAR(query_info.fixed_point_max, 9.74, 1e-4);
}

class SearcherTest : public TestWithParam<size_t> {};

TEST_P(SearcherTest, LinearLeafSearcherNonSimd) {
  MatrixXf query(3, 2);
  query << 0, 1, 2, 3, 3, 1;
  std::shared_ptr<MatrixXf> database(new MatrixXf(3, 5));
  *database << 0, 1, 2, 2, 1, 1, 0, 1, 2, 2, 2, 2, 5, 6, 1;
  std::vector<TopN> top_n;
  for (int i = 0; i < 2; ++i) {
    top_n.emplace_back(
        TopN(3, std::make_pair(std::numeric_limits<float>::max(), -1)));
  }
  auto leaf_searcher = LinearLeafSearcher::Create(database);
  ASSERT_TRUE(leaf_searcher->FindNeighbors(query, &top_n));

  constexpr float kEps = 1e-5;
  auto extracted = top_n[0].Take();
  EXPECT_NEAR(2.0, extracted[0].first, kEps);
  EXPECT_NEAR(5.0, extracted[1].first, kEps);
  EXPECT_NEAR(6.0, extracted[2].first, kEps);
  EXPECT_EQ(0, extracted[0].second);
  EXPECT_EQ(4, extracted[1].second);
  EXPECT_EQ(1, extracted[2].second);

  extracted = top_n[1].Take();
  EXPECT_NEAR(1.0, extracted[0].first, kEps);
  EXPECT_NEAR(6.0, extracted[1].first, kEps);
  EXPECT_NEAR(10.0, extracted[2].first, kEps);
  EXPECT_EQ(4, extracted[0].second);
  EXPECT_EQ(0, extracted[1].second);
  EXPECT_EQ(1, extracted[2].second);
}

TEST_P(SearcherTest, LinearLeafSearcherNonSimdDotProduct) {
  MatrixXf query(3, 2);
  query << 0, 1, 2, 3, 3, 1;
  auto database = std::make_shared<MatrixXf>(3, 5);
  *database << 0, 1, 2, 2, 1, 1, 0, 1, 2, 2, 2, 2, 5, 6, 1;

  std::vector<TopN> top_n(
      2, TopN(3, std::make_pair(std::numeric_limits<float>::max(), -1)));

  auto leaf_searcher = LinearLeafSearcher::Create(database, DOT_PRODUCT);
  ASSERT_TRUE(leaf_searcher->FindNeighbors(query, &top_n));

  auto extracted = top_n[0].Take();
  EXPECT_THAT(extracted, ElementsAre(Pair(-22, 3), Pair(-17, 2), Pair(-8, 0)));

  extracted = top_n[1].Take();
  EXPECT_THAT(extracted, ElementsAre(Pair(-14, 3), Pair(-10, 2), Pair(-8, 4)));
}

TEST_P(SearcherTest, AsymmetricHashNonSimd) {
  MatrixXf query(5, 2);
  query << 0, 1, 0, 1, 0, 1, 0, 1, 0, 1;
  std::shared_ptr<Matrix8u> database(new Matrix8u(2, 6));
  *database << 0, 1, 2, 2, 1, 0, 1, 0, 1, 2, 2, 0;
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  auto querier = AsymmetricHashQuerier::Create(proto);
  auto leaf_searcher =
      AsymmetricHashLeafSearcher::Create(database, 0, std::move(querier));
  std::vector<TopN> top_n;
  for (int i = 0; i < 2; ++i) {
    top_n.emplace_back(
        TopN(3, std::make_pair(std::numeric_limits<float>::max(), -1)));
  }
  ASSERT_TRUE(leaf_searcher->FindNeighbors(query, &top_n));

  constexpr float kEps = 1e-5;
  auto extracted = top_n[0].Take();
  EXPECT_NEAR(0.19, extracted[0].first, kEps);
  EXPECT_NEAR(0.19, extracted[1].first, kEps);
  EXPECT_NEAR(0.19, extracted[2].first, kEps);

  extracted = top_n[1].Take();
  EXPECT_NEAR(4.39, extracted[0].first, kEps);
  EXPECT_NEAR(5.79, extracted[1].first, kEps);
  EXPECT_NEAR(5.79, extracted[2].first, kEps);
}

#if defined(__ARM_NEON__) || defined(__SSE__)
TEST_P(SearcherTest, AsymmetricHashSimdFloat32x4) {
  MatrixXf query(5, 6);
  query << 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0,
      1, 0, 1, 0, 0, 1, 1;
  std::shared_ptr<Matrix8u> database(new Matrix8u(2, 9));
  *database << 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2;
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  auto querier = AsymmetricHashQuerier::Create(proto);
  auto leaf_searcher = AsymmetricHashLeafSearcher::Create(
      database, 0, std::move(querier), GetParam());
  std::vector<TopN> top_n;
  for (int i = 0; i < 6; ++i) {
    top_n.emplace_back(
        TopN(3, std::make_pair(std::numeric_limits<float>::max(), -1)));
  }
  ASSERT_TRUE(leaf_searcher->FindNeighbors(query, &top_n));

  constexpr float kEps = 1e-5;
  auto extracted = top_n[0].Take();
  EXPECT_NEAR(0.19, extracted[0].first, kEps);
  EXPECT_NEAR(0.19, extracted[1].first, kEps);
  EXPECT_NEAR(0.19, extracted[2].first, kEps);

  extracted = top_n[1].Take();
  EXPECT_NEAR(4.39, extracted[0].first, kEps);
  EXPECT_NEAR(4.39, extracted[1].first, kEps);
  EXPECT_NEAR(4.39, extracted[2].first, kEps);

  extracted = top_n[2].Take();
  EXPECT_NEAR(0.19, extracted[0].first, kEps);
  EXPECT_NEAR(0.19, extracted[1].first, kEps);
  EXPECT_NEAR(1.59, extracted[2].first, kEps);

  extracted = top_n[3].Take();
  EXPECT_NEAR(2.19, extracted[0].first, kEps);
  EXPECT_NEAR(2.19, extracted[1].first, kEps);
  EXPECT_NEAR(2.39, extracted[2].first, kEps);

  extracted = top_n[4].Take();
  EXPECT_NEAR(3.59, extracted[0].first, kEps);
  EXPECT_NEAR(3.59, extracted[1].first, kEps);
  EXPECT_NEAR(3.59, extracted[2].first, kEps);

  extracted = top_n[5].Take();
  EXPECT_NEAR(4.39, extracted[0].first, kEps);
  EXPECT_NEAR(4.39, extracted[1].first, kEps);
  EXPECT_NEAR(5.79, extracted[2].first, kEps);
}
#endif

#if defined(__ARM_NEON__) || defined(__SSE__)
TEST_P(SearcherTest, AsymmetricHashSimdInt16x8) {
  MatrixXf query(5, 11);
  query << 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0,
      1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0,
      1, 1, 1, 0, 0, 0, 0;
  std::shared_ptr<Matrix8u> database(new Matrix8u(2, 9));
  *database << 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2;
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  proto.set_lookup_type(AsymmetricHashingProto::INT16);
  auto querier = AsymmetricHashQuerier::Create(proto);
  QueryInfo query_info;
  ASSERT_TRUE(querier->Process(query, &query_info));

  auto leaf_searcher = AsymmetricHashLeafSearcher::Create(
      database, 0, std::move(querier), GetParam());
  std::vector<TopN> top_n;
  for (int i = 0; i < 11; ++i) {
    top_n.emplace_back(
        TopN(3, std::make_pair(std::numeric_limits<float>::max(), -1)));
  }
  ASSERT_TRUE(leaf_searcher->FindNeighbors(query, &top_n));

  auto extracted = top_n[0].Take();
  constexpr float kEps = 5e-2;
  EXPECT_NEAR(0.19, extracted[0].first, kEps);
  EXPECT_NEAR(0.19, extracted[1].first, kEps);
  EXPECT_NEAR(0.19, extracted[2].first, kEps);

  extracted = top_n[1].Take();
  EXPECT_NEAR(4.39, extracted[0].first, kEps);
  EXPECT_NEAR(4.39, extracted[1].first, kEps);
  EXPECT_NEAR(4.39, extracted[2].first, kEps);

  extracted = top_n[2].Take();
  EXPECT_NEAR(0.19, extracted[0].first, kEps);
  EXPECT_NEAR(0.19, extracted[1].first, kEps);
  EXPECT_NEAR(1.59, extracted[2].first, kEps);

  extracted = top_n[3].Take();
  EXPECT_NEAR(2.19, extracted[0].first, kEps);
  EXPECT_NEAR(2.19, extracted[1].first, kEps);
  EXPECT_NEAR(2.39, extracted[2].first, kEps);

  extracted = top_n[4].Take();
  EXPECT_NEAR(3.59, extracted[0].first, kEps);
  EXPECT_NEAR(3.59, extracted[1].first, kEps);
  EXPECT_NEAR(3.59, extracted[2].first, kEps);

  extracted = top_n[5].Take();
  EXPECT_NEAR(4.39, extracted[0].first, kEps);
  EXPECT_NEAR(4.39, extracted[1].first, kEps);
  EXPECT_NEAR(5.79, extracted[2].first, kEps);

  extracted = top_n[6].Take();
  EXPECT_NEAR(1.39, extracted[0].first, kEps);
  EXPECT_NEAR(1.39, extracted[1].first, kEps);
  EXPECT_NEAR(1.79, extracted[2].first, kEps);

  extracted = top_n[7].Take();
  EXPECT_NEAR(1.59, extracted[0].first, kEps);
  EXPECT_NEAR(1.59, extracted[1].first, kEps);
  EXPECT_NEAR(1.59, extracted[2].first, kEps);

  extracted = top_n[8].Take();
  EXPECT_NEAR(1.39, extracted[0].first, kEps);
  EXPECT_NEAR(1.39, extracted[1].first, kEps);
  EXPECT_NEAR(1.79, extracted[2].first, kEps);

  extracted = top_n[9].Take();
  EXPECT_NEAR(0.79, extracted[0].first, kEps);
  EXPECT_NEAR(0.79, extracted[1].first, kEps);
  EXPECT_NEAR(0.99, extracted[2].first, kEps);

  extracted = top_n[10].Take();
  EXPECT_NEAR(0.79, extracted[0].first, kEps);
  EXPECT_NEAR(0.79, extracted[1].first, kEps);
  EXPECT_NEAR(0.79, extracted[2].first, kEps);
}
#endif

#if defined(__ARM_NEON__) || defined(__SSE__)
TEST_P(SearcherTest, AsymmetricHashMiniBatchedSimdFail) {
  std::shared_ptr<Matrix8u> database(new Matrix8u(2, 9));
  *database << 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2;
  AsymmetricHashingProto proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &proto);
  proto.set_lookup_type(AsymmetricHashingProto::FLOAT);
  proto.set_query_distance(DistanceMeasure::UNSPECIFIED);
  auto querier = AsymmetricHashQuerier::Create(proto);
  auto leaf_searcher = AsymmetricHashLeafSearcher::Create(
      database, 0, std::move(querier), GetParam());

  MatrixXf queries(6, 6);
  queries << 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0,
      1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0;
  std::vector<TopN> top_n;
  for (int i = 0; i < queries.cols(); ++i) {
    top_n.emplace_back(
        TopN(3, std::make_pair(std::numeric_limits<float>::max(), -1)));
  }
  EXPECT_FALSE(leaf_searcher->FindNeighbors(queries, &top_n));
}
#endif

INSTANTIATE_TEST_SUITE_P(SearcherTest, SearcherTest,
                         Values(std::numeric_limits<size_t>::max(), 1, 2, 3, 7,
                                23));

}  // namespace

}  // namespace core
}  // namespace scann_ondevice
}  // namespace tflite

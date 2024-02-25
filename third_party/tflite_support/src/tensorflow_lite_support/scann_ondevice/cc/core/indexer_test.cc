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
#include "tensorflow_lite_support/scann_ondevice/cc/core/indexer.h"

#include <cstdint>
#include <utility>

#include <glog/logging.h>
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/port/proto2.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"

using std::vector;
using TextFormat = ::tflite::support::proto::TextFormat;

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

namespace tflite {
namespace scann_ondevice {
namespace core {
namespace {

TEST(IndexerTest, SquaredL2AsymmetricHash1) {
  AsymmetricHashingProto ah_proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &ah_proto);
  ah_proto.set_query_distance(SQUARED_L2_DISTANCE);

  vector<float> datapoint = {0.1, 0.2, -0.1, -0.2, -0.3};
  vector<uint8_t> result(2, 0);
  AsymmetricHashingIndexer indexer(ah_proto);
  indexer.EncodeDatapoint(datapoint, absl::MakeSpan(result));

  EXPECT_EQ(0, result[0]);
  EXPECT_EQ(0, result[1]);
}

TEST(IndexerTest, SquaredL2AsymmetricReconstruct1) {
  AsymmetricHashingProto ah_proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &ah_proto);
  ah_proto.set_query_distance(SQUARED_L2_DISTANCE);

  vector<float> datapoint = {0.1, 0.2, -0.1, -0.2, -0.3};
  vector<uint8_t> result(2, 0);
  AsymmetricHashingIndexer indexer(ah_proto);
  indexer.EncodeDatapoint(datapoint, absl::MakeSpan(result));

  vector<float> datapoint_recon(5, 0);
  SUPPORT_EXPECT_OK(indexer.DecodeDatapoint(result, absl::MakeSpan(datapoint_recon)));

  EXPECT_EQ(std::vector<float>({0.1, 0.2, -0.1, -0.2, -0.3}), datapoint_recon);
}

TEST(IndexerTest, SquaredL2AsymmetricHash2) {
  AsymmetricHashingProto ah_proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &ah_proto);
  ah_proto.set_query_distance(SQUARED_L2_DISTANCE);

  vector<float> datapoint = {0.8, 0.7, -0.4, -0.2, -0.1};
  vector<uint8_t> result(2, 0);
  AsymmetricHashingIndexer indexer(ah_proto);
  indexer.EncodeDatapoint(datapoint, absl::MakeSpan(result));

  EXPECT_EQ(2, result[0]);
  EXPECT_EQ(1, result[1]);
}

TEST(IndexerTest, SquaredL2AsymmetricReconstruct2) {
  AsymmetricHashingProto ah_proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &ah_proto);
  ah_proto.set_query_distance(SQUARED_L2_DISTANCE);

  vector<float> datapoint = {0.8, 0.7, -0.4, -0.2, -0.1};
  vector<uint8_t> result(2, 0);
  AsymmetricHashingIndexer indexer(ah_proto);
  indexer.EncodeDatapoint(datapoint, absl::MakeSpan(result));

  vector<float> datapoint_recon = {0.1, 0.2, -0.1, -0.2, -0.3};
  SUPPORT_EXPECT_OK(indexer.DecodeDatapoint(result, absl::MakeSpan(datapoint_recon)));

  EXPECT_EQ(std::vector<float>({0.9, 0.8, -0.3, -0.2, -0.1}), datapoint_recon);
}

TEST(IndexerTest, DotProductAsymmetricHash) {
  AsymmetricHashingProto ah_proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &ah_proto);
  ah_proto.set_query_distance(DOT_PRODUCT);

  vector<float> datapoint = {0.3, -0.1, -0.3, 0.5, 0.2};
  vector<uint8_t> result(2, 2);
  AsymmetricHashingIndexer indexer(ah_proto);
  indexer.EncodeDatapoint(datapoint, absl::MakeSpan(result));

  EXPECT_EQ(2, result[0]);
  EXPECT_EQ(1, result[1]);
}

TEST(IndexerTest, ShouldFailDecodeForMismatchingLength) {
  AsymmetricHashingProto ah_proto;
  TextFormat::ParseFromString(kExampleAsymmetricHashingProtoString, &ah_proto);
  ah_proto.set_query_distance(SQUARED_L2_DISTANCE);

  vector<float> result = {0.1, 0.2, -0.1, -0.2, -0.3};
  AsymmetricHashingIndexer indexer(ah_proto);
  auto status = indexer.DecodeDatapoint({}, absl::MakeSpan(result));
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), testing::HasSubstr("Mismatching dimensions"));
}

}  // namespace
}  // namespace core
}  // namespace scann_ondevice
}  // namespace tflite

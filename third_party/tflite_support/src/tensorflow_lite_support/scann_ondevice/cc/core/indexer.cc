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
#include <limits>

#include <glog/logging.h>
#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"

using absl::Span;
using std::vector;

namespace tflite {
namespace scann_ondevice {
namespace core {

namespace {

float ComputeSquaredL2Distance(Span<const float> a, Span<const float> b) {
  if (a.size() != b.size()) return 0;
  float result = 0;
  for (int i = 0; i < a.size(); ++i) {
    result += (a[i] - b[i]) * (a[i] - b[i]);
  }
  return result;
}

float ComputeDotProductDistance(Span<const float> a, Span<const float> b) {
  if (a.size() != b.size()) return 0;
  float result = 0;
  for (int i = 0; i < a.size(); ++i) {
    result += a[i] * b[i];
  }
  return -1 * result;
}

}  // namespace

AsymmetricHashingIndexer::AsymmetricHashingIndexer(
    const AsymmetricHashingProto& ah_proto)
    : dimensions_(),
      codebooks_(),
      distance_measure_(ah_proto.query_distance()) {
  int num_subspace = ah_proto.subspace_size();
  dimensions_.resize(num_subspace);
  codebooks_.resize(num_subspace);

  int subspace_index = 0;
  for (const AsymmetricHashingProto::SubspaceCodebook& codebook :
       ah_proto.subspace()) {
    if (codebook.entry().empty()) return;

    const int dimension = codebook.entry(0).dimension_size();
    const int num_codes = codebook.entry_size();
    dimensions_[subspace_index] = dimension;
    vector<vector<float>> temp_codebook(num_codes, vector<float>(dimension, 0));

    for (int i = 0; i < num_codes; ++i) {
      for (int j = 0; j < dimension; ++j) {
        temp_codebook[i][j] = codebook.entry(i).dimension(j);
      }
    }

    codebooks_[subspace_index] = temp_codebook;

    ++subspace_index;
  }

  total_dimension_ = 0;
  for (const uint8_t dim : dimensions_) total_dimension_ += dim;
}

void AsymmetricHashingIndexer::EncodeDatapoint(
    absl::Span<const float> original, absl::Span<uint8_t> encoded) const {
  if (original.size() != total_dimension_) return;
  if (encoded.size() != dimensions_.size()) return;

  int start_index = 0;
  for (int i = 0; i < dimensions_.size(); ++i) {
    auto raw_data = original.subspan(start_index, dimensions_[i]);
    int closest_index = 0;
    float smallest_distance = std::numeric_limits<float>::infinity();
    for (int j = 0; j < codebooks_[i].size(); ++j) {
      float new_distance;
      switch (distance_measure_) {
        case SQUARED_L2_DISTANCE:
          new_distance = ComputeSquaredL2Distance(raw_data, codebooks_[i][j]);
          break;
        case DOT_PRODUCT:
          new_distance = ComputeDotProductDistance(raw_data, codebooks_[i][j]);
          break;
        default:
          LOG(FATAL) << "Need to specify a distance measure for indexer";
      }
      if (new_distance < smallest_distance) {
        smallest_distance = new_distance;
        closest_index = j;
      }
    }
    encoded[i] = closest_index;

    start_index += dimensions_[i];
  }
}

absl::Status AsymmetricHashingIndexer::DecodeDatapoint(
    absl::Span<const uint8_t> encoded, absl::Span<float> reconstructed) const {
  if (encoded.size() < dimensions_.size()) {
    return absl::InvalidArgumentError("Mismatching dimensions");
  }
  float* __restrict result_ptr = reconstructed.data();

  for (int i = 0; i < dimensions_.size(); ++i) {
    std::memcpy(result_ptr, codebooks_[i][encoded[i]].data(),
                sizeof(float) * dimensions_[i]);
    result_ptr += dimensions_[i];
  }
  return absl::OkStatus();
}

}  // namespace core
}  // namespace scann_ondevice
}  // namespace tflite

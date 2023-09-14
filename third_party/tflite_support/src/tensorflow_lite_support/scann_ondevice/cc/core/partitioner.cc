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
#include "tensorflow_lite_support/scann_ondevice/cc/core/partitioner.h"

#include <glog/logging.h>
#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"

using Eigen::MatrixXf;
using Eigen::VectorXf;
using std::pair;
using std::vector;

namespace tflite {
namespace scann_ondevice {
namespace core {

std::unique_ptr<Partitioner> Partitioner::Create(
    const PartitionerProto& proto) {
  MatrixXf result(0, 0);

  int dims;
  int leaves = proto.leaf_size();
  if (leaves > 0) {
    dims = proto.leaf(0).dimension_size();
    result = MatrixXf(leaves, dims);
    for (int i = 0; i < leaves; ++i) {
      if (proto.leaf(i).dimension_size() != dims) {
        LOG(ERROR) << "Dimension mismatch at " << i << "-th leaf : expected "
                   << dims << " but was " << proto.leaf(i).dimension_size();
        return nullptr;
      }
      for (int j = 0; j < dims; ++j) {
        result(i, j) = proto.leaf(i).dimension(j);
      }
    }
  }

  VectorXf leaf_norms = result.rowwise().squaredNorm();
  return std::unique_ptr<Partitioner>(new Partitioner(
      std::move(result), std::move(leaf_norms), proto.query_distance()));
}

bool Partitioner::Partition(const Eigen::Ref<const Eigen::MatrixXf>& queries,
                            vector<vector<int>>* tokens) const {
  if (queries.cols() != tokens->size()) {
    LOG(ERROR) << "Number of tokens is " << tokens->size() << ", "
               << queries.cols() << " expected.";
    return false;
  }
  MatrixXf dist = -1 * leaves_ * queries;

  if (distance_ == SQUARED_L2_DISTANCE) {
    if (queries.rows() != leaves_.cols()) {
      LOG(ERROR) << "Query dimensions is " << queries.rows() << ", "
                 << leaves_.cols() << " expected.";
      return false;
    }
    dist = 2 * dist;
    dist.colwise() += leaf_norms_;
  }

  for (int i = 0; i < queries.cols(); ++i) {
    int n = (*tokens)[i].size();
    vector<pair<float, int>> results;
    results.reserve(leaves_.rows());
    for (int j = 0; j < leaves_.rows(); ++j) {
      results.emplace_back(dist(j, i), j);
    }
    std::nth_element(results.begin(), results.begin() + n, results.end(),
                     std::less<std::pair<float, int>>());
    for (int j = 0; j < n; ++j) {
      (*tokens)[i][j] = results[j].second;
    }
  }
  return true;
}

int Partitioner::NumPartitions() const { return leaves_.rows(); }

bool NoOpPartitioner::Partition(
    const Eigen::Ref<const Eigen::MatrixXf>& queries,
    std::vector<std::vector<int>>* tokens) const {
  if (queries.cols() != tokens->size()) {
    LOG(ERROR) << "Number of tokens is " << tokens->size() << ", "
               << queries.cols() << " expected.";
    return false;
  }
  for (int i = 0; i < tokens->size(); ++i) {
    if ((*tokens)[i].size() != 1) {
      LOG(ERROR) << "Query " << i << " expects " << tokens[i].size()
                 << " tokens to search but NoOpPartitioner can provide only 1.";
      return false;
    }
    (*tokens)[i][0] = 0;
  }
  return true;
}

int NoOpPartitioner::NumPartitions() const { return 1; }

}  // namespace core
}  // namespace scann_ondevice
}  // namespace tflite

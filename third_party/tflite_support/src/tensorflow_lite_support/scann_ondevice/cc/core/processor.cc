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
#include "tensorflow_lite_support/scann_ondevice/cc/core/processor.h"

#include <cstdint>

#include <glog/logging.h>
#include "tensorflow_lite_support/scann_ondevice/cc/core/index_table_sum.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"

namespace tflite {
namespace scann_ondevice {
namespace core {
template <>
std::shared_ptr<QueryInfo::Matrix<float>> QueryInfo::QueryLUT<float>() {
  return query_lut;
}
template <>
std::shared_ptr<QueryInfo::Matrix<uint16_t>> QueryInfo::QueryLUT<uint16_t>() {
  return query_lut_uint16;
}
template <>
std::shared_ptr<QueryInfo::Matrix<uint8_t>> QueryInfo::QueryLUT<uint8_t>() {
  return query_lut_uint8;
}

template <>
std::shared_ptr<QueryInfo::Matrix<float>>
QueryInfo::TransposedQueryLUT<float>() {
  return transposed_query_lut;
}
template <>
std::shared_ptr<QueryInfo::Matrix<uint16_t>>
QueryInfo::TransposedQueryLUT<uint16_t>() {
  return transposed_query_lut_uint16;
}
template <>
std::shared_ptr<QueryInfo::Matrix<uint8_t>>
QueryInfo::TransposedQueryLUT<uint8_t>() {
  return transposed_query_lut_uint8;
}

std::unique_ptr<AsymmetricHashQuerier> AsymmetricHashQuerier::Create(
    const AsymmetricHashingProto& proto) {
  std::vector<Eigen::MatrixXf> codebooks;
  std::vector<Eigen::VectorXf> codebook_norms;

  int n_subspaces = proto.subspace_size();
  if (n_subspaces == 0) {
    LOG(ERROR) << "Number of subspaces cannot be 0.";
    return nullptr;
  }
  int k_codes = proto.subspace(0).entry_size();
  if (k_codes == 0) {
    LOG(ERROR) << "Number of codes in the subspace cannot be 0.";
    return nullptr;
  }
  int total_dims = 0;
  for (int i = 0; i < n_subspaces; ++i) {
    if (proto.subspace(i).entry_size() != k_codes) {
      LOG(ERROR) << "Number of codes in the subspace " << i << " is "
                 << proto.subspace(i).entry_size() << " but " << k_codes
                 << " is expected.";
      return nullptr;
    }
    int dims = proto.subspace(i).entry(0).dimension_size();
    if (dims == 0) {
      LOG(ERROR) << "Number of dimensions in subspace cannot be 0.";
    }
    total_dims += dims;
    Eigen::MatrixXf codebook(k_codes, dims);
    for (int k = 0; k < k_codes; ++k) {
      for (int j = 0; j < dims; ++j) {
        codebook(k, j) = proto.subspace(i).entry(k).dimension(j);
      }
    }
    codebook_norms.push_back(codebook.rowwise().squaredNorm());
    codebooks.emplace_back(std::move(codebook));
  }

  return std::unique_ptr<AsymmetricHashQuerier>(new AsymmetricHashQuerier(
      std::move(codebooks), std::move(codebook_norms), proto.query_distance(),
      proto.lookup_type(), total_dims));
}

namespace {
template <typename T>
void ConvertLookupToFixedPoint(QueryInfo* query_info) {
  query_info->fixed_point_min = query_info->query_lut->array().minCoeff();
  query_info->fixed_point_max = query_info->query_lut->array().maxCoeff();
  query_info->fixed_point_scale =
      MaxQuantizationValue<T>::value /
      std::max(query_info->fixed_point_max - query_info->fixed_point_min,
               std::numeric_limits<float>::epsilon());
  Eigen::MatrixXf t =
      query_info->fixed_point_scale *
      (query_info->query_lut->array() - query_info->fixed_point_min);
  *(query_info->QueryLUT<T>()) = t.template cast<T>();
}

template <typename T>
void RearrangeLUTHelper(QueryInfo* query_info) {
  tflite::scann_ondevice::core::RearrangeLUT<T>(
      query_info->QueryLUT<T>()->data(), query_info->QueryLUT<T>()->rows(),
      query_info->QueryLUT<T>()->cols(),
      query_info->TransposedQueryLUT<T>()->data());
}
}  // namespace

bool AsymmetricHashQuerier::Process(
    const Eigen::Ref<const Eigen::MatrixXf>& queries,
    QueryInfo* query_info) const {
  int k_codes = codebooks_[0].rows();
  int n_subspaces = codebooks_.size();
  int n_queries = queries.cols();
  if (dims_ != queries.rows()) {
    LOG(ERROR) << "Query dimensions is " << queries.rows() << ", " << dims_
               << " expected.";
    return false;
  }

  if (query_info->query_lut == nullptr ||
      query_info->query_lut->cols() < n_queries) {
    query_info->query_lut =
        std::make_shared<Eigen::MatrixXf>(k_codes * n_subspaces, n_queries);
  }
  Eigen::MatrixXf& lut = *(query_info->query_lut);
  int total_dims = 0;
  for (int i = 0; i < n_subspaces; ++i) {
    if (query_distance_ == SQUARED_L2_DISTANCE) {
      lut.block(i * k_codes, 0, k_codes, n_queries).rowwise() =
          queries.block(total_dims, 0, codebooks_[i].cols(), n_queries)
              .colwise()
              .squaredNorm();
      lut.block(i * k_codes, 0, k_codes, n_queries).colwise() +=
          codebook_norms_[i];
      lut.block(i * k_codes, 0, k_codes, n_queries) -=
          2 * codebooks_[i] *
          queries.block(total_dims, 0, codebooks_[i].cols(), n_queries);
    } else if (query_distance_ == DOT_PRODUCT) {
      lut.block(i * k_codes, 0, k_codes, n_queries) =
          -codebooks_[i] *
          queries.block(total_dims, 0, codebooks_[i].cols(), n_queries);
    } else {
      LOG(ERROR) << "Unsupported distance measure: "
                 << DistanceMeasure_Name(query_distance_);
      return false;
    }
    total_dims += codebooks_[i].cols();
  }
  if (lookup_type_ == AsymmetricHashingProto::FLOAT) {
    if (query_info->transposed_query_lut == nullptr ||
        query_info->transposed_query_lut->cols() != lut.cols()) {
      query_info->transposed_query_lut =
          std::make_shared<QueryInfo::Matrix<float>>(lut.rows(), lut.cols());
    }
    RearrangeLUT(query_info);
  } else if (lookup_type_ == AsymmetricHashingProto::INT16) {
    if (query_info->query_lut_uint16 == nullptr ||
        query_info->query_lut_uint16->cols() != lut.cols()) {
      query_info->query_lut_uint16 =
          std::make_shared<QueryInfo::Matrix<uint16_t>>(lut.rows(), lut.cols());
    }
    if (query_info->transposed_query_lut_uint16 == nullptr ||
        query_info->transposed_query_lut_uint16->cols() != lut.cols()) {
      query_info->transposed_query_lut_uint16 =
          std::make_shared<QueryInfo::Matrix<uint16_t>>(lut.rows(), lut.cols());
    }
    ConvertLookupToFixedPoint<uint16_t>(query_info);
    RearrangeLUT(query_info);
  } else if (lookup_type_ == AsymmetricHashingProto::INT8) {
    if (query_info->query_lut_uint8 == nullptr ||
        query_info->query_lut_uint8->cols() != lut.cols()) {
      query_info->query_lut_uint8 =
          std::make_shared<QueryInfo::Matrix<uint8_t>>(lut.rows(), lut.cols());
    }
    if (query_info->transposed_query_lut_uint8 == nullptr ||
        query_info->transposed_query_lut_uint8->cols() != lut.cols()) {
      query_info->transposed_query_lut_uint8 =
          std::make_shared<QueryInfo::Matrix<uint8_t>>(lut.rows(), lut.cols());
    }
    ConvertLookupToFixedPoint<uint8_t>(query_info);
    RearrangeLUT(query_info);
  }

  return true;
}

void AsymmetricHashQuerier::RearrangeLUT(QueryInfo* query_info) const {
  if (lookup_type_ == AsymmetricHashingProto::FLOAT) {
    RearrangeLUTHelper<float>(query_info);
  } else if (lookup_type_ == AsymmetricHashingProto::INT16) {
    RearrangeLUTHelper<uint16_t>(query_info);
  } else if (lookup_type_ == AsymmetricHashingProto::INT8) {
    RearrangeLUTHelper<uint8_t>(query_info);
  }
}
}  // namespace core
}  // namespace scann_ondevice
}  // namespace tflite

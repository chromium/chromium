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

#include <cstdint>

#include "tensorflow_lite_support/scann_ondevice/cc/core/index_table_sum.h"
#include "tensorflow_lite_support/scann_ondevice/cc/core/processor.h"

namespace tflite {
namespace scann_ondevice {
namespace core {
namespace internal {

void ComputeAHDistance(const QueryInfo& query_info,
                       Eigen::Ref<const Matrix8u> database,
                       Eigen::Ref<Eigen::MatrixXf> output) {
  const int num_database = database.cols();
  const int num_chunks = database.rows();
  if (query_info.transposed_query_lut) {
    const int batch_size = query_info.transposed_query_lut->cols();
    const int num_centers =
        query_info.transposed_query_lut->rows() / num_chunks;
    IndexTableSum<float>(database.data(), num_chunks, num_database,
                         query_info.transposed_query_lut->data(), batch_size,
                         num_centers, std::nanf(""), std::nanf(""),
                         reinterpret_cast<float*>(output.data()));
  } else if (query_info.transposed_query_lut_uint16) {
    const int batch_size = query_info.transposed_query_lut_uint16->cols();
    const int num_centers =
        query_info.transposed_query_lut_uint16->rows() / num_chunks;
    IndexTableSum<uint16_t>(database.data(), num_chunks, num_database,
                            query_info.transposed_query_lut_uint16->data(),
                            batch_size, num_centers, query_info.fixed_point_min,
                            query_info.fixed_point_max,
                            reinterpret_cast<float*>(output.data()));
  } else if (query_info.transposed_query_lut_uint8) {
    const int batch_size = query_info.transposed_query_lut_uint8->cols();
    const int num_centers =
        query_info.transposed_query_lut_uint8->rows() / num_chunks;
    IndexTableSum<uint8_t>(database.data(), num_chunks, num_database,
                           query_info.transposed_query_lut_uint8->data(),
                           batch_size, num_centers, query_info.fixed_point_min,
                           query_info.fixed_point_max,
                           reinterpret_cast<float*>(output.data()));
  }
}

}  // namespace internal
}  // namespace core
}  // namespace scann_ondevice
}  // namespace tflite

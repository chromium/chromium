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

#include <string>

#include "tensorflow_lite_support/scann_ondevice/cc/core/serialized_searcher.pb.h"
#include "absl/types/optional.h"  // from @com_google_absl
#include "absl/types/span.h"  // from @com_google_absl
#include "pybind11/cast.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11_abseil/absl_casters.h"  // from @pybind11_abseil
#include "pybind11_abseil/status_casters.h"  // from @pybind11_abseil
#include "tensorflow_lite_support/scann_ondevice/cc/index_builder.h"

namespace pybind11 {

PYBIND11_MODULE(index_builder, m) {
  google::ImportStatusModule();

  m.def(
      "create_serialized_index_file",
      [](const uint32_t embedding_dim, const std::string& serialized_config,
         const std::string userinfo,
         absl::Span<const uint32_t> partition_assignment,
         absl::Span<const std::string> metadata, bool compression,
         absl::optional<absl::Span<const uint8_t>> hashed_database,
         absl::optional<absl::Span<const float>> float_database)
          -> absl::StatusOr<bytes> {
        tflite::scann_ondevice::core::ScannOnDeviceConfig config;
        config.ParseFromString(serialized_config);
        const auto status_or_bytes = tflite::scann_ondevice::CreateIndexBuffer(
            {.config = config,
             .embedding_dim = embedding_dim,
             .hashed_database = hashed_database,
             .float_database = float_database,
             .partition_assignment = partition_assignment,
             .metadata = metadata,
             .userinfo = userinfo},
            compression);
        if (!status_or_bytes.ok()) {
          return status_or_bytes.status();
        }
        return bytes(status_or_bytes.value());
      },
      arg("embedding_dim"), arg("serialized_config"), arg("userinfo"),
      arg("partition_assignment"), arg("metadata"), arg("compression") = true,
      arg("hashed_database") = absl::nullopt,
      arg("float_database") = absl::nullopt);
}

}  // namespace pybind11

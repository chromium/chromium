// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/distributed_point_functions/code/dpf/distributed_point_function.h"
#include "third_party/distributed_point_functions/dpf/distributed_point_function.pb.h"
#include "third_party/distributed_point_functions/shim/distributed_point_function_shim.h"

namespace distributed_point_functions {
std::optional<std::pair<DpfKey, DpfKey>> GenerateKeysIncremental(
    std::vector<DpfParameters> parameters,
    absl::uint128 alpha,
    std::vector<absl::uint128> beta) {
  // absl::StatusOr is not allowed in the codebase, but this minimal usage is
  // necessary to interact with //third_party/distributed_point_functions/.
  absl::StatusOr<std::unique_ptr<DistributedPointFunction>> dpf_result =
      DistributedPointFunction::CreateIncremental(std::move(parameters));
  if (!dpf_result.ok()) {
    LOG(ERROR) << "CreateIncremental() failed: " << dpf_result.status();
    return std::nullopt;
  }
  CHECK_NE(*dpf_result, nullptr);

  absl::StatusOr<std::pair<DpfKey, DpfKey>> keys_result =
      (*dpf_result)->GenerateKeysIncremental(alpha, std::move(beta));
  if (!keys_result.ok()) {
    LOG(ERROR) << "GenerateKeysIncremental() failed: " << keys_result.status();
    return std::nullopt;
  }
  return std::move(*keys_result);
}
}  // namespace distributed_point_functions

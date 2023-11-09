// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/distributed_point_functions/dpf/distributed_point_function.pb.h"
#include "third_party/distributed_point_functions/shim/distributed_point_function_shim.h"

// NOTE: To avoid name-colliding macros named "CHECK", this header must not be
// transitively included into a compilation unit that also uses base/check.h.
// See <https://crbug.com/1499970>.
#include "third_party/distributed_point_functions/code/dpf/distributed_point_function.h"

namespace distributed_point_functions {
std::optional<std::pair<DpfKey, DpfKey>> GenerateKeysIncremental(
    std::vector<DpfParameters> parameters,
    absl::uint128 alpha,
    std::vector<absl::uint128> beta) {
  // absl::StatusOr is not allowed in the codebase, but this minimal usage is
  // necessary to interact with //third_party/distributed_point_functions/.
  absl::StatusOr<std::unique_ptr<DistributedPointFunction>> status_or_dpf =
      DistributedPointFunction::CreateIncremental(std::move(parameters));
  if (!status_or_dpf.ok() || !status_or_dpf.value()) {
    return std::nullopt;
  }
  absl::StatusOr<std::pair<DpfKey, DpfKey>> status_or_dpf_keys =
      status_or_dpf.value()->GenerateKeysIncremental(alpha, std::move(beta));
  if (!status_or_dpf_keys.ok()) {
    return std::nullopt;
  }
  return std::move(status_or_dpf_keys.value());
}
}  // namespace distributed_point_functions

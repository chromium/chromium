// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <optional>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/distributed_point_functions/dpf/distributed_point_function.pb.h"
#include "third_party/distributed_point_functions/shim/distributed_point_function_shim.h"

namespace distributed_point_functions {

// The shim's GenerateKeysIncremental() can return a value besides std::nullopt.
TEST(DistributedPointFunctionShimTest, GenerateKeysIncrementalConstructsKeys) {
  constexpr size_t kBitLength = 32;
  std::vector<DpfParameters> parameters(kBitLength);
  for (size_t i = 0; i < parameters.size(); ++i) {
    parameters[i].set_log_domain_size(i + 1);
    parameters[i].mutable_value_type()->mutable_integer()->set_bitsize(
        parameters.size());
  }
  std::optional<std::pair<DpfKey, DpfKey>> maybe_dpf_keys =
      GenerateKeysIncremental(
          std::move(parameters),
          /*alpha=*/absl::uint128{1},
          /*beta=*/std::vector<absl::uint128>(kBitLength, absl::uint128{1}));
  EXPECT_TRUE(maybe_dpf_keys.has_value());
}

// When DistributedPointFunction::CreateIncremental() fails, the shim's
// GenerateKeysIncremental() should return std::nullopt.
TEST(DistributedPointFunctionShimTest, GenerateKeysIncrementalEmptyParameters) {
  EXPECT_FALSE(GenerateKeysIncremental(/*parameters=*/{},
                                       /*alpha=*/absl::uint128{}, /*beta=*/{}));
}

// When the length of beta does not match the number of parameters, the internal
// call to DistributedPointFunction::GenerateKeysIncremental() will fail, and
// the shim's GenerateKeysIncremental() should return std::nullopt.
TEST(DistributedPointFunctionShimTest, GenerateKeysIncrementalBetaWrongSize) {
  std::vector<DpfParameters> parameters(3);
  EXPECT_FALSE(
      GenerateKeysIncremental(/*parameters=*/std::vector<DpfParameters>(3),
                              /*alpha=*/absl::uint128{}, /*beta=*/{1, 2, 3}));
}

}  // namespace distributed_point_functions

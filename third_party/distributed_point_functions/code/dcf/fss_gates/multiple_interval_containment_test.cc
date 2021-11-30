// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dcf/fss_gates/multiple_interval_containment.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "absl/numeric/int128.h"
#include "dcf/fss_gates/multiple_interval_containment.pb.h"
#include "dcf/fss_gates/prng/basic_rng.h"
#include "dpf/distributed_point_function.pb.h"
#include "dpf/internal/status_matchers.h"
#include "dpf/internal/value_type_helpers.h"
#include "dpf/status_macros.h"

namespace distributed_point_functions {
namespace fss_gates {
namespace {

using ::testing::Test;

TEST(MICTest, GenAndEvalSucceedsForSmallGroup) {
  MicParameters mic_parameters;
  const int group_size = 64;
  const uint64_t interval_count = 5;

  // Setting input and output group to be Z_{2^64}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{10, 23, 45, 66, 15};
  std::vector<absl::uint128> qs{45, 30, 100, 250, 15};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  // Creating a MIC gate
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MultipleIntervalContainmentGate> MicGate,
      MultipleIntervalContainmentGate::Create(mic_parameters));

  MicKey key_0, key_1;

  // Initializing the input and output masks uniformly at random;
  const absl::string_view kSampleSeed = absl::string_view();
  DPF_ASSERT_OK_AND_ASSIGN(
      auto rng, distributed_point_functions::BasicRng::Create(kSampleSeed));

  absl::uint128 N = absl::uint128(1) << mic_parameters.log_group_size();

  DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_in, rng->Rand64());
  r_in = r_in % N;

  std::vector<absl::uint128> r_outs;

  for (int i = 0; i < interval_count; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_out, rng->Rand64());
    r_out = r_out % N;
    r_outs.push_back(r_out);
  }

  // Generating MIC gate keys
  DPF_ASSERT_OK_AND_ASSIGN(std::tie(key_0, key_1), MicGate->Gen(r_in, r_outs));

  // Inside this loop we will test the Evaluation of the MIC gate on
  // input values ranging between [0, 400)
  for (uint64_t i = 0; i < 400; i++) {
    std::vector<absl::uint128> res_0, res_1;

    // Evaluating MIC gate key_0 on masked input i + r_in
    DPF_ASSERT_OK_AND_ASSIGN(res_0, MicGate->Eval(key_0, (i + r_in) % N));

    // Evaluating MIC gate key_1 on masked input i + r_in
    DPF_ASSERT_OK_AND_ASSIGN(res_1, MicGate->Eval(key_1, (i + r_in) % N));

    // Reconstructing the actual output of the MIC gate by adding together
    // the secret shared output res_0 and res_1, and then subtracting out
    // the output mask r_out
    for (int j = 0; j < interval_count; j++) {
      absl::uint128 result = (res_0[j] + res_1[j] - r_outs[j]) % N;

      // If the input i lies inside the j^th interval, then the expected
      // output of MIC gate is 1, and 0 otherwise
      if (i >= ps[j] && i <= qs[j]) {
        EXPECT_EQ(result, 1);
      } else {
        EXPECT_EQ(result, 0);
      }
    }
  }
}

TEST(MICTest, GenAndEvalSucceedsForLargeGroup) {
  MicParameters mic_parameters;
  const int group_size = 127;
  const uint64_t interval_count = 3;
  const absl::uint128 two_power_127 = absl::uint128(1) << 127;
  const absl::uint128 two_power_126 = absl::uint128(1) << 126;

  // Setting input and output group to be Z_{2^127}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals

  std::vector<absl::uint128> ps{two_power_126, two_power_127 - 1,
                                two_power_127 - 3};
  std::vector<absl::uint128> qs{two_power_126 + 3, two_power_127 - 1,
                                two_power_127 - 2};

  std::vector<absl::uint128> x{two_power_126 - 1, two_power_126,
                               two_power_126 + 1, two_power_126 + 2,
                               two_power_126 + 3, two_power_126 + 4,
                               two_power_127 - 4, two_power_127 - 3,
                               two_power_127 - 2, two_power_127 - 1};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_high(
        absl::Uint128High64(ps[i]));
    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_high(
        absl::Uint128High64(qs[i]));
    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  // Creating a MIC gate
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MultipleIntervalContainmentGate> MicGate,
      MultipleIntervalContainmentGate::Create(mic_parameters));

  MicKey key_0, key_1;

  absl::uint128 N = absl::uint128(1) << mic_parameters.log_group_size();

  // Initializing the input and output masks uniformly at random;
  const absl::string_view kSampleSeed = absl::string_view();
  DPF_ASSERT_OK_AND_ASSIGN(
      auto rng, distributed_point_functions::BasicRng::Create(kSampleSeed));

  DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_in, rng->Rand128());

  r_in = r_in % N;

  std::vector<absl::uint128> r_outs;

  for (int i = 0; i < interval_count; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_out, rng->Rand128());
    r_out = r_out % N;
    r_outs.push_back(r_out);
  }

  // Generating MIC gate keys
  DPF_ASSERT_OK_AND_ASSIGN(std::tie(key_0, key_1), MicGate->Gen(r_in, r_outs));

  // Inside this loop we will test the Evaluation of the MIC gate on
  // input values in the vicinity of interval boundaries which are hardcoded
  // in the vector x.
  for (uint64_t i = 0; i < x.size(); i++) {
    std::vector<absl::uint128> res_0, res_1;

    // Evaluating MIC gate key_0 on masked input
    DPF_ASSERT_OK_AND_ASSIGN(res_0, MicGate->Eval(key_0, (x[i] + r_in) % N));

    // Evaluating MIC gate key_1 on masked input
    DPF_ASSERT_OK_AND_ASSIGN(res_1, MicGate->Eval(key_1, (x[i] + r_in) % N));

    // Reconstructing the actual output of the MIC gate by adding together
    // the secret shared output res_0 and res_1, and then subtracting out
    // the output mask r_out
    for (int j = 0; j < interval_count; j++) {
      absl::uint128 result = (res_0[j] + res_1[j] - r_outs[j]) % N;

      // If the input lies inside the j^th interval, then the expected
      // output of MIC gate is 1, and 0 otherwise
      if (x[i] >= ps[j] && x[i] <= qs[j]) {
        EXPECT_EQ(result, 1);
      } else {
        EXPECT_EQ(result, 0);
      }
    }
  }
}

TEST(MICTest, CreateFailsWith128bitGroup) {
  MicParameters mic_parameters;
  const int group_size = 128;
  const uint64_t interval_count = 5;

  // Setting input and output group to be Z_{2^128}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{10, 23, 45, 66, 15};
  std::vector<absl::uint128> qs{45, 30, 100, 250, 15};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  EXPECT_THAT(
      MultipleIntervalContainmentGate::Create(mic_parameters),
      dpf_internal::StatusIs(absl::StatusCode::kInvalidArgument,
                             "log_group_size should be in > 0 and < 128"));
}

TEST(MICTest, CreateFailsForIntervalBoundariesOutsideGroup) {
  MicParameters mic_parameters;
  const int group_size = 20;
  const uint64_t interval_count = 1;
  const absl::uint128 two_power_20 = absl::uint128(1) << 20;

  // Setting input and output group to be Z_{2^20}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{4};
  std::vector<absl::uint128> qs{two_power_20};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  EXPECT_THAT(MultipleIntervalContainmentGate::Create(mic_parameters),
              dpf_internal::StatusIs(
                  absl::StatusCode::kInvalidArgument,
                  "Interval bounds should be between 0 and 2^log_group_size"));
}

TEST(MICTest, CreateFailsForInvalidIntervalBoundaries) {
  MicParameters mic_parameters;
  const int group_size = 20;
  const uint64_t interval_count = 1;

  // Setting input and output group to be Z_{2^20}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{4};
  std::vector<absl::uint128> qs{3};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  EXPECT_THAT(
      MultipleIntervalContainmentGate::Create(mic_parameters),
      dpf_internal::StatusIs(absl::StatusCode::kInvalidArgument,
                             "Interval upper bounds should be >= lower bound"));
}

TEST(MICTest, CreateFailsForEmptyInterval) {
  MicParameters mic_parameters;
  const int group_size = 20;
  const uint64_t interval_count = 1;

  // Setting input and output group to be Z_{2^20}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{};
  std::vector<absl::uint128> qs{3};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    // Only setting upper bound (and skipping lower bound)
    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  EXPECT_THAT(MultipleIntervalContainmentGate::Create(mic_parameters),
              dpf_internal::StatusIs(absl::StatusCode::kInvalidArgument,
                                     "Intervals should be non-empty"));
}

TEST(MICTest, GenFailsForIncorrectNumberOfOutputMasks) {
  MicParameters mic_parameters;
  const int group_size = 64;
  const uint64_t interval_count = 5;

  // Setting input and output group to be Z_{2^64}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{10, 23, 45, 66, 15};
  std::vector<absl::uint128> qs{45, 30, 100, 250, 15};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  // Creating a MIC gate
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MultipleIntervalContainmentGate> MicGate,
      MultipleIntervalContainmentGate::Create(mic_parameters));

  MicKey key_0, key_1;

  absl::uint128 N = absl::uint128(1) << mic_parameters.log_group_size();

  // Initializing the input and output masks uniformly at random;
  const absl::string_view kSampleSeed = absl::string_view();
  DPF_ASSERT_OK_AND_ASSIGN(
      auto rng, distributed_point_functions::BasicRng::Create(kSampleSeed));

  DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_in, rng->Rand64());
  r_in = r_in % N;

  std::vector<absl::uint128> r_outs;

  // Setting only (interval_count - 1) many output masks
  for (int i = 0; i < interval_count - 1; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_out, rng->Rand64());
    r_out = r_out % N;
    r_outs.push_back(r_out);
  }

  // Generating MIC gate keys
  EXPECT_THAT(
      MicGate->Gen(r_in, r_outs),
      dpf_internal::StatusIs(
          absl::StatusCode::kInvalidArgument,
          "Count of output masks should be equal to the number of intervals"));
}

TEST(MICTest, GenFailsForInputMaskOutsideGroup) {
  MicParameters mic_parameters;
  const int group_size = 10;
  const uint64_t interval_count = 1;

  // Setting input and output group to be Z_{2^10}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{10};
  std::vector<absl::uint128> qs{45};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  // Creating a MIC gate
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MultipleIntervalContainmentGate> MicGate,
      MultipleIntervalContainmentGate::Create(mic_parameters));

  MicKey key_0, key_1;

  absl::uint128 N = absl::uint128(1) << mic_parameters.log_group_size();

  const absl::string_view kSampleSeed = absl::string_view();
  DPF_ASSERT_OK_AND_ASSIGN(
      auto rng, distributed_point_functions::BasicRng::Create(kSampleSeed));

  // Fixing r_in to be an element outside group
  absl::uint128 r_in = 2048;

  std::vector<absl::uint128> r_outs;

  // Initializing the output masks uniformly at random;
  for (int i = 0; i < interval_count; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_out, rng->Rand64());
    r_out = r_out % N;
    r_outs.push_back(r_out);
  }

  // Generating MIC gate keys
  EXPECT_THAT(MicGate->Gen(r_in, r_outs),
              dpf_internal::StatusIs(
                  absl::StatusCode::kInvalidArgument,
                  "Input mask should be between 0 and 2^log_group_size"));
}

TEST(MICTest, GenFailsForOutputMaskOutsideGroup) {
  MicParameters mic_parameters;
  const int group_size = 10;
  const uint64_t interval_count = 1;

  // Setting input and output group to be Z_{2^10}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{10};
  std::vector<absl::uint128> qs{45};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  // Creating a MIC gate
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MultipleIntervalContainmentGate> MicGate,
      MultipleIntervalContainmentGate::Create(mic_parameters));

  MicKey key_0, key_1;

  absl::uint128 N = absl::uint128(1) << mic_parameters.log_group_size();

  const absl::string_view kSampleSeed = absl::string_view();
  DPF_ASSERT_OK_AND_ASSIGN(
      auto rng, distributed_point_functions::BasicRng::Create(kSampleSeed));

  // Initializing the input masks uniformly at random;
  DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_in, rng->Rand64());
  r_in = r_in % N;

  std::vector<absl::uint128> r_outs;

  // Fixing the output masks to be elements outside group;
  for (int i = 0; i < interval_count; ++i) {
    absl::uint128 r_out = 2048;
    r_outs.push_back(r_out);
  }

  // Generating MIC gate keys
  EXPECT_THAT(MicGate->Gen(r_in, r_outs),
              dpf_internal::StatusIs(
                  absl::StatusCode::kInvalidArgument,
                  "Output mask should be between 0 and 2^log_group_size"));
}

TEST(MICTest, EvalFailsForMaskedInputOutsideGroup) {
  MicParameters mic_parameters;
  const int group_size = 64;
  const uint64_t interval_count = 1;

  // Setting input and output group to be Z_{2^64}
  mic_parameters.set_log_group_size(group_size);

  // Setting up the lower bound and upper bounds for intervals
  std::vector<absl::uint128> ps{10};
  std::vector<absl::uint128> qs{45};

  for (int i = 0; i < interval_count; ++i) {
    Interval* interval = mic_parameters.add_intervals();

    interval->mutable_lower_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(ps[i]));

    interval->mutable_upper_bound()->mutable_value_uint128()->set_low(
        absl::Uint128Low64(qs[i]));
  }

  // Creating a MIC gate
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MultipleIntervalContainmentGate> MicGate,
      MultipleIntervalContainmentGate::Create(mic_parameters));

  MicKey key_0, key_1;

  // Initializing the input and output masks uniformly at random;
  const absl::string_view kSampleSeed = absl::string_view();
  DPF_ASSERT_OK_AND_ASSIGN(
      auto rng, distributed_point_functions::BasicRng::Create(kSampleSeed));

  absl::uint128 N = absl::uint128(1) << mic_parameters.log_group_size();

  DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_in, rng->Rand64());
  r_in = r_in % N;

  std::vector<absl::uint128> r_outs;

  for (int i = 0; i < interval_count; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r_out, rng->Rand64());
    r_out = r_out % N;
    r_outs.push_back(r_out);
  }

  // Generating MIC gate keys
  DPF_ASSERT_OK_AND_ASSIGN(std::tie(key_0, key_1), MicGate->Gen(r_in, r_outs));

  // Calling Eval on a masked input which is not a group element
  EXPECT_THAT(MicGate->Eval(key_0, absl::uint128(1) << 72),
              dpf_internal::StatusIs(
                  absl::StatusCode::kInvalidArgument,
                  "Masked input should be between 0 and 2^log_group_size"));
}

}  // namespace
}  // namespace fss_gates
}  // namespace distributed_point_functions

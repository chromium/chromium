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

#include "dpf/distributed_point_function.h"

#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/config.h"
#include "absl/numeric/int128.h"
#include "absl/random/random.h"
#include "absl/random/uniform_int_distribution.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "absl/utility/utility.h"
#include "dpf/distributed_point_function.pb.h"
#include "dpf/internal/proto_validator.h"
#include "dpf/internal/status_matchers.h"
#include "dpf/xor_wrapper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace distributed_point_functions {
namespace {

using dpf_internal::IsOk;
using dpf_internal::IsOkAndHolds;
using dpf_internal::StatusIs;
using ::testing::HasSubstr;
using ::testing::Ne;
using ::testing::StartsWith;

TEST(DistributedPointFunction, TestCreate) {
  for (int log_domain_size = 0; log_domain_size <= 128; ++log_domain_size) {
    for (int element_bitsize = 1; element_bitsize <= 128;
         element_bitsize *= 2) {
      DpfParameters parameters;

      parameters.set_log_domain_size(log_domain_size);
      parameters.mutable_value_type()->mutable_integer()->set_bitsize(
          element_bitsize);

      EXPECT_THAT(DistributedPointFunction::Create(parameters),
                  IsOkAndHolds(Ne(nullptr)))
          << "log_domain_size=" << log_domain_size
          << " element_bitsize=" << element_bitsize;
    }
  }
}

TEST(DistributedPointFunction, TestCreateIncrementalLargeDomain) {
  std::vector<DpfParameters> parameters(2);
  parameters[0].mutable_value_type()->mutable_integer()->set_bitsize(128);
  parameters[1].mutable_value_type()->mutable_integer()->set_bitsize(128);

  // Test that creating an incremental DPF with a large total domain size works.
  parameters[0].set_log_domain_size(10);
  parameters[1].set_log_domain_size(100);

  EXPECT_THAT(DistributedPointFunction::CreateIncremental(parameters),
              IsOkAndHolds(Ne(nullptr)));
}

TEST(DistributedPointFunction, CreateFailsForTupleTypesWithDifferentIntModN) {
  DpfParameters parameters;
  parameters.set_log_domain_size(10);
  *(parameters.mutable_value_type()) =
      ToValueType<Tuple<IntModN<uint32_t, 3>, IntModN<uint64_t, 4>>>();

  EXPECT_THAT(
      DistributedPointFunction::Create(parameters),
      StatusIs(absl::StatusCode::kUnimplemented,
               "All elements of type IntModN in a tuple must be the same"));
}

TEST(DistributedPointFunction, CreateFailsForMissingValueType) {
  DpfParameters parameters;
  parameters.set_log_domain_size(10);

  EXPECT_THAT(
      DistributedPointFunction::Create(parameters),
      StatusIs(absl::StatusCode::kInvalidArgument, "`value_type` is required"));
}

TEST(DistributedPointFunction, CreateFailsForInvalidValueType) {
  DpfParameters parameters;
  parameters.set_log_domain_size(10);
  *(parameters.mutable_value_type()) = ValueType{};

  EXPECT_THAT(DistributedPointFunction::Create(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       StartsWith("ValidateValueType: Unsupported ValueType")));
}

TEST(DistributedPointFunction, TestGenerateKeysIncrementalVariadicTemplate) {
  std::vector<DpfParameters> parameters(2);

  parameters[0].set_log_domain_size(10);
  parameters[1].set_log_domain_size(20);
  *(parameters[0].mutable_value_type()) = ToValueType<uint16_t>();
  *(parameters[1].mutable_value_type()) =
      ToValueType<Tuple<uint32_t, uint64_t>>();
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<DistributedPointFunction> dpf,
      DistributedPointFunction::CreateIncremental(parameters));

  absl::StatusOr<std::pair<DpfKey, DpfKey>> keys = dpf->GenerateKeysIncremental(
      23, uint16_t{42}, Tuple<uint32_t, uint64_t>{123, 456});
  EXPECT_THAT(keys, IsOk());
}

TEST(DistributedPointFunction, TestGenerateKeysIncrementalTemplate) {
  std::vector<DpfParameters> parameters(2);
  using T = XorWrapper<absl::uint128>;

  parameters[0].set_log_domain_size(10);
  parameters[1].set_log_domain_size(20);
  *(parameters[0].mutable_value_type()) = ToValueType<T>();
  *(parameters[1].mutable_value_type()) = ToValueType<T>();
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<DistributedPointFunction> dpf,
      DistributedPointFunction::CreateIncremental(parameters));

  absl::StatusOr<std::pair<DpfKey, DpfKey>> keys =
      dpf->GenerateKeysIncremental(23, T{42}, T{123});
  EXPECT_THAT(keys, IsOk());
}

TEST(DistributedPointFunction, KeyGenerationFailsIfValueTypeNotRegistered) {
  DpfParameters parameters;
  parameters.set_log_domain_size(10);
  parameters.mutable_value_type()
      ->mutable_tuple()
      ->add_elements()
      ->mutable_integer()
      ->set_bitsize(32);
  DPF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<DistributedPointFunction> dpf,
                           DistributedPointFunction::Create(parameters));

  // Tuple<uint32_t> should not be registered by default.
  absl::uint128 alpha = 23;
  Value beta;
  beta.mutable_tuple()->add_elements()->mutable_integer()->set_value_uint64(42);

  EXPECT_THAT(dpf->GenerateKeys(alpha, beta),
              StatusIs(absl::StatusCode::kFailedPrecondition,
                       StartsWith("No value correction function known")));
}

TEST(DistributedPointFunction, EvaluationFailsIfDomainSizeGapTooLarge) {
  std::vector<DpfParameters> parameters(2);
  parameters[0].mutable_value_type()->mutable_integer()->set_bitsize(128);
  parameters[1].mutable_value_type()->mutable_integer()->set_bitsize(128);
  parameters[0].set_log_domain_size(10);
  parameters[1].set_log_domain_size(100);
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<DistributedPointFunction> dpf,
      DistributedPointFunction::CreateIncremental(parameters));

  std::pair<DpfKey, DpfKey> keys;
  DPF_ASSERT_OK_AND_ASSIGN(keys, dpf->GenerateKeysIncremental(123, 456u, 789u));
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf->CreateEvaluationContext(keys.first));

  EXPECT_THAT(
      dpf->EvaluateUntil<absl::uint128>(1, {}, ctx),
      StatusIs(absl::StatusCode::kInvalidArgument, StartsWith("Domain size")));
}

TEST(DistributedPointFunction, EvaluationFailsIfOutputSizeTooLarge) {
  std::vector<DpfParameters> parameters(2);
  parameters[0].mutable_value_type()->mutable_integer()->set_bitsize(128);
  parameters[1].mutable_value_type()->mutable_integer()->set_bitsize(128);
  parameters[0].set_log_domain_size(10);
  parameters[1].set_log_domain_size(72);
  DPF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<DistributedPointFunction> dpf,
      DistributedPointFunction::CreateIncremental(parameters));

  std::pair<DpfKey, DpfKey> keys;
  DPF_ASSERT_OK_AND_ASSIGN(keys, dpf->GenerateKeysIncremental(123, 456u, 789u));
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf->CreateEvaluationContext(keys.first));

  // Evaluate on 2**2 prefixes, bringing the output size to 2**(72-10+2) =
  // 2**64, which overflows an int64_t. Assumes a size_t is at most 64 bits.
  std::vector<absl::uint128> prefixes = {123, 456, 789, 1011};
  DPF_ASSERT_OK(dpf->EvaluateUntil<absl::uint128>(0, {}, ctx));
  EXPECT_THAT(
      dpf->EvaluateUntil<absl::uint128>(1, prefixes, ctx),
      StatusIs(absl::StatusCode::kInvalidArgument, StartsWith("Output size")));
}

TEST(DistributedPointFunction, TestSinglePointPartialEvaluation) {
  // Two hierarchy levels: The first will be evaluated with only a single
  // prefix, the second will be fully evaluated.
  std::vector<DpfParameters> parameters(2);
  parameters[0].set_log_domain_size(108);
  parameters[0].mutable_value_type()->mutable_integer()->set_bitsize(32);
  parameters[1].set_log_domain_size(128);
  parameters[1].mutable_value_type()->mutable_integer()->set_bitsize(32);

  DPF_ASSERT_OK_AND_ASSIGN(
      auto dpf, DistributedPointFunction::CreateIncremental(parameters));
  absl::uint128 prefix = 0xdeadbeef;
  absl::uint128 suffix = 23;
  absl::uint128 alpha = (prefix << 20) + suffix;
  uint32_t beta = 42;
  DpfKey key_a, key_b;
  DPF_ASSERT_OK_AND_ASSIGN(std::tie(key_a, key_b),
                           dpf->GenerateKeysIncremental(alpha, {beta, beta}));

  // First evaluate directly up to `prefix`
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx_a,
                           dpf->CreateEvaluationContext(key_a));
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx_b,
                           dpf->CreateEvaluationContext(key_b));
  std::vector<uint32_t> result_a, result_b;
  DPF_ASSERT_OK_AND_ASSIGN(result_a,
                           dpf->EvaluateAt<uint32_t>(0, {prefix}, ctx_a));
  DPF_ASSERT_OK_AND_ASSIGN(result_b,
                           dpf->EvaluateAt<uint32_t>(0, {prefix}, ctx_b));
  EXPECT_EQ(result_a[0] + result_b[0], beta);

  // Now fully evaluate the second level.
  DPF_ASSERT_OK_AND_ASSIGN(result_a,
                           dpf->EvaluateUntil<uint32_t>(1, {prefix}, ctx_a));
  DPF_ASSERT_OK_AND_ASSIGN(result_b,
                           dpf->EvaluateUntil<uint32_t>(1, {prefix}, ctx_b));
  EXPECT_EQ(result_a.size(), result_b.size());
  EXPECT_EQ(result_a.size(), 1 << 20);
  for (int i = 0; i < static_cast<int>(result_a.size()); ++i) {
    if (i == suffix) {
      EXPECT_EQ(result_a[i] + result_b[i], beta);
    } else {
      EXPECT_EQ(result_a[i] + result_b[i], 0);
    }
  }
}

class RegularDpfKeyGenerationTest
    : public testing::TestWithParam<std::tuple<int, int>> {
 public:
  void SetUp() {
    std::tie(log_domain_size_, element_bitsize_) = GetParam();
    DpfParameters parameters;
    parameters.set_log_domain_size(log_domain_size_);
    parameters.mutable_value_type()->mutable_integer()->set_bitsize(
        element_bitsize_);
    DPF_ASSERT_OK_AND_ASSIGN(dpf_,
                             DistributedPointFunction::Create(parameters));
    DPF_ASSERT_OK_AND_ASSIGN(
        proto_validator_, dpf_internal::ProtoValidator::Create({parameters}));
  }

 protected:
  int log_domain_size_;
  int element_bitsize_;
  std::unique_ptr<DistributedPointFunction> dpf_;
  std::unique_ptr<dpf_internal::ProtoValidator> proto_validator_;
};

TEST_P(RegularDpfKeyGenerationTest, KeyHasCorrectFormat) {
  DpfKey key_a, key_b;
  DPF_ASSERT_OK_AND_ASSIGN(std::tie(key_a, key_b), dpf_->GenerateKeys(0, 0));

  // Check that party is set correctly.
  EXPECT_EQ(key_a.party(), 0);
  EXPECT_EQ(key_b.party(), 1);
  // Check that keys are accepted by proto_validator_.
  DPF_EXPECT_OK(proto_validator_->ValidateDpfKey(key_a));
  DPF_EXPECT_OK(proto_validator_->ValidateDpfKey(key_b));
}

TEST_P(RegularDpfKeyGenerationTest, FailsIfBetaHasTheWrongSize) {
  EXPECT_THAT(
      dpf_->GenerateKeysIncremental(0, {1, 2}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "`beta` has to have the same size as `parameters` passed at "
               "construction"));
}

TEST_P(RegularDpfKeyGenerationTest, FailsIfAlphaIsTooLarge) {
  if (log_domain_size_ >= 128) {
    // Alpha is an absl::uint128, so never too large in this case.
    return;
  }

  EXPECT_THAT(dpf_->GenerateKeys((absl::uint128{1} << log_domain_size_), 1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`alpha` must be smaller than the output domain size"));
}

TEST_P(RegularDpfKeyGenerationTest, FailsIfBetaIsTooLarge) {
  if (element_bitsize_ >= 128) {
    // Beta is an absl::uint128, so never too large in this case.
    return;
  }

  const auto beta = absl::uint128{1} << element_bitsize_;

  // Not testing error message, as it's an implementation detail of
  // ProtoValidator.
  EXPECT_THAT(dpf_->GenerateKeys(0, beta),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

INSTANTIATE_TEST_SUITE_P(VaryDomainAndElementSizes, RegularDpfKeyGenerationTest,
                         testing::Combine(testing::Values(0, 1, 2, 3, 4, 5, 6,
                                                          7, 8, 9, 10, 32, 62),
                                          testing::Values(8, 16, 32, 64, 128)));

struct DpfTestParameters {
  int log_domain_size;
  int element_bitsize;

  friend std::ostream& operator<<(std::ostream& os,
                                  const DpfTestParameters& parameters) {
    return os << "{ log_domain_size: " << parameters.log_domain_size
              << ", element_bitsize: " << parameters.element_bitsize << " }";
  }
};

class IncrementalDpfTest : public testing::TestWithParam<
                               std::tuple</*parameters*/
                                          std::vector<DpfTestParameters>,
                                          /*alpha*/ absl::uint128,
                                          /*beta*/ std::vector<absl::uint128>,
                                          /*level_step*/ int,
                                          /*single_point*/ bool>> {
 protected:
  void SetUp() {
    const std::vector<DpfTestParameters>& parameters = std::get<0>(GetParam());
    parameters_.resize(parameters.size());
    for (int i = 0; i < static_cast<int>(parameters.size()); ++i) {
      parameters_[i].set_log_domain_size(parameters[i].log_domain_size);
      parameters_[i].mutable_value_type()->mutable_integer()->set_bitsize(
          parameters[i].element_bitsize);
    }
    DPF_ASSERT_OK_AND_ASSIGN(
        dpf_, DistributedPointFunction::CreateIncremental(parameters_));
    alpha_ = std::get<1>(GetParam());
    beta_ = std::get<2>(GetParam());
    DPF_ASSERT_OK_AND_ASSIGN(keys_,
                             dpf_->GenerateKeysIncremental(alpha_, beta_));
    level_step_ = std::get<3>(
        GetParam());  // Number of hierarchy level to evaluate at once.
    single_point_ = std::get<4>(GetParam());
    DPF_ASSERT_OK_AND_ASSIGN(proto_validator_,
                             dpf_internal::ProtoValidator::Create(parameters_));
  }

  // Returns the prefix of `index` for the domain of `hierarchy_level`.
  absl::uint128 GetPrefixForLevel(int hierarchy_level, absl::uint128 index) {
    absl::uint128 result = 0;
    int shift_amount = parameters_.back().log_domain_size() -
                       parameters_[hierarchy_level].log_domain_size();
    if (shift_amount < 128) {
      result = index >> shift_amount;
    }
    return result;
  }

  // Evaluates both contexts `ctx0` and `ctx1` at `hierarchy level`, using the
  // appropriate prefixes of `evaluation_points`. Checks that the expansion of
  // both keys form correct DPF shares, i.e., they add up to
  // `beta_[ctx.hierarchy_level()]` under prefixes of `alpha_`, and to 0
  // otherwise. If `singl_point == true`, only evaluates at the prefixes of the
  // given `evaluation_points`. Otherwise, fully expands the given
  // `hierarchy_level`.
  template <typename T>
  void EvaluateAndCheckLevel(int hierarchy_level,
                             absl::Span<const absl::uint128> evaluation_points,
                             EvaluationContext& ctx0, EvaluationContext& ctx1) {
    int previous_hierarchy_level = ctx0.previous_hierarchy_level();
    int current_log_domain_size =
        parameters_[hierarchy_level].log_domain_size();
    int previous_log_domain_size = 0;
    int num_expansions = 1;
    bool is_first_evaluation = previous_hierarchy_level < 0;
    std::vector<absl::uint128> prefixes;
    if (single_point_) {
      // Single point evaluation: Generate prefixes for the current hierarchy
      // level.
      prefixes.resize(evaluation_points.size());
      for (int i = 0; i < static_cast<int>(evaluation_points.size()); ++i) {
        prefixes[i] = GetPrefixForLevel(hierarchy_level, evaluation_points[i]);
      }
    } else if (!is_first_evaluation) {
      // Full expansion: Generate prefixes for the previous hierarchy level if
      // we're not on the first level.
      num_expansions = static_cast<int>(evaluation_points.size());
      prefixes.resize(evaluation_points.size());
      previous_log_domain_size =
          parameters_[previous_hierarchy_level].log_domain_size();
      for (int i = 0; i < static_cast<int>(evaluation_points.size()); ++i) {
        prefixes[i] =
            GetPrefixForLevel(previous_hierarchy_level, evaluation_points[i]);
      }
    }

    absl::StatusOr<std::vector<T>> result_0, result_1;
    if (single_point_) {
      result_0 = dpf_->EvaluateAt<T>(hierarchy_level, prefixes, ctx0);
      result_1 = dpf_->EvaluateAt<T>(hierarchy_level, prefixes, ctx1);
    } else {
      result_0 = dpf_->EvaluateUntil<T>(hierarchy_level, prefixes, ctx0);
      result_1 = dpf_->EvaluateUntil<T>(hierarchy_level, prefixes, ctx1);
    }

    // Check results are ok.
    DPF_EXPECT_OK(result_0)
        << "hierarchy_level=" << hierarchy_level << "\nparameters={\n"
        << parameters_[hierarchy_level].DebugString() << "}\n";
    DPF_EXPECT_OK(result_1);
    if (result_0.ok() && result_1.ok()) {
      // Check output sizes match.
      ASSERT_EQ(result_0->size(), result_1->size());

      if (single_point_) {
        absl::uint128 current_alpha_prefix =
            GetPrefixForLevel(hierarchy_level, alpha_);
        for (int i = 0; i < result_0->size(); ++i) {
          if (prefixes[i] == current_alpha_prefix) {
            EXPECT_EQ(static_cast<T>((*result_0)[i] + (*result_1)[i]),
                      beta_[hierarchy_level])
                << "i=" << i << ", hierarchy_level=" << hierarchy_level
                << "\nparameters={\n"
                << parameters_[hierarchy_level].DebugString() << "}\n"
                << "current_alpha_prefix=" << current_alpha_prefix << "\n"
                << "prefixes[" << i << "]=" << prefixes[i] << "\n"
                << "evaluation_points[" << i << "]=" << evaluation_points[i]
                << "\n";
          } else {
            EXPECT_EQ(static_cast<T>((*result_0)[i] + (*result_1)[i]), 0)
                << "i=" << i << ", hierarchy_level=" << hierarchy_level
                << "\nparameters={\n"
                << parameters_[hierarchy_level].DebugString() << "}\n"
                << "current_alpha_prefix=" << current_alpha_prefix << "\n"
                << "prefixes[" << i << "]=" << prefixes[i] << "\n"
                << "evaluation_points[" << i << "]=" << evaluation_points[i]
                << "\n";
          }
        }
      } else {
        int64_t outputs_per_prefix =
            int64_t{1} << (current_log_domain_size - previous_log_domain_size);
        int64_t expected_output_size = num_expansions * outputs_per_prefix;
        ASSERT_EQ(result_0->size(), expected_output_size);

        // Iterate over the outputs and check that they sum up to 0 or to
        // beta_[current_hierarchy_level].
        absl::uint128 previous_alpha_prefix = 0;
        if (!is_first_evaluation) {
          previous_alpha_prefix =
              GetPrefixForLevel(previous_hierarchy_level, alpha_);
        }
        absl::uint128 current_alpha_prefix =
            GetPrefixForLevel(hierarchy_level, alpha_);
        for (int64_t i = 0; i < expected_output_size; ++i) {
          int prefix_index = i / outputs_per_prefix;
          int prefix_expansion_index = i % outputs_per_prefix;
          // The output is on the path to alpha, if we're at the first level or
          // under a prefix of alpha, and the current block in the expansion of
          // the prefix is also on the path to alpha.
          if ((is_first_evaluation ||
               prefixes[prefix_index] == previous_alpha_prefix) &&
              prefix_expansion_index ==
                  (current_alpha_prefix % outputs_per_prefix)) {
            // We need to static_cast here since otherwise operator+ returns an
            // unsigned int without doing a modular reduction, which causes the
            // test to fail on types with sizeof(T) < sizeof(unsigned).
            EXPECT_EQ(static_cast<T>((*result_0)[i] + (*result_1)[i]),
                      beta_[hierarchy_level])
                << "i=" << i << ", hierarchy_level=" << hierarchy_level
                << "\nparameters={\n"
                << parameters_[hierarchy_level].DebugString() << "}\n"
                << "previous_alpha_prefix=" << previous_alpha_prefix << "\n"
                << "current_alpha_prefix=" << current_alpha_prefix << "\n";
          } else {
            EXPECT_EQ(static_cast<T>((*result_0)[i] + (*result_1)[i]), 0)
                << "i=" << i << ", hierarchy_level=" << hierarchy_level
                << "\nparameters={\n"
                << parameters_[hierarchy_level].DebugString() << "}\n";
          }
        }
      }
    }
  }

  std::vector<DpfParameters> parameters_;
  std::unique_ptr<DistributedPointFunction> dpf_;
  absl::uint128 alpha_;
  std::vector<absl::uint128> beta_;
  std::pair<DpfKey, DpfKey> keys_;
  int level_step_;
  bool single_point_;
  std::unique_ptr<dpf_internal::ProtoValidator> proto_validator_;
};

TEST_P(IncrementalDpfTest, CreateEvaluationContextCreatesValidContext) {
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));
  DPF_EXPECT_OK(proto_validator_->ValidateEvaluationContext(ctx));
}

TEST_P(IncrementalDpfTest, FailsIfPrefixNotPresentInCtx) {
  if (parameters_.size() < 3 || parameters_[0].log_domain_size() < 1 ||
      parameters_[0].value_type().integer().bitsize() != 128 ||
      parameters_[1].value_type().integer().bitsize() != 128 ||
      parameters_[2].value_type().integer().bitsize() != 128) {
    return;
  }
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  // Evaluate on prefixes 0 and 1, then delete partial evaluations for prefix 0.
  DPF_ASSERT_OK(dpf_->EvaluateNext<absl::uint128>({}, ctx));
  DPF_ASSERT_OK(dpf_->EvaluateNext<absl::uint128>({0, 1}, ctx));
  ctx.mutable_partial_evaluations()->erase(ctx.partial_evaluations().begin());

  // The missing prefix corresponds to hierarchy level 1, even though we
  // evaluate at level 2.
  EXPECT_THAT(dpf_->EvaluateNext<absl::uint128>({0}, ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Prefix not present in ctx.partial_evaluations at "
                       "hierarchy level 1"));
}

TEST_P(IncrementalDpfTest, FailsIfDuplicatePrefixInCtx) {
  if (parameters_.size() < 3 || parameters_[0].log_domain_size() < 1 ||
      parameters_[0].value_type().integer().bitsize() != 128 ||
      parameters_[1].value_type().integer().bitsize() != 128 ||
      parameters_[2].value_type().integer().bitsize() != 128) {
    return;
  }
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  // Evaluate on prefixes 0 and 1, then add duplicate prefix for 0 with
  // different seed.
  DPF_ASSERT_OK(dpf_->EvaluateNext<absl::uint128>({}, ctx));
  DPF_ASSERT_OK(dpf_->EvaluateNext<absl::uint128>({0, 1}, ctx));
  *(ctx.add_partial_evaluations()) = ctx.partial_evaluations(0);
  Block changed_seed = ctx.partial_evaluations(0).seed();
  changed_seed.set_low(changed_seed.low() + 1);
  *((ctx.mutable_partial_evaluations()->end() - 1)->mutable_seed()) =
      changed_seed;

  // The missing prefix corresponds to hierarchy level 1, even though we
  // evaluate at level 2.
  EXPECT_THAT(dpf_->EvaluateNext<absl::uint128>({0}, ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Duplicate prefix in `ctx.partial_evaluations()` with "
                       "mismatching seed or control bit"));
}

TEST_P(IncrementalDpfTest, EvaluationFailsOnEmptyContext) {
  if (parameters_[0].value_type().integer().bitsize() != 128) {
    return;
  }
  EvaluationContext ctx;

  // We don't check the error message, since it depends on the ProtoValidator
  // implementation which is tested in the corresponding unit test.
  EXPECT_THAT(dpf_->EvaluateNext<absl::uint128>({}, ctx),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(IncrementalDpfTest, EvaluationFailsIfHierarchyLevelNegative) {
  if (parameters_[0].value_type().integer().bitsize() != 128) {
    return;
  }
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  EXPECT_THAT(dpf_->EvaluateUntil<absl::uint128>(-1, {}, ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`hierarchy_level` must be non-negative and less than "
                       "parameters_.size()"));
}

TEST_P(IncrementalDpfTest, EvaluationFailsIfHierarchyLevelTooLarge) {
  if (parameters_[0].value_type().integer().bitsize() != 128) {
    return;
  }
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  EXPECT_THAT(dpf_->EvaluateUntil<absl::uint128>(parameters_.size(), {}, ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`hierarchy_level` must be non-negative and less than "
                       "parameters_.size()"));
}

TEST_P(IncrementalDpfTest, EvaluationFailsIfValueTypeDoesntMatch) {
  using SomeStrangeType = Tuple<uint8_t, uint32_t, uint8_t, uint16_t, uint8_t>;
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  EXPECT_THAT(
      dpf_->EvaluateUntil<SomeStrangeType>(0, {}, ctx),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Value type T doesn't match parameters at `hierarchy_level`"));
}

TEST_P(IncrementalDpfTest, EvaluationFailsIfLevelAlreadyEvaluated) {
  if (parameters_.size() < 2 ||
      parameters_[0].value_type().integer().bitsize() != 128) {
    return;
  }
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  DPF_ASSERT_OK(dpf_->EvaluateUntil<absl::uint128>(0, {}, ctx));

  EXPECT_THAT(dpf_->EvaluateUntil<absl::uint128>(0, {}, ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`hierarchy_level` must be greater than "
                       "`ctx.previous_hierarchy_level`"));
}

TEST_P(IncrementalDpfTest, EvaluationFailsIfPrefixesNotEmptyOnFirstCall) {
  if (parameters_[0].value_type().integer().bitsize() != 128) {
    return;
  }
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  EXPECT_THAT(
      dpf_->EvaluateUntil<absl::uint128>(0, {0}, ctx),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "`prefixes` must be empty if and only if this is the first call with "
          "`ctx`."));
}

TEST_P(IncrementalDpfTest, EvaluationFailsIfPrefixOutOfRange) {
  if (parameters_.size() < 2 ||
      parameters_[0].value_type().integer().bitsize() != 128 ||
      parameters_[1].value_type().integer().bitsize() != 128) {
    return;
  }
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx,
                           dpf_->CreateEvaluationContext(keys_.first));

  DPF_ASSERT_OK(dpf_->EvaluateUntil<absl::uint128>(0, {}, ctx));
  auto invalid_prefix = absl::uint128{1} << parameters_[0].log_domain_size();

  EXPECT_THAT(dpf_->EvaluateUntil<absl::uint128>(1, {invalid_prefix}, ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       StrFormat("Index %d out of range for hierarchy level 0",
                                 invalid_prefix)));
}

TEST_P(IncrementalDpfTest, TestCorrectness) {
  // Generate a random set of evaluation points. The library should be able to
  // handle duplicates, so fixing the size to 1000 works even for smaller
  // domains.
  absl::BitGen rng;
  absl::uniform_int_distribution<uint64_t> dist;
  const int kNumEvaluationPoints = 1000;
  std::vector<absl::uint128> evaluation_points(kNumEvaluationPoints);
  for (int i = 0; i < kNumEvaluationPoints - 1; ++i) {
    evaluation_points[i] = absl::MakeUint128(dist(rng), dist(rng));
    if (parameters_.back().log_domain_size() < 128) {
      evaluation_points[i] %= absl::uint128{1}
                              << parameters_.back().log_domain_size();
    }
  }
  evaluation_points.back() = alpha_;  // Always evaluate on alpha_.

  int num_levels = static_cast<int>(parameters_.size());
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx0,
                           dpf_->CreateEvaluationContext(keys_.first));
  DPF_ASSERT_OK_AND_ASSIGN(EvaluationContext ctx1,
                           dpf_->CreateEvaluationContext(keys_.second));

  for (int i = level_step_ - 1; i < num_levels; i += level_step_) {
    switch (parameters_[i].value_type().integer().bitsize()) {
      case 8:
        EvaluateAndCheckLevel<uint8_t>(i, evaluation_points, ctx0, ctx1);
        break;
      case 16:
        EvaluateAndCheckLevel<uint16_t>(i, evaluation_points, ctx0, ctx1);
        break;
      case 32:
        EvaluateAndCheckLevel<uint32_t>(i, evaluation_points, ctx0, ctx1);
        break;
      case 64:
        EvaluateAndCheckLevel<uint64_t>(i, evaluation_points, ctx0, ctx1);
        break;
      case 128:
        EvaluateAndCheckLevel<absl::uint128>(i, evaluation_points, ctx0, ctx1);
        break;
      default:
        ASSERT_TRUE(0) << "Unsupported element_bitsize";
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    OneHierarchyLevelVaryElementSizes, IncrementalDpfTest,
    testing::Combine(
        // DPF parameters.
        testing::Values(
            // Vary element sizes, small domain size.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 16}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 32}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 128}},
            // Vary element sizes, medium domain size.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 10, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 10, .element_bitsize = 16}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 10, .element_bitsize = 32}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 10, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 10, .element_bitsize = 128}}),
        testing::Values(0, 1, 15),  // alpha
        testing::Values(std::vector<absl::uint128>(1, 1),
                        std::vector<absl::uint128>(1, 100),
                        std::vector<absl::uint128>(1, 255)),  // beta
        testing::Values(1),                                   // level_step
        testing::Values(false, true)                          // single_point
        ));

INSTANTIATE_TEST_SUITE_P(
    OneHierarchyLevelVaryDomainSizes, IncrementalDpfTest,
    testing::Combine(
        // DPF parameters.
        testing::Values(
            // Vary domain sizes, small element size.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 1, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 2, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 3, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 6, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 7, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 8, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 9, .element_bitsize = 8}},
            // Vary domain sizes, medium element size.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 1, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 2, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 3, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 6, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 7, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 8, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 9, .element_bitsize = 64}},
            // Vary domain sizes, large element size.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 1, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 2, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 3, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 6, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 7, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 8, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 9, .element_bitsize = 128}}),
        testing::Values(0),  // alpha
        testing::Values(std::vector<absl::uint128>(1, 1),
                        std::vector<absl::uint128>(1, 100),
                        std::vector<absl::uint128>(1, 255)),  // beta
        testing::Values(1),                                   // level_step
        testing::Values(false, true)                          // single_point
        ));

INSTANTIATE_TEST_SUITE_P(
    TwoHierarchyLevels, IncrementalDpfTest,
    testing::Combine(
        // DPF parameters.
        testing::Values(
            // Equal element sizes.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 8},
                {.log_domain_size = 10, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 16},
                {.log_domain_size = 10, .element_bitsize = 16}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 32},
                {.log_domain_size = 10, .element_bitsize = 32}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 64},
                {.log_domain_size = 10, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 128},
                {.log_domain_size = 10, .element_bitsize = 128}},
            // First correction is in seed word.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 8},
                {.log_domain_size = 10, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 16},
                {.log_domain_size = 10, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 32},
                {.log_domain_size = 10, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 64},
                {.log_domain_size = 10, .element_bitsize = 128}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 128},
                {.log_domain_size = 10, .element_bitsize = 128}}),
        testing::Values(0, 1, 2, 100, 1023),  // alpha
        testing::Values(std::vector<absl::uint128>({1, 2}),
                        std::vector<absl::uint128>({80, 90}),
                        std::vector<absl::uint128>({255, 255})),  // beta
        testing::Values(1, 2),                                    // level_step
        testing::Values(false, true)  // single_point
        ));

INSTANTIATE_TEST_SUITE_P(
    ThreeHierarchyLevels, IncrementalDpfTest,
    testing::Combine(
        // DPF parameters.
        testing::Values<std::vector<DpfTestParameters>>(
            // Equal element sizes.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 8},
                {.log_domain_size = 10, .element_bitsize = 8},
                {.log_domain_size = 15, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 16},
                {.log_domain_size = 10, .element_bitsize = 16},
                {.log_domain_size = 15, .element_bitsize = 16}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 32},
                {.log_domain_size = 10, .element_bitsize = 32},
                {.log_domain_size = 15, .element_bitsize = 32}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 64},
                {.log_domain_size = 10, .element_bitsize = 64},
                {.log_domain_size = 15, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 128},
                {.log_domain_size = 10, .element_bitsize = 128},
                {.log_domain_size = 15, .element_bitsize = 128}},
            // Varying element sizes
            std::vector<DpfTestParameters>{
                {.log_domain_size = 5, .element_bitsize = 8},
                {.log_domain_size = 10, .element_bitsize = 16},
                {.log_domain_size = 15, .element_bitsize = 32}},
            // Small level distances.
            std::vector<DpfTestParameters>{
                {.log_domain_size = 4, .element_bitsize = 8},
                {.log_domain_size = 5, .element_bitsize = 8},
                {.log_domain_size = 6, .element_bitsize = 8}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 3, .element_bitsize = 16},
                {.log_domain_size = 4, .element_bitsize = 16},
                {.log_domain_size = 5, .element_bitsize = 16}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 2, .element_bitsize = 32},
                {.log_domain_size = 3, .element_bitsize = 32},
                {.log_domain_size = 4, .element_bitsize = 32}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 1, .element_bitsize = 64},
                {.log_domain_size = 2, .element_bitsize = 64},
                {.log_domain_size = 3, .element_bitsize = 64}},
            std::vector<DpfTestParameters>{
                {.log_domain_size = 0, .element_bitsize = 128},
                {.log_domain_size = 1, .element_bitsize = 128},
                {.log_domain_size = 2, .element_bitsize = 128}}),
        testing::Values(0, 1),                                   // alpha
        testing::Values(std::vector<absl::uint128>({1, 2, 3})),  // beta
        testing::Values(1, 2),                                   // level_step
        testing::Values(false, true)                             // single_point
        ));

INSTANTIATE_TEST_SUITE_P(
    MaximumOutputDomainSize, IncrementalDpfTest,
    testing::Combine(
        // DPF parameters. We want to be able to evaluate at every bit, so this
        // lambda returns a vector with 129 parameters with log domain sizes
        // 0...128.
        testing::Values([]() -> std::vector<DpfTestParameters> {
          std::vector<DpfTestParameters> parameters(129);
          for (int i = 0; i < static_cast<int>(parameters.size()); ++i) {
            parameters[i].log_domain_size = i;
            parameters[i].element_bitsize = 64;
          }
          return parameters;
        }()),
        testing::Values(absl::MakeUint128(23, 42)),                 // alpha
        testing::Values(std::vector<absl::uint128>(129, 1234567)),  // beta
        testing::Values(1, 2, 3, 5, 7),  // level_step
        testing::Values(false, true)     // single_point
        ));

template <typename T>
class DpfEvaluationTest : public ::testing::Test {
 protected:
  void SetUp() { SetUp(10, 23); }
  void SetUp(int log_domain_size, absl::uint128 alpha) {
    return SetUp(absl::MakeConstSpan(&log_domain_size, 1), alpha);
  }
  void SetUp(absl::Span<const int> log_domain_size, absl::uint128 alpha) {
    log_domain_size_.resize(log_domain_size.size());
    absl::c_copy(log_domain_size, log_domain_size_.begin());
    alpha_ = alpha;
    beta_.resize(log_domain_size.size());
    for (T& beta : beta_) {
      SetTo42(beta);
    }
    parameters_.resize(log_domain_size.size());
    for (int i = 0; i < parameters_.size(); ++i) {
      parameters_[i].set_log_domain_size(log_domain_size_[i]);
      parameters_[i].set_security_parameter(48);
      *(parameters_[i].mutable_value_type()) = ToValueType<T>();
    }
    DPF_ASSERT_OK_AND_ASSIGN(
        dpf_, DistributedPointFunction::CreateIncremental(parameters_));
    DPF_ASSERT_OK(this->dpf_->template RegisterValueType<T>());
    DPF_ASSERT_OK_AND_ASSIGN(
        keys_, this->dpf_->GenerateKeysIncremental(
                   this->alpha_, absl::MakeConstSpan(this->beta_)));
  }

  // Helper function that recursively sets all elements of a tuple to 42.
  template <typename T0>
  static void SetTo42(T0& x) {
    x = T0(42);
  }
  template <typename T0, typename... Tn>
  static void SetTo42(T0& x0, Tn&... xn) {
    SetTo42(x0);
    SetTo42(xn...);
  }
  template <typename... Tn>
  static void SetTo42(Tuple<Tn...>& x) {
    absl::apply([](auto&... in) { SetTo42(in...); }, x.value());
  }

  std::vector<int> log_domain_size_;
  absl::uint128 alpha_;
  std::vector<T> beta_;
  std::vector<DpfParameters> parameters_;
  std::unique_ptr<DistributedPointFunction> dpf_;
  std::pair<DpfKey, DpfKey> keys_;
};

using MyIntModN = IntModN<uint32_t, 4294967291u>;                // 2**32 - 5.
using MyIntModN64 = IntModN<uint64_t, 18446744073709551557ull>;  // 2**64 - 59.
#ifdef ABSL_HAVE_INTRINSIC_INT128
using MyIntModN128 =
    IntModN<absl::uint128, (unsigned __int128)(absl::MakeUint128(
                               65535u, 18446744073709551551ull))>;  // 2**80-65
#endif
using DpfEvaluationTypes = ::testing::Types<
    // Integers
    uint8_t, uint32_t, uint64_t, absl::uint128,
    // Tuple
    Tuple<uint8_t>, Tuple<uint32_t>, Tuple<absl::uint128>,
    Tuple<uint32_t, uint32_t>, Tuple<uint32_t, uint64_t>,
    Tuple<uint64_t, uint64_t>, Tuple<uint8_t, uint16_t, uint32_t, uint64_t>,
    Tuple<uint32_t, uint32_t, uint32_t, uint32_t>,
    Tuple<uint32_t, Tuple<uint32_t, uint32_t>, uint32_t>,
    Tuple<uint32_t, absl::uint128>,
    // IntModN
    MyIntModN, Tuple<MyIntModN>, Tuple<uint32_t, MyIntModN>,
    Tuple<absl::uint128, MyIntModN>, Tuple<MyIntModN, Tuple<MyIntModN>>,
    Tuple<MyIntModN, MyIntModN, MyIntModN, MyIntModN, MyIntModN>,
    Tuple<MyIntModN64, MyIntModN64>
#ifdef ABSL_HAVE_INTRINSIC_INT128
    ,
    Tuple<MyIntModN128, MyIntModN128>,
#endif
    // XorWrapper
    XorWrapper<uint8_t>, XorWrapper<absl::uint128>,
    Tuple<XorWrapper<uint32_t>, absl::uint128>>;
TYPED_TEST_SUITE(DpfEvaluationTest, DpfEvaluationTypes);

TYPED_TEST(DpfEvaluationTest, TestRegularDpf) {
  int log_domain_size = 10;
  absl::uint128 alpha = 23;
  this->SetUp(log_domain_size, alpha);
  DPF_ASSERT_OK_AND_ASSIGN(
      EvaluationContext ctx_1,
      this->dpf_->CreateEvaluationContext(this->keys_.first));
  DPF_ASSERT_OK_AND_ASSIGN(
      EvaluationContext ctx_2,
      this->dpf_->CreateEvaluationContext(this->keys_.second));
  DPF_ASSERT_OK_AND_ASSIGN(
      std::vector<TypeParam> output_1,
      this->dpf_->template EvaluateNext<TypeParam>({}, ctx_1));
  DPF_ASSERT_OK_AND_ASSIGN(
      std::vector<TypeParam> output_2,
      this->dpf_->template EvaluateNext<TypeParam>({}, ctx_2));

  EXPECT_EQ(output_1.size(), 1 << log_domain_size);
  EXPECT_EQ(output_2.size(), 1 << log_domain_size);
  for (int i = 0; i < (1 << log_domain_size); ++i) {
    TypeParam sum = output_1[i] + output_2[i];
    if (i == this->alpha_) {
      EXPECT_EQ(sum, this->beta_[0]);
    } else {
      EXPECT_EQ(sum, TypeParam{});
    }
  }
}

TYPED_TEST(DpfEvaluationTest, TestBatchSinglePointEvaluation) {
  // Set Up with a large output domain, to make sure this works.
  for (int log_domain_size : {0, 1, 2, 32, 128}) {
    absl::uint128 max_evaluation_point = absl::Uint128Max();
    if (log_domain_size < 128) {
      max_evaluation_point = (absl::uint128{1} << log_domain_size) - 1;
    }
    const absl::uint128 alpha = 23 & max_evaluation_point;
    this->SetUp(log_domain_size, alpha);
    for (int num_evaluation_points : {0, 1, 2, 100, 1000}) {
      std::vector<absl::uint128> evaluation_points(num_evaluation_points);
      for (int i = 0; i < num_evaluation_points; ++i) {
        evaluation_points[i] = i & max_evaluation_point;
      }
      DPF_ASSERT_OK_AND_ASSIGN(std::vector<TypeParam> output_1,
                               this->dpf_->template EvaluateAt<TypeParam>(
                                   this->keys_.first, 0, evaluation_points));
      DPF_ASSERT_OK_AND_ASSIGN(std::vector<TypeParam> output_2,
                               this->dpf_->template EvaluateAt<TypeParam>(
                                   this->keys_.second, 0, evaluation_points));
      ASSERT_EQ(output_1.size(), output_2.size());
      ASSERT_EQ(output_1.size(), num_evaluation_points);

      for (int i = 0; i < num_evaluation_points; ++i) {
        TypeParam sum = output_1[i] + output_2[i];
        if (evaluation_points[i] == this->alpha_) {
          EXPECT_EQ(sum, this->beta_[0])
              << "i=" << i << ", log_domain_size=" << log_domain_size;
        } else {
          EXPECT_EQ(sum, TypeParam{})
              << "i=" << i << ", log_domain_size=" << log_domain_size;
        }
      }
    }
  }
}

TYPED_TEST(DpfEvaluationTest, TestEvaluateAndApplySimpleAddition) {
  std::vector<std::vector<int>> parameters = {
      {0, 1, 2}, {8, 16, 32, 64}, {0, 128}, {128}, {/* filled below */}};
  for (int i = 0; i <= 128; ++i) {
    parameters.back().push_back(i);
  }
  for (const auto& log_domain_sizes : parameters) {
    absl::uint128 max_domain_element = absl::Uint128Max();
    if (log_domain_sizes.back() < 128) {
      max_domain_element = (absl::uint128{1} << log_domain_sizes.back()) - 1;
    }
    absl::uint128 alpha = max_domain_element;
    this->SetUp(log_domain_sizes, alpha);

    std::vector<absl::uint128> evaluation_points = {23, 42, 123, 0,
                                                    absl::Uint128Max()};
    for (auto& point : evaluation_points) {
      point &= max_domain_element;
    }
    std::vector<const DpfKey*> keys = {
        &(this->keys_.first), &(this->keys_.second), &(this->keys_.first),
        &(this->keys_.second), &(this->keys_.first)};
    int num_levels = log_domain_sizes.size();
    int num_keys = keys.size();

    std::vector<TypeParam> sum(num_keys, TypeParam{});
    int count = 0;
    auto fn = [&sum, &count](absl::Span<const TypeParam> values) {
      for (int i = 0; i < values.size(); ++i) {
        sum[i] += values[i];
      }
      ++count;
      return true;
    };

    // Run evaluation level-by-level to compute the expected sum.
    std::vector<TypeParam> expected(num_keys, TypeParam{});
    for (int hierarchy_level = 0; hierarchy_level < num_levels;
         ++hierarchy_level) {
      const int shift_amount =
          (log_domain_sizes.back() - log_domain_sizes[hierarchy_level]);
      for (int i = 0; i < num_keys; ++i) {
        absl::uint128 prefix = 0;
        if (shift_amount < 128) {
          prefix = evaluation_points[i] >> shift_amount;
        }
        DPF_ASSERT_OK_AND_ASSIGN(
            auto result,
            this->dpf_->template EvaluateAt<TypeParam>(
                *keys[i], hierarchy_level, absl::MakeConstSpan(&prefix, 1)));
        expected[i] += result[0];
      }
    }

    EXPECT_THAT(this->dpf_->template EvaluateAndApply<TypeParam>(
                    keys, evaluation_points, fn),
                IsOk());
    EXPECT_EQ(sum, expected)
        << "log_domain_sizes=" << absl::StrJoin(log_domain_sizes, " ");
    EXPECT_EQ(count, num_levels);
  }
}

TYPED_TEST(DpfEvaluationTest,
           EvaluateAndApplyFailsWithTooManyEvaluationPoints) {
  std::vector<absl::uint128> evaluation_points = {0, 1};

  EXPECT_THAT(
      this->dpf_->template EvaluateAndApply<TypeParam>(
          absl::MakeConstSpan(&(this->keys_.first), 1), evaluation_points,
          [](absl::Span<const TypeParam>) { return true; }),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("evaluation_points")));
}

TYPED_TEST(DpfEvaluationTest, EvaluateAndApplyFailsWithInvalidKey) {
  DpfKey key;

  EXPECT_THAT(this->dpf_->template EvaluateAndApply<TypeParam>(
                  absl::MakeConstSpan(&key, 1), {0},
                  [](absl::Span<const TypeParam>) { return true; }),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("key")));
}

}  // namespace
}  // namespace distributed_point_functions

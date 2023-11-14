// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/distributed_point_functions/code/dpf/distributed_point_function.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <memory>

#define DPF_FUZZER_ASSERT(x)                                         \
  if (!(x)) {                                                        \
    printf("DPF assertion failed: function %s, file %s, line %d.\n", \
           __PRETTY_FUNCTION__, __FILE__, __LINE__);                 \
    abort();                                                         \
  }

namespace {

const size_t UINT128_SIZE = 2 * sizeof(uint64_t);

// Constructs a `uint128` numeric value from two 64-bit unsigned integers
// consumed from the data provider.
absl::uint128 ConsumeUint128(FuzzedDataProvider& data_provider) {
  uint64_t high = data_provider.ConsumeIntegral<uint64_t>();
  uint64_t low = data_provider.ConsumeIntegral<uint64_t>();
  return absl::MakeUint128(high, low);
}

// Returns the prefix of `index` for the domain of `hierarchy_level`.
// Adapted from `DpfEvaluationTest::GetPrefixForLevel()`.
absl::uint128 GetPrefixForLevel(
    int hierarchy_level,
    absl::uint128 index,
    const std::vector<distributed_point_functions::DpfParameters>& parameters) {
  absl::uint128 result = 0;
  int shift_amount = parameters.back().log_domain_size() -
                     parameters[hierarchy_level].log_domain_size();
  if (shift_amount < 128)
    result = index >> shift_amount;
  return result;
}

// Evaluates both contexts `ctx0` and `ctx1` at `hierarchy level`, using the
// appropriate prefixes of `evaluation_points`. Checks that the expansion of
// both keys from correct DPF shares, i.e., they add up to
// `beta[ctx.hierarchy_level()]` under prefixes of `alpha`, and to 0 otherwise.
// Adapted from `DpfEvaluationTest::EvaluateAndCheckLevel()`.
template <typename T>
void EvaluateAndCheckLevel(
    int hierarchy_level,
    absl::Span<const absl::uint128> evaluation_points,
    absl::uint128 alpha,
    const std::vector<absl::uint128>& beta,
    distributed_point_functions::EvaluationContext& ctx0,
    distributed_point_functions::EvaluationContext& ctx1,
    const std::vector<distributed_point_functions::DpfParameters>& parameters,
    const distributed_point_functions::DistributedPointFunction& dpf) {
  int previous_hierarchy_level = ctx0.previous_hierarchy_level();
  int current_log_domain_size = parameters[hierarchy_level].log_domain_size();
  int previous_log_domain_size = 0;
  int num_expansions = 1;
  bool is_first_evaluation = previous_hierarchy_level < 0;
  // Generate prefixes if we're not on the first level.
  std::vector<absl::uint128> prefixes;
  if (!is_first_evaluation) {
    num_expansions = static_cast<int>(evaluation_points.size());
    prefixes.resize(evaluation_points.size());
    previous_log_domain_size =
        parameters[previous_hierarchy_level].log_domain_size();
    for (int i = 0; i < static_cast<int>(evaluation_points.size()); ++i)
      prefixes[i] = GetPrefixForLevel(previous_hierarchy_level,
                                      evaluation_points[i], parameters);
  }

  // Evaluating a key with N correction words leads to an O(2^N) malloc, which
  // will unsurprisingly cause a fuzzer crash. See <https://crbug.com/1494260>.
  constexpr size_t kMaxCorrectionWords = 30;
  if (ctx0.key().correction_words().size() > kMaxCorrectionWords) {
    return;
  }
  absl::StatusOr<std::vector<T>> result_0 =
      dpf.EvaluateUntil<T>(hierarchy_level, prefixes, ctx0);
  DPF_FUZZER_ASSERT(result_0.ok());
  if (ctx1.key().correction_words().size() > kMaxCorrectionWords) {
    return;
  }
  absl::StatusOr<std::vector<T>> result_1 =
      dpf.EvaluateUntil<T>(hierarchy_level, prefixes, ctx1);
  DPF_FUZZER_ASSERT(result_1.ok());

  DPF_FUZZER_ASSERT(result_0->size() == result_1->size());
  int64_t outputs_per_prefix =
      int64_t{1} << (current_log_domain_size - previous_log_domain_size);
  int64_t expected_output_size = num_expansions * outputs_per_prefix;
  DPF_FUZZER_ASSERT(static_cast<int64_t>(result_0->size()) ==
                    expected_output_size);

  // Iterator over the outputs and check that they sum up to 0 or to
  // `beta[current_hierarchy_level]`;
  absl::uint128 previous_alpha_prefix = 0;
  if (!is_first_evaluation)
    previous_alpha_prefix =
        GetPrefixForLevel(previous_hierarchy_level, alpha, parameters);

  absl::uint128 current_alpha_prefix =
      GetPrefixForLevel(hierarchy_level, alpha, parameters);
  for (int64_t i = 0; i < expected_output_size; ++i) {
    int prefix_index = i / outputs_per_prefix;
    int prefix_expansion_index = i % outputs_per_prefix;
    // The output is on the path to `alpha`, if we're at the first level or
    // under a prefix of `alpha`, and the current block in the expansion of the
    // prefix is also on the path to `alpha`.
    if ((is_first_evaluation ||
         prefixes[prefix_index] == previous_alpha_prefix) &&
        prefix_expansion_index == (current_alpha_prefix % outputs_per_prefix)) {
      // We need to static_cast here since otherwise operator+ returns an
      // unsigned int without doing a modular reduction, which causes the test
      // to fail on types with sizeof(T) < sizeof(unsigned).
      DPF_FUZZER_ASSERT(
          absl::uint128{static_cast<T>((*result_0)[i] + (*result_1)[i])} ==
          beta[hierarchy_level]);
    } else {
      DPF_FUZZER_ASSERT(static_cast<T>((*result_0)[i] + (*result_1)[i]) == 0U);
    }
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Use magic separator to split the input into two parts. The first part will
  // generate alpha, and an array of parameters and betas. The second part will
  // generate level step and an array of evaluation points.
  const uint8_t separator[] = {0xDE, 0xAD, 0xBE, 0xEF};

  const uint8_t* pos =
      std::search(data, data + size, separator, separator + sizeof(separator));

  const uint8_t* data1 = data;
  size_t size1 = pos - data;

  const uint8_t* data2 =
      (pos == data + size) ? nullptr : pos + sizeof(separator);
  size_t size2 = data2 ? (data + size) - (pos + sizeof(separator)) : 0;

  FuzzedDataProvider data_provider1(data1, size1);

  if (data_provider1.remaining_bytes() < UINT128_SIZE)
    return 0;

  absl::uint128 alpha = ConsumeUint128(data_provider1);

  std::vector<int32_t> log_domain_sizes;
  std::vector<int32_t> element_bitsizes;
  std::vector<distributed_point_functions::DpfParameters> parameters;
  std::vector<absl::uint128> beta;

  // log_domain_size(int32_t), element_bitsize(int32_t),
  // beta(uint128)
  while (data_provider1.remaining_bytes() >=
         (2 * sizeof(int32_t) + UINT128_SIZE)) {
    int32_t log_domain_size = data_provider1.ConsumeIntegral<int32_t>();
    int32_t element_bitsize = data_provider1.ConsumeIntegral<int32_t>();
    log_domain_sizes.push_back(log_domain_size);
    element_bitsizes.push_back(element_bitsize);

    distributed_point_functions::DpfParameters parameter;
    parameter.set_log_domain_size(log_domain_size);
    parameter.mutable_value_type()->mutable_integer()->set_bitsize(
        element_bitsize);
    parameters.push_back(parameter);

    beta.push_back(ConsumeUint128(data_provider1));
  }

  absl::StatusOr<
      std::unique_ptr<distributed_point_functions::DistributedPointFunction>>
      status_or_dpf = distributed_point_functions::DistributedPointFunction::
          CreateIncremental(parameters);

  size_t num_levels = parameters.size();

  if (!status_or_dpf.ok()) {
    // `log_domain_size` is expected to be in ascending order and
    // `element_bitsize` is expected to be non-decreasing. As it is hard for the
    // fuzzer to land upon a valid input, we sort the parameters and try again
    // if the construction fails.
    std::sort(log_domain_sizes.begin(), log_domain_sizes.end());
    std::sort(element_bitsizes.begin(), element_bitsizes.end());
    for (size_t i = 0; i < num_levels; ++i) {
      parameters[i].set_log_domain_size(log_domain_sizes[i]);
      parameters[i].mutable_value_type()->mutable_integer()->set_bitsize(
          element_bitsizes[i]);
    }

    status_or_dpf = distributed_point_functions::DistributedPointFunction::
        CreateIncremental(parameters);
  }

  if (!status_or_dpf.ok())
    return 0;

  std::unique_ptr<distributed_point_functions::DistributedPointFunction> dpf =
      std::move(status_or_dpf).value();

  absl::StatusOr<std::pair<distributed_point_functions::DpfKey,
                           distributed_point_functions::DpfKey>>
      status_or_keys = dpf->GenerateKeysIncremental(alpha, beta);
  if (!status_or_keys.ok())
    return 0;

  std::pair<distributed_point_functions::DpfKey,
            distributed_point_functions::DpfKey>
      keys = std::move(status_or_keys).value();

  // Adapted from `DpfEvaluationTest.TestCorrectness()`.
  absl::StatusOr<distributed_point_functions::EvaluationContext>
      status_or_ctx0 = dpf->CreateEvaluationContext(keys.first);
  DPF_FUZZER_ASSERT(status_or_ctx0.ok());

  absl::StatusOr<distributed_point_functions::EvaluationContext>
      status_or_ctx1 = dpf->CreateEvaluationContext(keys.second);
  DPF_FUZZER_ASSERT(status_or_ctx1.ok());

  distributed_point_functions::EvaluationContext ctx0 =
      std::move(status_or_ctx0).value();
  distributed_point_functions::EvaluationContext ctx1 =
      std::move(status_or_ctx1).value();

  // Generate evaluation points.
  FuzzedDataProvider data_provider2(data2, size2);
  if (data_provider2.remaining_bytes() < sizeof(int))
    return 0;

  int level_step = data_provider2.ConsumeIntegralInRange<int>(1, 10);

  std::vector<absl::uint128> evaluation_points;
  while (data_provider2.remaining_bytes() >= UINT128_SIZE) {
    evaluation_points.push_back(ConsumeUint128(data_provider2));
    if (parameters.back().log_domain_size() < 128)
      evaluation_points.back() %=
          (absl::uint128{1} << parameters.back().log_domain_size());
  }

  // Always evaluate on alpha.
  evaluation_points.push_back(alpha);

  int32_t previous_log_domain_size = 0;
  for (int i = level_step - 1; i < static_cast<int>(num_levels);
       i += level_step) {
    // If any gap in the log_domain_sizes used in successive evaluations is
    // larger than 62, validation will fail in `EvaluateAndCheckLevel`.
    int32_t current_log_domain_size = parameters[i].log_domain_size();
    if (current_log_domain_size - previous_log_domain_size > 62)
      return 0;
    previous_log_domain_size = current_log_domain_size;

    switch (parameters[i].value_type().integer().bitsize()) {
      case 8:
        EvaluateAndCheckLevel<uint8_t>(i, evaluation_points, alpha, beta, ctx0,
                                       ctx1, parameters, *dpf);
        break;
      case 16:
        EvaluateAndCheckLevel<uint16_t>(i, evaluation_points, alpha, beta, ctx0,
                                        ctx1, parameters, *dpf);
        break;
      case 32:
        EvaluateAndCheckLevel<uint32_t>(i, evaluation_points, alpha, beta, ctx0,
                                        ctx1, parameters, *dpf);
        break;
      case 64:
        EvaluateAndCheckLevel<uint64_t>(i, evaluation_points, alpha, beta, ctx0,
                                        ctx1, parameters, *dpf);
        break;
      case 128:
        EvaluateAndCheckLevel<absl::uint128>(i, evaluation_points, alpha, beta,
                                             ctx0, ctx1, parameters, *dpf);
        break;
      default:
        // DPF construction should've failed if the parameters were invalid.
        DPF_FUZZER_ASSERT(false);
        break;
    }
  }

  return 0;
}

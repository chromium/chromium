/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dpf/aes_128_fixed_key_hash.h"

#include "dpf/internal/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace distributed_point_functions {
namespace {

using dpf_internal::StatusIs;

// Test blocks for keys, inputs, and outputs.
constexpr absl::uint128 kKey0 =
    absl::MakeUint128(0x0000000000000000, 0x0000000000000000);
constexpr absl::uint128 kKey1 =
    absl::MakeUint128(0x1111111111111111, 0x1111111111111111);
constexpr absl::uint128 kSeed0 =
    absl::MakeUint128(0x0123012301230123, 0x0123012301230123);
constexpr absl::uint128 kSeed1 =
    absl::MakeUint128(0x4567456745674567, 0x4567456745674567);
constexpr absl::uint128 kSeed2 =
    absl::MakeUint128(0x89ab89ab89ab89ab, 0x89ab89ab89ab89ab);
constexpr absl::uint128 kSeed3 =
    absl::MakeUint128(0xcdefcdefcdefcdef, 0xcdefcdefcdefcdef);

TEST(PseudorandomGeneratorTest, CreateSucceeds) {
  DPF_EXPECT_OK(Aes128FixedKeyHash::Create(kKey0));
}

TEST(PseudorandomGeneratorTest, SameKeysAndSeedsGenerateSameOutput) {
  std::vector<absl::uint128> in;

  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg_0,
                           Aes128FixedKeyHash::Create(kKey0));
  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg_1,
                           Aes128FixedKeyHash::Create(kKey0));
  in = {kSeed0};
  // Initialize output arrays with different values, to make sure they are the
  // same afterwards.
  std::vector<absl::uint128> out_0(in.size(), kSeed2), out_1(in.size(), kSeed3);

  DPF_EXPECT_OK(prg_0.Evaluate(in, absl::MakeSpan(out_0)));
  DPF_EXPECT_OK(prg_1.Evaluate(in, absl::MakeSpan(out_1)));
  EXPECT_THAT(out_0, testing::ElementsAreArray(out_1));
}

TEST(PseudorandomGeneratorTest, DifferentKeysGenerateDifferentOutput) {
  std::vector<absl::uint128> in{kSeed0};

  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg_0,
                           Aes128FixedKeyHash::Create(kKey0));
  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg_1,
                           Aes128FixedKeyHash::Create(kKey1));
  // Initialize output arrays with the same values, to make sure they are
  // different afterwards.
  std::vector<absl::uint128> out_0(in.size(), kSeed2), out_1(in.size(), kSeed2);

  DPF_EXPECT_OK(prg_0.Evaluate(in, absl::MakeSpan(out_0)));
  DPF_EXPECT_OK(prg_1.Evaluate(in, absl::MakeSpan(out_1)));
  EXPECT_THAT(out_0, testing::Not(testing::ElementsAreArray(out_1)));
}

TEST(PseudorandomGeneratorTest, DifferentSeedsGenerateDifferentOutput) {
  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg,
                           Aes128FixedKeyHash::Create(kKey0));
  std::vector<absl::uint128> in_0, in_1;

  in_0 = {kSeed0};
  in_1 = {kSeed1};
  // Initialize output arrays with the same values, to make sure they are
  // different afterwards.
  std::vector<absl::uint128> out_0(in_0.size(), kSeed2),
      out_1(in_1.size(), kSeed2);

  DPF_EXPECT_OK(prg.Evaluate(in_0, absl::MakeSpan(out_0)));
  DPF_EXPECT_OK(prg.Evaluate(in_1, absl::MakeSpan(out_1)));
  EXPECT_THAT(out_0, testing::Not(testing::ElementsAreArray(out_1)));
}

TEST(PseudorandomGeneratorTest, BatchedEvaluationEqualsBlockWiseEvaluation) {
  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg,
                           Aes128FixedKeyHash::Create(kKey0));
  std::vector<absl::uint128> in_0, in_1, in_2;

  in_0 = {kSeed0};
  in_1 = {kSeed1};
  in_2 = {kSeed0, kSeed1};
  std::vector<absl::uint128> out_0(in_0.size()), out_1(in_1.size()),
      out_2(in_2.size());

  DPF_EXPECT_OK(prg.Evaluate(in_0, absl::MakeSpan(out_0)));
  DPF_EXPECT_OK(prg.Evaluate(in_1, absl::MakeSpan(out_1)));
  DPF_EXPECT_OK(prg.Evaluate(in_2, absl::MakeSpan(out_2)));
  EXPECT_THAT(out_2, testing::ElementsAre(out_0[0], out_1[0]));
}

TEST(PseudorandomGeneratorTest, TestSpecificOutputValues) {
  std::vector<absl::uint128> in, out_0, out_1;

  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg_0,
                           Aes128FixedKeyHash::Create(kKey0));
  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg_1,
                           Aes128FixedKeyHash::Create(kKey1));
  in = {kSeed0, kSeed1};
  out_0.resize(in.size());
  out_1.resize(in.size());

  DPF_EXPECT_OK(prg_0.Evaluate(in, absl::MakeSpan(out_0)));
  DPF_EXPECT_OK(prg_1.Evaluate(in, absl::MakeSpan(out_1)));
  EXPECT_THAT(out_0,
              testing::ElementsAre(
                  absl::MakeUint128(0x73c2dc14812be4ef, 0xeac64d09c8adf8ed),
                  absl::MakeUint128(0xb8f33653a53a8436, 0xaedf39b62de91d95)));
  EXPECT_THAT(out_1,
              testing::ElementsAre(
                  absl::MakeUint128(0x934704aff58fa233, 0xd3c20d1b9cc18d8f),
                  absl::MakeUint128(0x530098817046d284, 0x43e61d3273a04f7c)));
}

TEST(PseudorandomGeneratorTest, EvaluateFailsWhenSizesDontMatch) {
  std::vector<absl::uint128> in{kSeed0};
  DPF_ASSERT_OK_AND_ASSIGN(Aes128FixedKeyHash prg,
                           Aes128FixedKeyHash::Create(kKey0));

  std::vector<absl::uint128> out(in.size() + 1);

  EXPECT_THAT(prg.Evaluate(in, absl::MakeSpan(out)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Input and output sizes don't match"));
}

}  // namespace
}  // namespace distributed_point_functions

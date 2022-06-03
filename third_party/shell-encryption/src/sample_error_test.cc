/*
 * Copyright 2018 Google LLC.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sample_error.h"

#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "context.h"
#include "montgomery.h"
#include "symmetric_encryption.h"
#include "testing/parameters.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

const int kTestingRounds = 10;
const std::vector<rlwe::Uint64> variances = {8, 15, 29, 50};

template <typename ModularInt>
class SampleErrorTest : public ::testing::Test {};
TYPED_TEST_SUITE(SampleErrorTest, rlwe::testing::ModularIntTypes);

TYPED_TEST(SampleErrorTest, CheckUpperBoundOnNoise) {
  using Int = typename TypeParam::Int;

  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(0);

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));

    for (auto variance : variances) {
      for (int i = 0; i < kTestingRounds; i++) {
        ASSERT_OK_AND_ASSIGN(std::vector<TypeParam> error,
                             rlwe::SampleFromErrorDistribution<TypeParam>(
                                 context->GetN(), variance, prng.get(),
                                 context->GetModulusParams()));
        // Check that each coefficient is in [-2*variance, 2*variance]
        for (int j = 0; j < context->GetN(); j++) {
          Int reduced = error[j].ExportInt(context->GetModulusParams());
          if (reduced > (context->GetModulus() >> 1)) {
            EXPECT_LT(context->GetModulus() - reduced, 2 * variance + 1);
          } else {
            EXPECT_LT(reduced, 2 * variance + 1);
          }
        }
      }
    }
  }
}

TYPED_TEST(SampleErrorTest, FailOnTooLargeVariance) {
  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(0);
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));

    rlwe::Uint64 variance = rlwe::kMaxVariance + 1;
    EXPECT_THAT(
        rlwe::SampleFromErrorDistribution<TypeParam>(
            context->GetN(), variance, prng.get(), context->GetModulusParams()),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            HasSubstr(absl::StrCat("The variance, ", variance,
                                   ", must be at most ", rlwe::kMaxVariance))));
  }
}

}  // namespace

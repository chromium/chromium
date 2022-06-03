/*
 * Copyright 2020 Google LLC.
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

#include "context.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/numeric/int128.h"
#include "constants.h"
#include "integral_types.h"
#include "montgomery.h"
#include "status_macros.h"
#include "testing/parameters.h"
#include "testing/status_testing.h"

namespace {

template <typename ModularInt>
class ContextTest : public ::testing::Test {};
TYPED_TEST_SUITE(ContextTest, rlwe::testing::ModularIntTypes);

TYPED_TEST(ContextTest, CreateWorks) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
  }
}

TYPED_TEST(ContextTest, ParametersMatch) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));

    ASSERT_EQ(context->GetLogN(), params.log_n);
    ASSERT_EQ(context->GetN(), context->GetNttParams()->number_coeffs);
    ASSERT_EQ(context->GetLogT(), params.log_t);
    ASSERT_EQ(context->GetModulus(), params.modulus);
    ASSERT_EQ(context->GetModulus(), context->GetModulusParams()->modulus);
    ASSERT_EQ(context->GetVariance(), params.variance);
  }
}

}  // namespace

/*
 * Copyright 2017 Google LLC.
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

#include "ntt_parameters.h"

#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/numeric/int128.h"
#include "constants.h"
#include "montgomery.h"
#include "status_macros.h"
#include "testing/parameters.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

template <typename ModularInt>
class NttParametersTest : public testing::Test {};
TYPED_TEST_SUITE(NttParametersTest, rlwe::testing::ModularIntTypes);

TYPED_TEST(NttParametersTest, LogNumCoeffsTooLarge) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    // Do not create a context, since it creates NttParameters already. Instead,
    // create the modulus parameters manually.
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    int log_n = rlwe::kMaxLogNumCoeffs + 1;
    EXPECT_THAT(
        rlwe::InitializeNttParameters<TypeParam>(log_n, modulus_params.get()),
        StatusIs(
            ::absl::StatusCode::kInvalidArgument,
            HasSubstr(absl::StrCat("log_n, ", log_n, ", must be less than ",
                                   rlwe::kMaxLogNumCoeffs, "."))));

    log_n = (sizeof(typename TypeParam::Int) * 8) - 1;
    if (log_n <= rlwe::kMaxLogNumCoeffs) {
      EXPECT_THAT(
          rlwe::InitializeNttParameters<TypeParam>(log_n, modulus_params.get()),
          StatusIs(
              ::absl::StatusCode::kInvalidArgument,
              HasSubstr(absl::StrCat(
                  "log_n, ", log_n,
                  ", does not fit into underlying ModularInt::Int type."))));
    }
  }
}

TYPED_TEST(NttParametersTest, PrimitiveNthRootOfUnity) {
  unsigned int log_ns[] = {2u, 4u, 6u, 8u, 11u};
  unsigned int len = 5;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    // Do not create a context, since it creates NttParameters already. Instead,
    // create the modulus parameters manually.
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    for (unsigned int i = 0; i < len; i++) {
      ASSERT_OK_AND_ASSIGN(TypeParam w,
                           rlwe::internal::PrimitiveNthRootOfUnity<TypeParam>(
                               log_ns[i], modulus_params.get()));
      unsigned int n = 1 << log_ns[i];

      // Ensure it is really a n-th root of unity.
      auto res = w.ModExp(n, modulus_params.get());
      auto one = TypeParam::ImportOne(modulus_params.get());
      EXPECT_EQ(res, one) << "Not an n-th root of unity.";

      // Ensure it is really a primitive n-th root of unity.
      auto res2 = w.ModExp(n / 2, modulus_params.get());
      EXPECT_NE(res2, one) << "Not a primitive n-th root of unity.";
    }
  }
}

TYPED_TEST(NttParametersTest, NttPsis) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    // Do not create a context, since it creates NttParameters already. Instead,
    // create the modulus parameters manually.
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    const size_t n = 1 << params.log_n;
    // The values of psi should be the powers of the primitive 2n-th root of
    // unity.
    // Obtain the psis.
    ASSERT_OK_AND_ASSIGN(
        std::vector<TypeParam> psis,
        rlwe::internal::NttPsis<TypeParam>(params.log_n, modulus_params.get()));

    // Verify that that the 0th entry is 1.
    TypeParam one = TypeParam::ImportOne(modulus_params.get());
    EXPECT_EQ(one, psis[0]);

    // Verify that the 1th entry is a primitive 2n-th root of unity.
    auto r1 = psis[1].ModExp(2 * n, modulus_params.get());
    auto r2 = psis[1].ModExp(n, modulus_params.get());
    EXPECT_EQ(one, r1);
    EXPECT_NE(one, r2);

    // Verify that each subsequent entry is the appropriate power of the 1th
    // entry.
    for (unsigned int i = 2; i < n; i++) {
      auto ri = psis[1].ModExp(i, modulus_params.get());
      EXPECT_EQ(psis[i], ri);
    }
  }
}

TYPED_TEST(NttParametersTest, NttPsisBitrev) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    // Do not create a context, since it creates NttParameters already. Instead,
    // create the modulus parameters manually.
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    const size_t n = 1 << params.log_n;

    // The values of psi should be bitreversed.
    // Target vector: obtain the psis in bitreversed order.
    ASSERT_OK_AND_ASSIGN(
        std::vector<TypeParam> psis_bitrev,
        rlwe::NttPsisBitrev<TypeParam>(params.log_n, modulus_params.get()));
    // Obtain the psis.
    ASSERT_OK_AND_ASSIGN(
        std::vector<TypeParam> psis,
        rlwe::internal::NttPsis<TypeParam>(params.log_n, modulus_params.get()));
    // Obtain the mapping for bitreversed order
    std::vector<unsigned int> bit_rev =
        rlwe::internal::BitrevArray(params.log_n);

    for (unsigned int i = 0; i < n; i++) {
      EXPECT_EQ(psis_bitrev[i], psis[bit_rev[i]]);
    }
  }
}

TYPED_TEST(NttParametersTest, NttPsisInvBitrev) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    // Do not create a context, since it creates NttParameters already. Instead,
    // create the modulus parameters manually.
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    const size_t n = 1 << params.log_n;

    // The values of the vectors should be psi^(-(brv[k]+1) for all k.
    // Target vector: obtain the psi inv in bit reversed order.
    ASSERT_OK_AND_ASSIGN(
        std::vector<TypeParam> psis_inv_bitrev,
        rlwe::NttPsisInvBitrev<TypeParam>(params.log_n, modulus_params.get()));
    // Obtain the psis.
    ASSERT_OK_AND_ASSIGN(
        std::vector<TypeParam> psis,
        rlwe::internal::NttPsis<TypeParam>(params.log_n, modulus_params.get()));
    // Obtain the mapping for bitreversed order
    std::vector<unsigned int> bit_rev =
        rlwe::internal::BitrevArray(params.log_n);

    for (unsigned int i = 0; i < n; i++) {
      EXPECT_EQ(modulus_params->One(),
                psis_inv_bitrev[i]
                    .Mul(psis[1], modulus_params.get())
                    .Mul(psis[bit_rev[i]], modulus_params.get())
                    .ExportInt(modulus_params.get()));
    }
  }
}

TEST(NttParametersRegularTest, Bitrev) {
  for (unsigned int log_N = 2; log_N < 11; log_N++) {
    unsigned int N = 1 << log_N;
    std::vector<unsigned int> bit_rev = rlwe::internal::BitrevArray(log_N);

    // Visit each entry of the array.
    for (unsigned int i = 0; i < N; i++) {
      for (unsigned int j = 0; j < log_N; j++) {
        // Ensure bit j of i is equal to bit (log_N - j) of bit_rev[i].
        rlwe::Uint64 mask1 = 1 << j;
        rlwe::Uint64 mask2 = 1 << (log_N - j - 1);
        EXPECT_EQ((i & mask1) == 0, (bit_rev[i] & mask2) == 0);
      }
    }
  }
}

TYPED_TEST(NttParametersTest, IncorrectNTTParams) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    // Do not create a context, since it creates NttParameters already. Instead,
    // create the modulus parameters manually.
    // modulus + 2, will no longer be 1 mod 2*n
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus + 2));

    EXPECT_THAT(
        rlwe::InitializeNttParameters<TypeParam>(params.log_n,
                                                 modulus_params.get()),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr(absl::StrCat("modulus is not 1 mod 2n for logn, ",
                                        params.log_n))));
  }
}

// Test all the NTT Parameter fields.
TYPED_TEST(NttParametersTest, Initialize) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    // Do not create a context, since it creates NttParameters already. Instead,
    // create the modulus parameters manually.
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    const size_t n = 1 << params.log_n;

    ASSERT_OK_AND_ASSIGN(rlwe::NttParameters<TypeParam> ntt_params,
                         rlwe::InitializeNttParameters<TypeParam>(
                             params.log_n, modulus_params.get()));

    TypeParam one = TypeParam::ImportOne(modulus_params.get());

    // Obtain the mapping for bitreversed order
    std::vector<unsigned int> bit_rev =
        rlwe::internal::BitrevArray(params.log_n);

    // Test first entry of psis in bitreversed order is one.
    EXPECT_EQ(one, ntt_params.psis_bitrev[0]);

    // Test n/2-th (brv[1]-th) entry of psis in bitreversed order is a primitive
    // 2n-th root of unity.
    auto psi = ntt_params.psis_bitrev[bit_rev[1]];
    auto r1 = psi.ModExp(2 * n, modulus_params.get());
    auto r2 = psi.ModExp(n, modulus_params.get());
    EXPECT_EQ(one, r1);
    EXPECT_NE(one, r2);

    // The values of psis should be the powers of the primitive 2n-th root of
    // unity in bitreversed order.
    for (unsigned int i = 0; i < n; i++) {
      auto bi = psi.ModExp(i, modulus_params.get());
      EXPECT_EQ(ntt_params.psis_bitrev[bit_rev[i]], bi);
    }

    // Test psis_inv_bitrev contains the inverses of the powers of psi in
    // bitreversed order, each multiplied by the inverse of psi.
    for (unsigned int i = 0; i < n; i++) {
      EXPECT_EQ(modulus_params->One(),
                ntt_params.psis_bitrev[i]
                    .Mul(psi, modulus_params.get())
                    .Mul(ntt_params.psis_inv_bitrev[i], modulus_params.get())
                    .ExportInt(modulus_params.get()));
    }
  }
}

}  // namespace

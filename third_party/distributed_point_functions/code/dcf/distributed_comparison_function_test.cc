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

#include "dcf/distributed_comparison_function.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/random/random.h"
#include "absl/utility/utility.h"
#include "dpf/internal/status_matchers.h"

namespace distributed_point_functions {

namespace {

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

TEST(DcfTest, CreateFailsWithZeroLogDomainSize) {
  DcfParameters parameters;
  parameters.mutable_parameters()
      ->mutable_value_type()
      ->mutable_integer()
      ->set_bitsize(32);

  parameters.mutable_parameters()->set_log_domain_size(0);

  EXPECT_THAT(DistributedComparisonFunction::Create(parameters),
              dpf_internal::StatusIs(absl::StatusCode::kInvalidArgument,
                                     "A DCF must have log_domain_size >= 1"));
}

template <typename T, int log_domain_size>
class DcfTestParameters {
 public:
  using ValueType = T;
  static constexpr int kLogDomainSize = log_domain_size;
};

template <typename T>
struct DcfTest : public testing::Test {
  void SetUp() {
    DcfParameters parameters;
    parameters.mutable_parameters()->set_log_domain_size(T::kLogDomainSize);
    *(parameters.mutable_parameters()->mutable_value_type()) =
        ToValueType<typename T::ValueType>();

    DPF_ASSERT_OK_AND_ASSIGN(dcf_,
                             DistributedComparisonFunction::Create(parameters));
  }

  std::unique_ptr<DistributedComparisonFunction> dcf_;
};

using MyIntModN = IntModN<uint32_t, 4294967291u>;  // 2**32 - 5.
using DcfTestTypes = ::testing::Types<
    DcfTestParameters<uint32_t, 1>, DcfTestParameters<uint32_t, 2>,
    DcfTestParameters<uint32_t, 5>, DcfTestParameters<absl::uint128, 5>,
    DcfTestParameters<Tuple<uint32_t, uint32_t>, 5>,
    DcfTestParameters<Tuple<uint32_t, absl::uint128>, 5>,
    DcfTestParameters<Tuple<MyIntModN, MyIntModN>, 5> >;

TYPED_TEST_SUITE(DcfTest, DcfTestTypes);

TYPED_TEST(DcfTest, CreateWorks) {
  EXPECT_THAT(this->dcf_, testing::Ne(nullptr));
}

TYPED_TEST(DcfTest, GenEval) {
  using ValueType = typename TypeParam::ValueType;
  const absl::uint128 domain_size = absl::uint128{1}
                                    << TypeParam::kLogDomainSize;
  ValueType beta;
  SetTo42(beta);
  for (absl::uint128 alpha = 0; alpha < domain_size; ++alpha) {
    // Generate keys.
    DcfKey key_0, key_1;
    DPF_ASSERT_OK_AND_ASSIGN(std::tie(key_0, key_1),
                             this->dcf_->GenerateKeys(alpha, beta));

    // Evaluate on every point in the domain.
    for (absl::uint128 x = 0; x < domain_size; ++x) {
      DPF_ASSERT_OK_AND_ASSIGN(
          ValueType result_0,
          this->dcf_->template Evaluate<ValueType>(key_0, x));
      DPF_ASSERT_OK_AND_ASSIGN(
          ValueType result_1,
          this->dcf_->template Evaluate<ValueType>(key_1, x));
      if (x < alpha) {
        EXPECT_EQ(ValueType(result_0 + result_1), beta)
            << "x=" << x << ", alpha=" << alpha;
      } else {
        EXPECT_EQ(ValueType(result_0 + result_1), ValueType{})
            << "x=" << x << ", alpha=" << alpha;
      }
    }
  }
}

TEST(DcfTest, WorksCorrectlyOnUint64TWithLargeDomain) {
  using ValueType = uint64_t;
  const absl::uint128 domain_size = absl::uint128{1} << 64;
  ValueType beta;
  SetTo42(beta);
  absl::uint128 alpha = 50;

  DcfParameters parameters;
  parameters.mutable_parameters()->set_log_domain_size(64);
  *(parameters.mutable_parameters()->mutable_value_type()) =
      ToValueType<uint64_t>();

  DPF_ASSERT_OK_AND_ASSIGN(auto dcf,
                           DistributedComparisonFunction::Create(parameters));

  // Generate keys.
  DcfKey key_0, key_1;
  DPF_ASSERT_OK_AND_ASSIGN(std::tie(key_0, key_1),
                           dcf->GenerateKeys(alpha, beta));

  // Evaluate on every point in the domain smaller than alpha.
  for (absl::uint128 x = 0; x < alpha; ++x) {
    DPF_ASSERT_OK_AND_ASSIGN(ValueType result_0,
                             dcf->template Evaluate<ValueType>(key_0, x));
    DPF_ASSERT_OK_AND_ASSIGN(ValueType result_1,
                             dcf->template Evaluate<ValueType>(key_1, x));
    EXPECT_EQ(ValueType(result_0 + result_1), beta)
        << "x=" << x << ", alpha=" << alpha;
  }

  // Evaluate on 100 random points in the domain.
  absl::BitGen rng;
  absl::uniform_int_distribution<uint64_t> dist;
  const int kNumEvaluationPoints = 100;
  std::vector<absl::uint128> evaluation_points(kNumEvaluationPoints);
  for (int i = 0; i < kNumEvaluationPoints - 1; ++i) {
    evaluation_points[i] =
        absl::MakeUint128(dist(rng), dist(rng)) % domain_size;
    DPF_ASSERT_OK_AND_ASSIGN(
        uint64_t result_0,
        dcf->template Evaluate<ValueType>(key_0, evaluation_points[i]));
    DPF_ASSERT_OK_AND_ASSIGN(
        ValueType result_1,
        dcf->template Evaluate<ValueType>(key_1, evaluation_points[i]));
    if (evaluation_points[i] < alpha) {
      EXPECT_EQ(ValueType(result_0 + result_1), beta)
          << "x=" << evaluation_points[i] << ", alpha=" << alpha;
    } else {
      EXPECT_EQ(ValueType(result_0 + result_1), ValueType{})
          << "x=" << evaluation_points[i] << ", alpha=" << alpha;
    }
  }
}

}  // namespace

}  // namespace distributed_point_functions

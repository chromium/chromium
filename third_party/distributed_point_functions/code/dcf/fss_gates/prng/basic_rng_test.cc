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

#include "dcf/fss_gates/prng/basic_rng.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/numeric/int128.h"
#include "dpf/internal/status_matchers.h"

namespace distributed_point_functions {
namespace {

using ::testing::Test;

const absl::string_view kSampleSeed = absl::string_view();
constexpr int kNumSamples = 10;

class BasicRngTest : public Test {
 protected:
  void SetUp() {
    DPF_ASSERT_OK_AND_ASSIGN(rng_, BasicRng::Create(kSampleSeed));
  }

  std::unique_ptr<BasicRng> rng_;
};

TEST_F(BasicRngTest, Test8BitRand) {
  // Two random 8 bit strings have 1/256 probability of being equal. Instead,
  // we check that 8 consecutively generated strings are not all equal.
  bool equal = true;
  DPF_ASSERT_OK_AND_ASSIGN(uint8_t prev, rng_->Rand8());
  for (int i = 0; i < 8; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(uint8_t next, rng_->Rand8());
    if (next != prev) {
      equal = false;
    }
    prev = next;
  }
  EXPECT_FALSE(equal);
}

TEST_F(BasicRngTest, Test64BitRand) {
  DPF_ASSERT_OK_AND_ASSIGN(uint64_t r1, rng_->Rand64());
  DPF_ASSERT_OK_AND_ASSIGN(uint64_t r2, rng_->Rand64());
  EXPECT_NE(r1, r2);
}

TEST_F(BasicRngTest, Test128BitRand) {
  DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r1, rng_->Rand128());
  DPF_ASSERT_OK_AND_ASSIGN(absl::uint128 r2, rng_->Rand128());
  EXPECT_NE(r1, r2);
}

TEST_F(BasicRngTest, BytesAreDifferent64) {
  std::vector<uint64_t> rand(kNumSamples);
  for (size_t i = 0; i < kNumSamples; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(rand[i], rng_->Rand64());
  }

  for (int i = 0; i < sizeof(uint64_t); ++i) {
    bool not_all_equal = false;
    for (int j = 1; j < kNumSamples; ++j) {
      auto byte1 = static_cast<uint8_t>(rand[j - 1] >> (8 * i));
      auto byte2 = static_cast<uint8_t>(rand[j] >> (8 * i));
      if (byte1 != byte2) {
        not_all_equal = true;
      }
    }
    EXPECT_TRUE(not_all_equal);
  }
}

TEST_F(BasicRngTest, BytesAreDifferent128) {
  std::vector<absl::uint128> rand(kNumSamples);
  for (int i = 0; i < kNumSamples; ++i) {
    DPF_ASSERT_OK_AND_ASSIGN(rand[i], rng_->Rand128());
  }

  for (size_t i = 0; i < sizeof(absl::uint128); ++i) {
    bool not_all_equal = false;
    for (int j = 1; j < kNumSamples; ++j) {
      auto byte1 = static_cast<uint8_t>(rand[j - 1] >> (8 * i));
      auto byte2 = static_cast<uint8_t>(rand[j] >> (8 * i));
      if (byte1 != byte2) {
        not_all_equal = true;
      }
    }
    EXPECT_TRUE(not_all_equal);
  }
}

}  // namespace
}  // namespace distributed_point_functions

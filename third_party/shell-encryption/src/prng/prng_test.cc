/*
 * Copyright 2019 Google LLC.
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

// This file includes unit tests for the functions available in the interface of
// SecurePrng. PRNGs that follow some specific behavior, such as using an
// internal buffer with random data, and repopulating the buffer according to a
// salt counter, should add specific unit tests files.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <gtest/gtest-typed-test.h>
#include "absl/strings/str_cat.h"
#include "prng/integral_prng_testing_types.h"
#include "prng/integral_prng_types.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"

namespace rlwe {

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

constexpr int kLargeRequestRounds = 2048;

template <typename Prng>
class PrngTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(seed_, Prng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(prng_, Prng::Create(seed_));
  }

  std::string seed_;
  std::unique_ptr<Prng> prng_;
};

TYPED_TEST_SUITE(PrngTest, TestingPrngTypes);

TYPED_TEST(PrngTest, TestNonGeneratedKey) {
  std::string fake_key = "fake_key";
  EXPECT_THAT(
      TypeParam::Create(fake_key),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat(
                   "Cannot create Prng with key of the wrong "
                   "size. Real key length of ",
                   fake_key.length(), " instead of expected key length of ",
                   TypeParam::SeedLength(), "."))));
}

TYPED_TEST(PrngTest, TestLength) {
  ASSERT_OK_AND_ASSIGN(std::string key, TypeParam::GenerateSeed());
  EXPECT_EQ(key.length(), TypeParam::SeedLength());
}

TYPED_TEST(PrngTest, TestRandomness) {
  ASSERT_OK_AND_ASSIGN(std::string key1, TypeParam::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(std::string key2, TypeParam::GenerateSeed());
  EXPECT_NE(key1, key2);
}

TYPED_TEST(PrngTest, TestRand8) {
  // Two random 8 bit strings have 1/256 probability of being equal. Instead,
  // we check that 8 consecutively generated strings are not all equal.
  bool equal = true;
  ASSERT_OK_AND_ASSIGN(Uint64 prev, this->prng_->Rand8());
  for (int i = 0; i < 8; ++i) {
    ASSERT_OK_AND_ASSIGN(Uint64 next, this->prng_->Rand8());
    if (next != prev) {
      equal = false;
    }
    prev = next;
  }
  EXPECT_FALSE(equal);
}

TYPED_TEST(PrngTest, TestRand64) {
  ASSERT_OK_AND_ASSIGN(auto r1, this->prng_->Rand64());
  ASSERT_OK_AND_ASSIGN(auto r2, this->prng_->Rand64());
  EXPECT_NE(r1, r2);
}

TYPED_TEST(PrngTest, ReplaySameInKeyTest) {
  ASSERT_OK_AND_ASSIGN(auto same_prng, TypeParam::Create(this->seed_));

  ASSERT_OK_AND_ASSIGN(auto r8, this->prng_->Rand8());
  ASSERT_OK_AND_ASSIGN(auto same_r8, same_prng->Rand8());
  EXPECT_EQ(r8, same_r8);

  ASSERT_OK_AND_ASSIGN(auto r64, this->prng_->Rand64());
  ASSERT_OK_AND_ASSIGN(auto same_r64, same_prng->Rand64());
  EXPECT_EQ(r64, same_r64);
}

TYPED_TEST(PrngTest, ReplaySameInKeyTestWithResalting) {
  ASSERT_OK_AND_ASSIGN(auto same_key, TypeParam::Create(this->seed_));
  for (int i = 0; i < kLargeRequestRounds; ++i) {
    SCOPED_TRACE(absl::StrCat("Iteration ", i, "."));
    if (i % 2 == 0) {
      ASSERT_OK_AND_ASSIGN(auto r64, this->prng_->Rand64());
      ASSERT_OK_AND_ASSIGN(auto same_r64, same_key->Rand64());
      EXPECT_EQ(r64, same_r64);
    } else {
      ASSERT_OK_AND_ASSIGN(auto r8, this->prng_->Rand8());
      ASSERT_OK_AND_ASSIGN(auto same_r8, same_key->Rand8());
      EXPECT_EQ(r8, same_r8);
    }
  }
}

TYPED_TEST(PrngTest, ReplayDifferentInKeyTest) {
  ASSERT_OK_AND_ASSIGN(std::string other_seed, TypeParam::GenerateSeed());
  EXPECT_NE(this->seed_, other_seed);
  ASSERT_OK_AND_ASSIGN(auto other_prng, TypeParam::Create(other_seed));
  // Two random 8 bit strings have 1/256 probability of being equal. Instead,
  // we check that a sequence of 8 bit strings from each PRNG are not equal.
  bool equal = true;
  for (int i = 0; i < 8; ++i) {
    ASSERT_OK_AND_ASSIGN(auto r8, this->prng_->Rand8());
    ASSERT_OK_AND_ASSIGN(auto other_r8, other_prng->Rand8());
    if (r8 != other_r8) {
      equal = false;
    }
  }
  EXPECT_FALSE(equal);
  ASSERT_OK_AND_ASSIGN(auto r64, this->prng_->Rand64());
  ASSERT_OK_AND_ASSIGN(auto other_r64, other_prng->Rand64());
  EXPECT_NE(r64, other_r64);
}

TYPED_TEST(PrngTest, GeneratesUniqueRandomStrings) {
  const int kKeySize = 20;
  const int kIterations = 10000;
  const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";

  std::vector<std::string> keys;
  for (int i = 0; i < kIterations; i++) {
    // Create a random key
    std::string key(kKeySize, 0);
    for (int j = 0; j < kKeySize; j++) {
      ASSERT_OK_AND_ASSIGN(auto v, this->prng_->Rand8());
      key[j] = charset[static_cast<int>(v) % sizeof(charset)];
    }

    // With very high probability (~(1/36)^20), a key will only appear once.
    int count = 0;
    for (auto k : keys) {
      if (k == key) count++;
    }
    ASSERT_EQ(count, 0);
    keys.push_back(key);
  }
}

}  // namespace
}  // namespace rlwe

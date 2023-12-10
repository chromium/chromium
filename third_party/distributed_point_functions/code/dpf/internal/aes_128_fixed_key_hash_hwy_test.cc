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

#include <limits>
#include <memory>
#include <vector>

#include "absl/flags/parse.h"
#include "absl/log/absl_log.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "dpf/aes_128_fixed_key_hash.h"
#include "dpf/internal/get_hwy_mode.h"
#include "dpf/internal/status_matchers.h"
#include "gtest/gtest.h"
#include "hwy/aligned_allocator.h"
#include "hwy/detect_targets.h"
#include "openssl/aes.h"

// clang-format off
#define HWY_IS_TEST 1
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dpf/internal/aes_128_fixed_key_hash_hwy_test.cc"  // NOLINT
#include "hwy/foreach_target.h"
// clang-format on

#include "dpf/internal/aes_128_fixed_key_hash_hwy.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace distributed_point_functions {
namespace dpf_internal {
namespace HWY_NAMESPACE {

#if HWY_TARGET == HWY_SCALAR

void TestAllAes() {
  return;  // HWY_SCALAR doesn't support AES instructions, so nothing to test.
}

#else

namespace hn = hwy::HWY_NAMESPACE;

constexpr absl::uint128 kKey0 =
    absl::MakeUint128(0x0000000000000000, 0x0000000000000000);
constexpr absl::uint128 kKey1 =
    absl::MakeUint128(0x1111111111111111, 0x1111111111111111);
constexpr int kNumBlocks = 128;  // Must be divisible by (4 * hn::Lanes(d)).
constexpr int kNumBytes = kNumBlocks * sizeof(absl::uint128);

class TestOutputMatchesOpenSSL {
 public:
  template <typename T, typename D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    Reset();
    EvaluateOne(d);
    CheckResult();
    Reset();
    EvaluateFour(d);
    CheckResult();
  }

 private:
  void Reset() {
    inputs_ = hwy::AllocateAligned<absl::uint128>(kNumBlocks);
    ASSERT_NE(inputs_, nullptr);
    masks_ = hwy::AllocateAligned<uint64_t>(2 * kNumBlocks);
    ASSERT_NE(masks_, nullptr);
    for (int i = 0; i < kNumBlocks; ++i) {
      inputs_[i] = absl::MakeUint128(i, i + 1);
      masks_[2 * i] = masks_[2 * i + 1] =
          (i % 3 == 0) ? std::numeric_limits<uint64_t>::max() : 0;
    }
    outputs_ = hwy::AllocateAligned<absl::uint128>(kNumBlocks);
    ASSERT_NE(outputs_, nullptr);
    ASSERT_EQ(0, AES_set_encrypt_key(reinterpret_cast<const uint8_t*>(&kKey0),
                                     128, &expanded_key_0_));
    ASSERT_EQ(0, AES_set_encrypt_key(reinterpret_cast<const uint8_t*>(&kKey1),
                                     128, &expanded_key_1_));
    input_ptr_ = reinterpret_cast<const uint8_t*>(inputs_.get());
    output_ptr_ = reinterpret_cast<uint8_t*>(outputs_.get());
  }

  void CheckResult() {
    // Check the result by comparing with OpenSSL-based AES hash.
    DPF_ASSERT_OK_AND_ASSIGN(
        distributed_point_functions::Aes128FixedKeyHash hash_0,
        distributed_point_functions::Aes128FixedKeyHash::Create(kKey0));
    DPF_ASSERT_OK_AND_ASSIGN(
        distributed_point_functions::Aes128FixedKeyHash hash_1,
        distributed_point_functions::Aes128FixedKeyHash::Create(kKey1));
    std::vector<absl::uint128> wanted_0(kNumBlocks), wanted_1(kNumBlocks);
    DPF_ASSERT_OK(
        hash_0.Evaluate(absl::MakeConstSpan(inputs_.get(), kNumBlocks),
                        absl::MakeSpan(wanted_0)));
    DPF_ASSERT_OK(
        hash_1.Evaluate(absl::MakeConstSpan(inputs_.get(), kNumBlocks),
                        absl::MakeSpan(wanted_1)));
    for (int i = 0; i < kNumBlocks; ++i) {
      if (i % 3 == 0) {
        EXPECT_EQ(wanted_1[i], outputs_.get()[i]) << "i=" << i;
      } else {
        EXPECT_EQ(wanted_0[i], outputs_.get()[i]) << "i=" << i;
      }
    }
  }

  template <typename D>
  void EvaluateOne(D d) {
    hn::Repartition<uint64_t, D> d64;
    for (int i = 0; i + hn::Lanes(d) <= kNumBytes; i += hn::Lanes(d)) {
      const auto in = hn::Load(d, input_ptr_ + i);
      const auto mask =
          hn::MaskFromVec(hn::Load(d64, masks_.get() + i / sizeof(uint64_t)));
      auto out = hn::Undefined(d);
      HashOneWithKeyMask(
          d, in, mask, reinterpret_cast<const uint8_t*>(expanded_key_0_.rd_key),
          reinterpret_cast<const uint8_t*>(expanded_key_1_.rd_key), out);
      hn::Store(out, d, output_ptr_ + i);
    }
  }

  template <typename D>
  void EvaluateFour(D d) {
    hn::Repartition<uint64_t, D> d64;
    // Evaluate four vectors at once. Assumes kNumBytes is divisible by (4 *
    // hn::Lanes(d)).
    for (int i = 0; i < kNumBytes; i += 4 * hn::Lanes(d)) {
      const auto in_0 = hn::Load(d, input_ptr_ + i);
      const auto in_1 = hn::Load(d, input_ptr_ + i + 1 * hn::Lanes(d));
      const auto in_2 = hn::Load(d, input_ptr_ + i + 2 * hn::Lanes(d));
      const auto in_3 = hn::Load(d, input_ptr_ + i + 3 * hn::Lanes(d));
      const auto mask_0 =
          hn::MaskFromVec(hn::Load(d64, masks_.get() + i / sizeof(uint64_t)));
      const auto mask_1 = hn::MaskFromVec(hn::Load(
          d64, masks_.get() + (i + 1 * hn::Lanes(d)) / sizeof(uint64_t)));
      const auto mask_2 = hn::MaskFromVec(hn::Load(
          d64, masks_.get() + (i + 2 * hn::Lanes(d)) / sizeof(uint64_t)));
      const auto mask_3 = hn::MaskFromVec(hn::Load(
          d64, masks_.get() + (i + 3 * hn::Lanes(d)) / sizeof(uint64_t)));
      auto out_0 = hn::Undefined(d);
      auto out_1 = hn::Undefined(d);
      auto out_2 = hn::Undefined(d);
      auto out_3 = hn::Undefined(d);
      HashFourWithKeyMask(
          d, in_0, in_1, in_2, in_3, mask_0, mask_1, mask_2, mask_3,
          reinterpret_cast<const uint8_t*>(expanded_key_0_.rd_key),
          reinterpret_cast<const uint8_t*>(expanded_key_1_.rd_key), out_0,
          out_1, out_2, out_3);
      hn::Store(out_0, d, output_ptr_ + i);
      hn::Store(out_1, d, output_ptr_ + i + 1 * hn::Lanes(d));
      hn::Store(out_2, d, output_ptr_ + i + 2 * hn::Lanes(d));
      hn::Store(out_3, d, output_ptr_ + i + 3 * hn::Lanes(d));
    }

    // Check the result by comparing with OpenSSL-based AES hash.
    DPF_ASSERT_OK_AND_ASSIGN(
        distributed_point_functions::Aes128FixedKeyHash hash_0,
        distributed_point_functions::Aes128FixedKeyHash::Create(kKey0));
    DPF_ASSERT_OK_AND_ASSIGN(
        distributed_point_functions::Aes128FixedKeyHash hash_1,
        distributed_point_functions::Aes128FixedKeyHash::Create(kKey1));
    std::vector<absl::uint128> wanted_0(kNumBlocks), wanted_1(kNumBlocks);
    DPF_ASSERT_OK(
        hash_0.Evaluate(absl::MakeConstSpan(inputs_.get(), kNumBlocks),
                        absl::MakeSpan(wanted_0)));
    DPF_ASSERT_OK(
        hash_1.Evaluate(absl::MakeConstSpan(inputs_.get(), kNumBlocks),
                        absl::MakeSpan(wanted_1)));
    for (int i = 0; i < kNumBlocks; ++i) {
      if (i % 3 == 0) {
        EXPECT_EQ(wanted_1[i], outputs_.get()[i]) << "i=" << i;
      } else {
        EXPECT_EQ(wanted_0[i], outputs_.get()[i]) << "i=" << i;
      }
    }
  }

  hwy::AlignedFreeUniquePtr<absl::uint128[]> inputs_, outputs_;
  hwy::AlignedFreeUniquePtr<uint64_t[]> masks_;
  const uint8_t* input_ptr_;
  uint8_t* output_ptr_;
  HWY_ALIGN AES_KEY expanded_key_0_, expanded_key_1_;
};

void TestAllAes() {
  hn::ForGE128Vectors<TestOutputMatchesOpenSSL>()(uint8_t{0});
}

#endif  // HWY_TARGET == HWY_SCALAR

}  // namespace HWY_NAMESPACE
}  // namespace dpf_internal
}  // namespace distributed_point_functions
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace distributed_point_functions {
namespace dpf_internal {
HWY_BEFORE_TEST(Aes128FixedKeyHashHwyTest);
HWY_EXPORT_AND_TEST_P(Aes128FixedKeyHashHwyTest, TestAllAes);

TEST(Aes128FixedKeyHashHwy, LogHwyMode) {
  ABSL_LOG(INFO) << "Highway is in " << GetHwyModeAsString() << " mode";
}

}  // namespace dpf_internal
}  // namespace distributed_point_functions

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

#endif

/*
 * Copyright 2019 Google Inc.
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

#include "third_party/private-join-and-compute/src/crypto/big_num.h"

#include <cmath>
#include <vector>

#include "third_party/private-join-and-compute/src/chromium_patch.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/openssl.inc"
#include "third_party/private-join-and-compute/src/util/status.inc"

namespace private_join_and_compute {

BigNum::BigNum(const BigNum& other)
    : bn_(BignumPtr(CHECK_NOTNULL(BN_dup(other.bn_.get())))),
      bn_ctx_(other.bn_ctx_) {}

BigNum& BigNum::operator=(const BigNum& other) {
  bn_ = BignumPtr(CHECK_NOTNULL(BN_dup(other.bn_.get())));
  bn_ctx_ = other.bn_ctx_;
  return *this;
}

BigNum::BigNum(BigNum&& other)
    : bn_(std::move(other.bn_)), bn_ctx_(other.bn_ctx_) {}

BigNum& BigNum::operator=(BigNum&& other) {
  bn_ = std::move(other.bn_);
  bn_ctx_ = other.bn_ctx_;
  return *this;
}

BigNum::BigNum(BN_CTX* bn_ctx, uint64_t number) : BigNum::BigNum(bn_ctx) {
  CRYPTO_CHECK(BN_set_u64(bn_.get(), number));
}

BigNum::BigNum(BN_CTX* bn_ctx, const std::string& bytes)
    : BigNum::BigNum(bn_ctx) {
  CRYPTO_CHECK(nullptr !=
               BN_bin2bn(reinterpret_cast<const unsigned char*>(bytes.data()),
                         bytes.size(), bn_.get()));
}

BigNum::BigNum(BN_CTX* bn_ctx, const unsigned char* bytes, int length)
    : BigNum::BigNum(bn_ctx) {
  CRYPTO_CHECK(nullptr != BN_bin2bn(bytes, length, bn_.get()));
}

BigNum::BigNum(BN_CTX* bn_ctx) {
  bn_ = BignumPtr(CHECK_NOTNULL(BN_new()));
  bn_ctx_ = bn_ctx;
}

BigNum::BigNum(BN_CTX* bn_ctx, BignumPtr bn) {
  bn_ = std::move(bn);
  bn_ctx_ = bn_ctx;
}

const BIGNUM* BigNum::GetConstBignumPtr() const { return bn_.get(); }

std::string BigNum::ToBytes() const {
  CHECK(IsNonNegative()) << "Cannot serialize a negative BigNum.";
  int length = BN_num_bytes(bn_.get());
  std::vector<unsigned char> bytes(length);
  BN_bn2bin(bn_.get(), bytes.data());
  return std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
}

StatusOr<uint64_t> BigNum::ToIntValue() const {
  uint64_t val;
  if (!BN_get_u64(bn_.get(), &val)) {
    return InvalidArgumentError("BigNum has more than 64 bits.");
  }
  return val;
}

int BigNum::BitLength() const { return BN_num_bits(bn_.get()); }

bool BigNum::IsPrime(double prime_error_probability) const {
  int rounds = static_cast<int>(ceil(-log(prime_error_probability) / log(4)));
  return (1 == BN_is_prime_ex(bn_.get(), rounds, bn_ctx_, nullptr));
}

bool BigNum::IsSafePrime(double prime_error_probability) const {
  return IsPrime(prime_error_probability) &&
         ((*this - BigNum(bn_ctx_, 1)) / BigNum(bn_ctx_, 2))
             .IsPrime(prime_error_probability);
}

bool BigNum::IsZero() const { return BN_is_zero(bn_.get()); }

bool BigNum::IsOne() const { return BN_is_one(bn_.get()); }

bool BigNum::IsNonNegative() const { return !BN_is_negative(bn_.get()); }

BigNum BigNum::GetLastNBits(int n) const {
  BigNum r = *this;
  // Returns 0 on error (if r is already shorter than n bits), but the return
  // value in that case should be the original value so there is no need to have
  // error checking here.
  BN_mask_bits(r.bn_.get(), n);
  return r;
}

bool BigNum::IsBitSet(int n) const { return BN_is_bit_set(bn_.get(), n); }

// Returns a BigNum whose value is (- *this).
// Causes a check failure if the operation fails.
BigNum BigNum::Neg() const {
  BigNum r = *this;
  BN_set_negative(r.bn_.get(), !BN_is_negative(r.bn_.get()));
  return r;
}

BigNum BigNum::Add(const BigNum& val) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_add(r.bn_.get(), bn_.get(), val.bn_.get()));
  return r;
}

BigNum BigNum::Mul(const BigNum& val) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_mul(r.bn_.get(), bn_.get(), val.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::Sub(const BigNum& val) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_sub(r.bn_.get(), bn_.get(), val.bn_.get()));
  return r;
}

BigNum BigNum::Div(const BigNum& val) const {
  BigNum r(bn_ctx_);
  BignumPtr rem(CHECK_NOTNULL(BN_new()));
  CRYPTO_CHECK(
      1 == BN_div(r.bn_.get(), rem.get(), bn_.get(), val.bn_.get(), bn_ctx_));
  CHECK(BN_is_zero(rem.get())) << "Use DivAndTruncate() instead of Div() if "
                                  "you want truncated division.";
  return r;
}

BigNum BigNum::DivAndTruncate(const BigNum& val) const {
  BigNum r(bn_ctx_);
  BignumPtr rem(CHECK_NOTNULL(BN_new()));
  CRYPTO_CHECK(
      1 == BN_div(r.bn_.get(), rem.get(), bn_.get(), val.bn_.get(), bn_ctx_));
  return r;
}

int BigNum::CompareTo(const BigNum& val) const {
  return BN_cmp(bn_.get(), val.bn_.get());
}

BigNum BigNum::Exp(const BigNum& exponent) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 ==
               BN_exp(r.bn_.get(), bn_.get(), exponent.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::Mod(const BigNum& m) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_nnmod(r.bn_.get(), bn_.get(), m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModAdd(const BigNum& val, const BigNum& m) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_mod_add(r.bn_.get(), bn_.get(), val.bn_.get(),
                               m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModSub(const BigNum& val, const BigNum& m) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_mod_sub(r.bn_.get(), bn_.get(), val.bn_.get(),
                               m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModMul(const BigNum& val, const BigNum& m) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_mod_mul(r.bn_.get(), bn_.get(), val.bn_.get(),
                               m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModExp(const BigNum& exponent, const BigNum& m) const {
  CHECK(exponent.IsNonNegative()) << "Cannot use a negative exponent in BigNum "
                                     "ModExp.";
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_mod_exp(r.bn_.get(), bn_.get(), exponent.bn_.get(),
                               m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModSqr(const BigNum& m) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_mod_sqr(r.bn_.get(), bn_.get(), m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModInverse(const BigNum& m) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(nullptr !=
               BN_mod_inverse(r.bn_.get(), bn_.get(), m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModSqrt(const BigNum& m) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(nullptr !=
               BN_mod_sqrt(r.bn_.get(), bn_.get(), m.bn_.get(), bn_ctx_));
  return r;
}

BigNum BigNum::ModNegate(const BigNum& m) const {
  if (IsZero()) {
    return *this;
  }
  return m - Mod(m);
}

BigNum BigNum::Lshift(int n) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_lshift(r.bn_.get(), bn_.get(), n));
  return r;
}

BigNum BigNum::Rshift(int n) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_rshift(r.bn_.get(), bn_.get(), n));
  return r;
}

BigNum BigNum::Gcd(const BigNum& val) const {
  BigNum r(bn_ctx_);
  CRYPTO_CHECK(1 == BN_gcd(r.bn_.get(), bn_.get(), val.bn_.get(), bn_ctx_));
  return r;
}

}  // namespace private_join_and_compute

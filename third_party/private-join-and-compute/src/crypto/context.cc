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

#include "third_party/private-join-and-compute/src/crypto/context.h"

#include <math.h>

#include <algorithm>
#include <cmath>

#include "third_party/private-join-and-compute/src/chromium_patch.h"

namespace private_join_and_compute {

std::string OpenSSLErrorString() {
  char buf[256];
  ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
  return buf;
}

Context::Context()
    : bn_ctx_(CHECK_NOTNULL(BN_CTX_new())),
      evp_md_ctx_(CHECK_NOTNULL(EVP_MD_CTX_create())),
      zero_bn_(CreateBigNum(0)),
      one_bn_(CreateBigNum(1)),
      two_bn_(CreateBigNum(2)),
      three_bn_(CreateBigNum(3)) {
  CHECK(RAND_status()) << "OpenSSL PRNG is not properly seeded.";
  HMAC_CTX_init(&hmac_ctx_);
}

Context::~Context() { HMAC_CTX_cleanup(&hmac_ctx_); }

BN_CTX* Context::GetBnCtx() { return bn_ctx_.get(); }

BigNum Context::CreateBigNum(const std::string& bytes) {
  return BigNum(bn_ctx_.get(), bytes);
}

BigNum Context::CreateBigNum(uint64_t number) {
  return BigNum(bn_ctx_.get(), number);
}

BigNum Context::CreateBigNum(BigNum::BignumPtr bn) {
  return BigNum(bn_ctx_.get(), std::move(bn));
}

std::string Context::Sha256String(const std::string& bytes) {
  unsigned char hash[EVP_MAX_MD_SIZE];
  CRYPTO_CHECK(1 ==
               EVP_DigestInit_ex(evp_md_ctx_.get(), EVP_sha256(), nullptr));
  CRYPTO_CHECK(
      1 == EVP_DigestUpdate(evp_md_ctx_.get(), bytes.c_str(), bytes.length()));
  unsigned int md_len;
  CRYPTO_CHECK(1 == EVP_DigestFinal_ex(evp_md_ctx_.get(), hash, &md_len));
  return std::string(reinterpret_cast<char*>(hash), md_len);
}

std::string Context::Sha512String(const std::string& bytes) {
  unsigned char hash[EVP_MAX_MD_SIZE];
  CRYPTO_CHECK(1 ==
               EVP_DigestInit_ex(evp_md_ctx_.get(), EVP_sha512(), nullptr));
  CRYPTO_CHECK(
      1 == EVP_DigestUpdate(evp_md_ctx_.get(), bytes.c_str(), bytes.length()));
  unsigned int md_len;
  CRYPTO_CHECK(1 == EVP_DigestFinal_ex(evp_md_ctx_.get(), hash, &md_len));
  return std::string(reinterpret_cast<char*>(hash), md_len);
}

BigNum Context::RandomOracle(const std::string& x, const BigNum& max_value,
                             RandomOracleHashType hash_type) {
  int hash_output_length = 256;
  if (hash_type == SHA512) {
    hash_output_length = 512;
  }
  int output_bit_length = max_value.BitLength() + hash_output_length;
  int iter_count =
      std::ceil(static_cast<float>(output_bit_length) / hash_output_length);
  CHECK(iter_count * hash_output_length < 130048)
      << "The domain bit length must not be greater than "
         "130048. Desired bit length: "
      << output_bit_length;
  int excess_bit_count = (iter_count * hash_output_length) - output_bit_length;
  BigNum hash_output = CreateBigNum(0);
  for (int i = 1; i < iter_count + 1; i++) {
    hash_output = hash_output.Lshift(hash_output_length);
    std::string hashed_string;
    if (hash_type == SHA512) {
      hashed_string = Sha512String(CreateBigNum(i).ToBytes().append(x));
    } else {
      hashed_string = Sha256String(CreateBigNum(i).ToBytes().append(x));
    }
    hash_output = hash_output + CreateBigNum(hashed_string);
  }
  return hash_output.Rshift(excess_bit_count).Mod(max_value);
}

BigNum Context::RandomOracleSha512(const std::string& x,
                                   const BigNum& max_value) {
  return RandomOracle(x, max_value, SHA512);
}

BigNum Context::RandomOracleSha256(const std::string& x,
                                   const BigNum& max_value) {
  return RandomOracle(x, max_value, SHA256);
}

BigNum Context::PRF(const std::string& key, const std::string& data,
                    const BigNum& max_value) {
  CHECK_GE(key.size() * 8, 80u);
  CHECK_LE(max_value.BitLength(), 512)
      << "The requested output length is not supported. The maximum "
         "supported output length is 512. The requested output length is "
      << max_value.BitLength();
  CRYPTO_CHECK(1 == HMAC_Init_ex(&hmac_ctx_, key.c_str(), key.size(),
                                 EVP_sha512(), nullptr));
  CRYPTO_CHECK(1 ==
               HMAC_Update(&hmac_ctx_,
                           reinterpret_cast<const unsigned char*>(data.data()),
                           data.size()));
  unsigned int md_len;
  unsigned char hash[EVP_MAX_MD_SIZE];
  CRYPTO_CHECK(1 == HMAC_Final(&hmac_ctx_, hash, &md_len));
  BigNum hash_bn(bn_ctx_.get(), hash, md_len);
  BigNum hash_bn_reduced = hash_bn.GetLastNBits(max_value.BitLength());
  if (hash_bn_reduced < max_value) {
    return hash_bn_reduced;
  } else {
    return Context::PRF(key, hash_bn.ToBytes(), max_value);
  }
}

BigNum Context::GenerateSafePrime(int prime_length) {
  BigNum r(bn_ctx_.get());
  CRYPTO_CHECK(1 == BN_generate_prime_ex(r.bn_.get(), prime_length, 1, nullptr,
                                         nullptr, nullptr));
  return r;
}

BigNum Context::GeneratePrime(int prime_length) {
  BigNum r(bn_ctx_.get());
  CRYPTO_CHECK(1 == BN_generate_prime_ex(r.bn_.get(), prime_length, 0, nullptr,
                                         nullptr, nullptr));
  return r;
}

BigNum Context::GenerateRandLessThan(const BigNum& max_value) {
  BigNum r(bn_ctx_.get());
  CRYPTO_CHECK(1 == BN_rand_range(r.bn_.get(), max_value.bn_.get()));
  return r;
}

BigNum Context::GenerateRandBetween(const BigNum& start, const BigNum& end) {
  CHECK(start < end);
  return GenerateRandLessThan(end - start) + start;
}

std::string Context::GenerateRandomBytes(int num_bytes) {
  CHECK_GE(num_bytes, 0) << "num_bytes must be nonnegative, provided value was "
                         << num_bytes << ".";
  std::unique_ptr<unsigned char[]> bytes(new unsigned char[num_bytes]);
  CRYPTO_CHECK(1 == RAND_bytes(bytes.get(), num_bytes));
  return std::string(reinterpret_cast<char*>(bytes.get()), num_bytes);
}

BigNum Context::RelativelyPrimeRandomLessThan(const BigNum& num) {
  BigNum rand_num = GenerateRandLessThan(num);
  while (rand_num.Gcd(num) > One()) {
    rand_num = GenerateRandLessThan(num);
  }
  return rand_num;
}

}  // namespace private_join_and_compute

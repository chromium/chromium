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

#include "third_party/private-join-and-compute/src/crypto/ec_commutative_cipher.h"

#include <memory>
#include <string>
#include <utility>

#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/private-join-and-compute/src/crypto/big_num.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"
#include "third_party/private-join-and-compute/src/util/status.inc"

namespace private_join_and_compute {

namespace {

constexpr absl::string_view kEcCommutativeCipherDst = "ECCommutativeCipher";

// Invert private scalar using Fermat's Little Theorem to avoid side-channel
// attacks. This avoids the caveat of ModInverseBlinded, namely that the
// underlying BN_mod_inverse_blinded is not available on all platforms.
BigNum InvertPrivateScalar(const BigNum& scalar,
                           const ECGroup& ec_group,
                           Context& context) {
  const BigNum& order = ec_group.GetOrder();
  return scalar.ModExp(order.Sub(context.Two()), order);
}

}  // namespace

ECCommutativeCipher::ECCommutativeCipher(std::unique_ptr<Context> context,
                                         ECGroup group,
                                         BigNum private_key,
                                         HashType hash_type)
    : context_(std::move(context)),
      group_(std::move(group)),
      private_key_(std::move(private_key)),
      private_key_inverse_(
          InvertPrivateScalar(private_key_, group_, *context_)),
      hash_type_(hash_type) {}

bool ECCommutativeCipher::ValidateHashType(HashType hash_type) {
  return (hash_type == SHA256 || hash_type == SHA384 || hash_type == SHA512 ||
          hash_type == SSWU_RO);
}

bool ECCommutativeCipher::ValidateHashType(int hash_type) {
  return hash_type >= SHA256 && hash_type <= SSWU_RO;
}

StatusOr<std::unique_ptr<ECCommutativeCipher>>
ECCommutativeCipher::CreateWithNewKey(int curve_id, HashType hash_type) {
  std::unique_ptr<Context> context(new Context);
  StatusOr<ECGroup> group = ECGroup::Create(curve_id, context.get());
  if (!group.ok()) {
    return group.status();
  }
  if (!ECCommutativeCipher::ValidateHashType(hash_type)) {
    return InvalidArgumentError("Invalid hash type.");
  }
  BigNum private_key = group->GeneratePrivateKey();
  return std::unique_ptr<ECCommutativeCipher>(
      new ECCommutativeCipher(std::move(context), *std::move(group),
                              std::move(private_key), hash_type));
}

StatusOr<std::unique_ptr<ECCommutativeCipher>>
ECCommutativeCipher::CreateFromKey(int curve_id,
                                   absl::string_view key_bytes,
                                   HashType hash_type) {
  std::unique_ptr<Context> context(new Context);
  StatusOr<ECGroup> group = ECGroup::Create(curve_id, context.get());
  if (!group.ok()) {
    return group.status();
  }
  if (!ECCommutativeCipher::ValidateHashType(hash_type)) {
    return InvalidArgumentError("Invalid hash type.");
  }
  BigNum private_key = context->CreateBigNum(key_bytes);
  auto status = group->CheckPrivateKey(private_key);
  if (!status.ok()) {
    return status;
  }
  return std::unique_ptr<ECCommutativeCipher>(
      new ECCommutativeCipher(std::move(context), *std::move(group),
                              std::move(private_key), hash_type));
}

StatusOr<std::unique_ptr<ECCommutativeCipher>>
ECCommutativeCipher::CreateWithKeyFromSeed(int curve_id,
                                           absl::string_view seed_bytes,
                                           absl::string_view tag_bytes,
                                           HashType hash_type) {
  std::unique_ptr<Context> context(new Context);
  StatusOr<ECGroup> group = ECGroup::Create(curve_id, context.get());
  if (!group.ok()) {
    return group.status();
  }
  if (seed_bytes.size() < 16) {
    return InvalidArgumentError("Seed is too short.");
  }
  if (!ECCommutativeCipher::ValidateHashType(hash_type)) {
    return InvalidArgumentError("Invalid hash type.");
  }
  BigNum private_key = context->PRF(seed_bytes, tag_bytes, group->GetOrder());
  return std::unique_ptr<ECCommutativeCipher>(
      new ECCommutativeCipher(std::move(context), *std::move(group),
                              std::move(private_key), hash_type));
}

StatusOr<std::string> ECCommutativeCipher::Encrypt(
    absl::string_view plaintext) {
  StatusOr<ECPoint> hashed_point = HashToTheCurveInternal(plaintext);
  if (!hashed_point.ok()) {
    return hashed_point.status();
  }
  StatusOr<ECPoint> encrypted_point = Encrypt(*hashed_point);
  if (!encrypted_point.ok()) {
    return encrypted_point.status();
  }
  return encrypted_point->ToBytesCompressed();
}

StatusOr<std::string> ECCommutativeCipher::ReEncrypt(
    absl::string_view ciphertext) {
  StatusOr<ECPoint> point = group_.CreateECPoint(ciphertext);
  if (!point.ok()) {
    return point.status();
  }
  StatusOr<ECPoint> reencrypted_point = Encrypt(*point);
  if (!reencrypted_point.ok()) {
    return reencrypted_point.status();
  }
  return reencrypted_point->ToBytesCompressed();
}

StatusOr<ECPoint> ECCommutativeCipher::Encrypt(const ECPoint& point) {
  return point.Mul(private_key_);
}

StatusOr<std::pair<std::string, std::string>>
ECCommutativeCipher::ReEncryptElGamalCiphertext(
    const std::pair<std::string, std::string>& elgamal_ciphertext) {
  StatusOr<ECPoint> u = group_.CreateECPoint(elgamal_ciphertext.first);
  if (!u.ok()) {
    return u.status();
  }
  StatusOr<ECPoint> e = group_.CreateECPoint(elgamal_ciphertext.second);
  if (!e.ok()) {
    return e.status();
  }

  elgamal::Ciphertext decoded_ciphertext = {*std::move(u), *std::move(e)};

  StatusOr<elgamal::Ciphertext> reencrypted_ciphertext =
      elgamal::Exp(decoded_ciphertext, private_key_);
  if (!reencrypted_ciphertext.ok()) {
    return reencrypted_ciphertext.status();
  }

  StatusOr<std::string> serialized_u =
      reencrypted_ciphertext->u.ToBytesCompressed();
  if (!serialized_u.ok()) {
    return serialized_u.status();
  }
  StatusOr<std::string> serialized_e =
      reencrypted_ciphertext->e.ToBytesCompressed();
  if (!serialized_e.ok()) {
    return serialized_e.status();
  }

  return std::make_pair(*std::move(serialized_u), *std::move(serialized_e));
}

StatusOr<std::string> ECCommutativeCipher::Decrypt(
    absl::string_view ciphertext) {
  StatusOr<ECPoint> point = group_.CreateECPoint(ciphertext);
  if (!point.ok()) {
    return point.status();
  }
  StatusOr<ECPoint> decrypted_point = point->Mul(private_key_inverse_);
  if (!decrypted_point.ok()) {
    return decrypted_point.status();
  }
  return decrypted_point->ToBytesCompressed();
}

StatusOr<ECPoint> ECCommutativeCipher::HashToTheCurveInternal(
    absl::string_view plaintext) {
  StatusOr<ECPoint> point;
  if (hash_type_ == SHA512) {
    point = group_.GetPointByHashingToCurveSha512(plaintext);
  } else if (hash_type_ == SHA384) {
    point = group_.GetPointByHashingToCurveSha384(plaintext);
  } else if (hash_type_ == SHA256) {
    point = group_.GetPointByHashingToCurveSha256(plaintext);
  } else if (hash_type_ == SSWU_RO) {
    point = group_.GetPointByHashingToCurveSswuRo(plaintext,
                                                  kEcCommutativeCipherDst);
  } else {
    return InvalidArgumentError("Invalid hash type.");
  }
  return point;
}

StatusOr<std::string> ECCommutativeCipher::HashToTheCurve(
    absl::string_view plaintext) {
  StatusOr<ECPoint> point = HashToTheCurveInternal(plaintext);
  if (!point.ok()) {
    return point.status();
  }
  return point->ToBytesCompressed();
}

std::string ECCommutativeCipher::GetPrivateKeyBytes() const {
  return private_key_.ToBytes();
}

}  // namespace private_join_and_compute

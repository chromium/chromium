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

#include "third_party/private-join-and-compute/src/crypto/ec_commutative_cipher.h"

#include <utility>

#include "third_party/private-join-and-compute/src/crypto/big_num.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/util/status.inc"

namespace private_join_and_compute {
namespace {

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
  return (hash_type == SHA256 || hash_type == SHA512);
}

StatusOr<std::unique_ptr<ECCommutativeCipher>>
ECCommutativeCipher::CreateWithNewKey(int curve_id, HashType hash_type) {
  std::unique_ptr<Context> context(new Context);
  ASSIGN_OR_RETURN(ECGroup group, ECGroup::Create(curve_id, context.get()));
  if (!ECCommutativeCipher::ValidateHashType(hash_type)) {
    return InvalidArgumentError("Invalid hash type.");
  }
  BigNum private_key = group.GeneratePrivateKey();
  return std::unique_ptr<ECCommutativeCipher>(new ECCommutativeCipher(
      std::move(context), std::move(group), std::move(private_key), hash_type));
}

StatusOr<std::unique_ptr<ECCommutativeCipher>>
ECCommutativeCipher::CreateFromKey(int curve_id, const std::string& key_bytes,
                                   HashType hash_type) {
  std::unique_ptr<Context> context(new Context);
  ASSIGN_OR_RETURN(ECGroup group, ECGroup::Create(curve_id, context.get()));
  if (!ECCommutativeCipher::ValidateHashType(hash_type)) {
    return InvalidArgumentError("Invalid hash type.");
  }
  BigNum private_key = context->CreateBigNum(key_bytes);
  auto status = group.CheckPrivateKey(private_key);
  if (!status.ok()) {
    return status;
  }
  return std::unique_ptr<ECCommutativeCipher>(new ECCommutativeCipher(
      std::move(context), std::move(group), std::move(private_key), hash_type));
}

StatusOr<std::string> ECCommutativeCipher::Encrypt(
    const std::string& plaintext) const {
  StatusOr<ECPoint> status_or_point;
  if (hash_type_ == SHA512) {
    status_or_point = group_.GetPointByHashingToCurveSha512(plaintext);
  } else if (hash_type_ == SHA256) {
    status_or_point = group_.GetPointByHashingToCurveSha256(plaintext);
  } else {
    return InvalidArgumentError("Invalid hash type.");
  }

  if (!status_or_point.ok()) {
    return status_or_point.status();
  }
  ASSIGN_OR_RETURN(ECPoint encrypted_point, Encrypt(status_or_point.value()));
  return encrypted_point.ToBytesCompressed();
}

StatusOr<std::string> ECCommutativeCipher::ReEncrypt(
    const std::string& ciphertext) const {
  ASSIGN_OR_RETURN(ECPoint point, group_.CreateECPoint(ciphertext));
  ASSIGN_OR_RETURN(ECPoint reencrypted_point, Encrypt(point));
  return reencrypted_point.ToBytesCompressed();
}

StatusOr<ECPoint> ECCommutativeCipher::Encrypt(const ECPoint& point) const {
  return point.Mul(private_key_);
}

StatusOr<std::string> ECCommutativeCipher::Decrypt(
    const std::string& ciphertext) const {
  ASSIGN_OR_RETURN(ECPoint point, group_.CreateECPoint(ciphertext));
  ASSIGN_OR_RETURN(ECPoint decrypted_point, point.Mul(private_key_inverse_));
  return decrypted_point.ToBytesCompressed();
}

std::string ECCommutativeCipher::GetPrivateKeyBytes() const {
  return private_key_.ToBytes();
}

}  // namespace private_join_and_compute

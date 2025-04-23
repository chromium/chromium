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

#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "third_party/abseil-cpp/absl/memory/memory.h"
#include "third_party/private-join-and-compute/src/crypto/big_num.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
#include "third_party/private-join-and-compute/src/util/status.inc"

namespace private_join_and_compute {

namespace elgamal {

StatusOr<std::pair<std::unique_ptr<PublicKey>, std::unique_ptr<PrivateKey>>>
GenerateKeyPair(const ECGroup& ec_group) {
  StatusOr<ECPoint> g = ec_group.GetFixedGenerator();
  if (!g.ok()) {
    return g.status();
  }
  BigNum x = ec_group.GeneratePrivateKey();
  StatusOr<ECPoint> y = g->Mul(x);
  if (!y.ok()) {
    return y.status();
  }

  std::unique_ptr<PublicKey> public_key(
      new PublicKey({*std::move(g), *std::move(y)}));
  std::unique_ptr<PrivateKey> private_key(new PrivateKey({std::move(x)}));

  return {{std::move(public_key), std::move(private_key)}};
}

StatusOr<std::unique_ptr<PublicKey>> GeneratePublicKeyFromShares(
    const std::vector<std::unique_ptr<elgamal::PublicKey>>& shares) {
  if (shares.empty()) {
    return InvalidArgumentError(
        "ElGamal::GeneratePublicKeyFromShares() : empty shares provided");
  }
  StatusOr<ECPoint> g = (*shares.begin())->g.Clone();
  if (!g.ok()) {
    return g.status();
  }
  StatusOr<ECPoint> y = (*shares.begin())->y.Clone();
  if (!y.ok()) {
    return y.status();
  }
  for (size_t i = 1; i < shares.size(); i++) {
    CHECK(g->CompareTo((*shares.at(i)).g))
        << "Invalid public key shares provided with different generators g";
    y = y->Add((*shares.at(i)).y);
    if (!y.ok()) {
      return y.status();
    }
  }

  return absl::WrapUnique(new PublicKey({*std::move(g), *std::move(y)}));
}

StatusOr<elgamal::Ciphertext> Mul(const elgamal::Ciphertext& ciphertext1,
                                  const elgamal::Ciphertext& ciphertext2) {
  StatusOr<ECPoint> u = ciphertext1.u.Add(ciphertext2.u);
  if (!u.ok()) {
    return u.status();
  }
  StatusOr<ECPoint> e = ciphertext1.e.Add(ciphertext2.e);
  if (!e.ok()) {
    return e.status();
  }
  return {{*std::move(u), *std::move(e)}};
}

StatusOr<elgamal::Ciphertext> Exp(const elgamal::Ciphertext& ciphertext,
                                  const BigNum& scalar) {
  StatusOr<ECPoint> u = ciphertext.u.Mul(scalar);
  if (!u.ok()) {
    return u.status();
  }
  StatusOr<ECPoint> e = ciphertext.e.Mul(scalar);
  if (!e.ok()) {
    return e.status();
  }
  return {{*std::move(u), *std::move(e)}};
}

StatusOr<Ciphertext> GetZero(const ECGroup* group) {
  StatusOr<ECPoint> u = group->GetPointAtInfinity();
  if (!u.ok()) {
    return u.status();
  }
  StatusOr<ECPoint> e = group->GetPointAtInfinity();
  if (!e.ok()) {
    return e.status();
  }
  return {{*std::move(u), *std::move(e)}};
}

StatusOr<Ciphertext> CloneCiphertext(const Ciphertext& ciphertext) {
  StatusOr<ECPoint> clone_u = ciphertext.u.Clone();
  if (!clone_u.ok()) {
    return clone_u.status();
  }
  StatusOr<ECPoint> clone_e = ciphertext.e.Clone();
  if (!clone_e.ok()) {
    return clone_e.status();
  }
  return {{*std::move(clone_u), *std::move(clone_e)}};
}

bool IsCiphertextZero(const Ciphertext& ciphertext) {
  return ciphertext.u.IsPointAtInfinity() && ciphertext.e.IsPointAtInfinity();
}

}  // namespace elgamal

////////////////////////////////////////////////////////////////////////////////
// PUBLIC ELGAMAL
////////////////////////////////////////////////////////////////////////////////

ElGamalEncrypter::ElGamalEncrypter(
    const ECGroup* ec_group,
    std::unique_ptr<elgamal::PublicKey> elgamal_public_key)
    : ec_group_(ec_group), public_key_(std::move(elgamal_public_key)) {}

// Encrypts a message m, that has already been mapped onto the curve.
StatusOr<elgamal::Ciphertext> ElGamalEncrypter::Encrypt(
    const ECPoint& message) const {
  BigNum r = ec_group_->GeneratePrivateKey();  // generate a random exponent
  // u = g^r , e = m * y^r .
  StatusOr<ECPoint> u = public_key_->g.Mul(r);
  if (!u.ok()) {
    return u.status();
  }
  StatusOr<ECPoint> y_to_r = public_key_->y.Mul(r);
  if (!y_to_r.ok()) {
    return y_to_r.status();
  }
  StatusOr<ECPoint> e = message.Add(*y_to_r);
  if (!e.ok()) {
    return e.status();
  }
  return {{*std::move(u), *std::move(e)}};
}

StatusOr<elgamal::Ciphertext> ElGamalEncrypter::ReRandomize(
    const elgamal::Ciphertext& elgamal_ciphertext) const {
  BigNum r = ec_group_->GeneratePrivateKey();  // generate a random exponent
  // u = old_u * g^r , e = old_e * y^r .
  StatusOr<ECPoint> g_to_r = public_key_->g.Mul(r);
  if (!g_to_r.ok()) {
    return g_to_r.status();
  }
  StatusOr<ECPoint> u = elgamal_ciphertext.u.Add(*g_to_r);
  if (!u.ok()) {
    return u.status();
  }
  StatusOr<ECPoint> y_to_r = public_key_->y.Mul(r);
  if (!y_to_r.ok()) {
    return y_to_r.status();
  }
  StatusOr<ECPoint> e = elgamal_ciphertext.e.Add(*y_to_r);
  if (!e.ok()) {
    return e.status();
  }
  return {{*std::move(u), *std::move(e)}};
}

////////////////////////////////////////////////////////////////////////////////
// PRIVATE ELGAMAL
////////////////////////////////////////////////////////////////////////////////

ElGamalDecrypter::ElGamalDecrypter(
    std::unique_ptr<elgamal::PrivateKey> elgamal_private_key)
    : private_key_(std::move(elgamal_private_key)) {}

StatusOr<ECPoint> ElGamalDecrypter::Decrypt(
    const elgamal::Ciphertext& ciphertext) const {
  StatusOr<ECPoint> u_to_x = ciphertext.u.Mul(private_key_->x);
  if (!u_to_x.ok()) {
    return u_to_x.status();
  }
  StatusOr<ECPoint> u_to_x_inverse = u_to_x->Inverse();
  if (!u_to_x_inverse.ok()) {
    return u_to_x_inverse.status();
  }
  StatusOr<ECPoint> message = ciphertext.e.Add(*u_to_x_inverse);
  if (!message.ok()) {
    return message.status();
  }
  return {*std::move(message)};
}

StatusOr<elgamal::Ciphertext> ElGamalDecrypter::PartialDecrypt(
    const elgamal::Ciphertext& ciphertext) const {
  StatusOr<ECPoint> clone_u = ciphertext.u.Clone();
  if (!clone_u.ok()) {
    return clone_u.status();
  }
  StatusOr<ECPoint> dec_e = ElGamalDecrypter::Decrypt(ciphertext);
  if (!dec_e.ok()) {
    return dec_e.status();
  }
  return {{*std::move(clone_u), *std::move(dec_e)}};
}

}  // namespace private_join_and_compute

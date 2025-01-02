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

#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

#include <vector>

#include "third_party/private-join-and-compute/src/crypto/big_num.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
#include "third_party/private-join-and-compute/src/util/status.inc"

namespace private_join_and_compute {

namespace elgamal {

StatusOr<std::pair<std::unique_ptr<PublicKey>, std::unique_ptr<PrivateKey>>>
GenerateKeyPair(const ECGroup& ec_group) {
  ASSIGN_OR_RETURN(ECPoint g, ec_group.GetFixedGenerator());
  BigNum x = ec_group.GeneratePrivateKey();
  ASSIGN_OR_RETURN(ECPoint y, g.Mul(x));

  std::unique_ptr<PublicKey> public_key(
      new PublicKey({std::move(g), std::move(y)}));
  std::unique_ptr<PrivateKey> private_key(new PrivateKey({std::move(x)}));

  return {{std::move(public_key), std::move(private_key)}};
}

StatusOr<elgamal::Ciphertext> Mul(const elgamal::Ciphertext& ciphertext1,
                                  const elgamal::Ciphertext& ciphertext2) {
  ASSIGN_OR_RETURN(ECPoint u, ciphertext1.u.Add(ciphertext2.u));
  ASSIGN_OR_RETURN(ECPoint e, ciphertext1.e.Add(ciphertext2.e));
  return {{std::move(u), std::move(e)}};
}

StatusOr<elgamal::Ciphertext> Exp(const elgamal::Ciphertext& ciphertext,
                                  const BigNum& scalar) {
  ASSIGN_OR_RETURN(ECPoint u, ciphertext.u.Mul(scalar));
  ASSIGN_OR_RETURN(ECPoint e, ciphertext.e.Mul(scalar));
  return {{std::move(u), std::move(e)}};
}

StatusOr<Ciphertext> GetZero(const ECGroup* group) {
  ASSIGN_OR_RETURN(ECPoint u, group->GetPointAtInfinity());
  ASSIGN_OR_RETURN(ECPoint e, group->GetPointAtInfinity());
  return {{std::move(u), std::move(e)}};
}

StatusOr<Ciphertext> CloneCiphertext(const Ciphertext& ciphertext) {
  ASSIGN_OR_RETURN(ECPoint clone_u, ciphertext.u.Clone());
  ASSIGN_OR_RETURN(ECPoint clone_e, ciphertext.e.Clone());
  return {{std::move(clone_u), std::move(clone_e)}};
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
  ASSIGN_OR_RETURN(ECPoint u, public_key_->g.Mul(r));
  ASSIGN_OR_RETURN(ECPoint y_to_r, public_key_->y.Mul(r));
  ASSIGN_OR_RETURN(ECPoint e, message.Add(y_to_r));
  return {{std::move(u), std::move(e)}};
}

StatusOr<elgamal::Ciphertext> ElGamalEncrypter::ReRandomize(
    const elgamal::Ciphertext& elgamal_ciphertext) const {
  BigNum r = ec_group_->GeneratePrivateKey();  // generate a random exponent
  // u = old_u * g^r , e = old_e * y^r .
  ASSIGN_OR_RETURN(ECPoint g_to_r, public_key_->g.Mul(r));
  ASSIGN_OR_RETURN(ECPoint u, elgamal_ciphertext.u.Add(g_to_r));
  ASSIGN_OR_RETURN(ECPoint y_to_r, public_key_->y.Mul(r));
  ASSIGN_OR_RETURN(ECPoint e, elgamal_ciphertext.e.Add(y_to_r));
  return {{std::move(u), std::move(e)}};
}

////////////////////////////////////////////////////////////////////////////////
// PRIVATE ELGAMAL
////////////////////////////////////////////////////////////////////////////////

ElGamalDecrypter::ElGamalDecrypter(
    std::unique_ptr<elgamal::PrivateKey> elgamal_private_key)
    : private_key_(std::move(elgamal_private_key)) {}

StatusOr<ECPoint> ElGamalDecrypter::Decrypt(
    const elgamal::Ciphertext& ciphertext) const {
  ASSIGN_OR_RETURN(ECPoint u_to_x, ciphertext.u.Mul(private_key_->x));
  ASSIGN_OR_RETURN(ECPoint u_to_x_inverse, u_to_x.Inverse());
  ASSIGN_OR_RETURN(ECPoint message, ciphertext.e.Add(u_to_x_inverse));
  return {std::move(message)};
}

}  // namespace private_join_and_compute

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

#include "third_party/private-join-and-compute/src/crypto/ec_point.h"

#include <string>
#include <utility>
#include <vector>

#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/private-join-and-compute/src/chromium_patch.h"
#include "third_party/private-join-and-compute/src/crypto/big_num.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/openssl.inc"
#include "third_party/private-join-and-compute/src/util/status.inc"

namespace private_join_and_compute {

ECPoint::ECPoint(const EC_GROUP* group, BN_CTX* bn_ctx)
    : bn_ctx_(bn_ctx), group_(group) {
  point_ = ECPointPtr(EC_POINT_new(group_));
}

ECPoint::ECPoint(const EC_GROUP* group, BN_CTX* bn_ctx, const BigNum& x,
                 const BigNum& y)
    : ECPoint::ECPoint(group, bn_ctx) {
  CRYPTO_CHECK(1 == EC_POINT_set_affine_coordinates_GFp(
                        group_, point_.get(), x.GetConstBignumPtr(),
                        y.GetConstBignumPtr(), bn_ctx_));
}

ECPoint::ECPoint(const EC_GROUP* group, BN_CTX* bn_ctx, ECPointPtr point)
    : ECPoint::ECPoint(group, bn_ctx) {
  point_ = std::move(point);
}

StatusOr<std::string> ECPoint::ToBytesCompressed() const {
  int length = EC_POINT_point2oct(
      group_, point_.get(), POINT_CONVERSION_COMPRESSED, nullptr, 0, bn_ctx_);
  std::vector<unsigned char> bytes(length);
  if (0 == EC_POINT_point2oct(group_, point_.get(), POINT_CONVERSION_COMPRESSED,
                              bytes.data(), length, bn_ctx_)) {
    return InternalError(
        absl::StrCat("EC_POINT_point2oct failed:", OpenSSLErrorString()));
  }
  return std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
}

StatusOr<std::string> ECPoint::ToBytesUnCompressed() const {
  int length = EC_POINT_point2oct(
      group_, point_.get(), POINT_CONVERSION_UNCOMPRESSED, nullptr, 0, bn_ctx_);
  std::vector<unsigned char> bytes(length);
  if (0 == EC_POINT_point2oct(group_, point_.get(),
                              POINT_CONVERSION_UNCOMPRESSED, bytes.data(),
                              length, bn_ctx_)) {
    return InternalError(
        absl::StrCat("EC_POINT_point2oct failed:", OpenSSLErrorString()));
  }
  return std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
}

StatusOr<std::pair<BigNum::BignumPtr, BigNum::BignumPtr>>
ECPoint::GetAffineCoordinates() const {
  BIGNUM* bn_x = BN_new();
  BIGNUM* bn_y = BN_new();
  if (bn_x == nullptr || bn_y == nullptr) {
    return InternalError(
        absl::StrCat("ECPoint::GetAffineCoordinates - Could not create BIGNUM.",
                     OpenSSLErrorString()));
  }
  if (0 == EC_POINT_get_affine_coordinates(group_, point_.get(), bn_x, bn_y,
                                           bn_ctx_)) {
    return InternalError(absl::StrCat(
        "EC_POINT_get_affine_coordinates_GFp failed:", OpenSSLErrorString()));
  }
  return std::make_pair(BigNum::BignumPtr(bn_x), BigNum::BignumPtr(bn_y));
}

StatusOr<ECPoint> ECPoint::Mul(const BigNum& scalar) const {
  ECPoint r = ECPoint(group_, bn_ctx_);
  if (1 != EC_POINT_mul(group_, r.point_.get(), nullptr, point_.get(),
                        scalar.GetConstBignumPtr(), bn_ctx_)) {
    return InternalError(
        absl::StrCat("EC_POINT_mul failed:", OpenSSLErrorString()));
  }
  return std::move(r);
}

StatusOr<ECPoint> ECPoint::Add(const ECPoint& point) const {
  ECPoint r = ECPoint(group_, bn_ctx_);
  if (1 != EC_POINT_add(group_, r.point_.get(), point_.get(),
                        point.point_.get(), bn_ctx_)) {
    return InternalError(
        absl::StrCat("EC_POINT_add failed:", OpenSSLErrorString()));
  }
  return std::move(r);
}

StatusOr<ECPoint> ECPoint::Clone() const {
  ECPoint r = ECPoint(group_, bn_ctx_);
  if (1 != EC_POINT_copy(r.point_.get(), point_.get())) {
    return InternalError(
        absl::StrCat("EC_POINT_copy failed:", OpenSSLErrorString()));
  }
  return std::move(r);
}

StatusOr<ECPoint> ECPoint::Inverse() const {
  // Create a copy of this.
  StatusOr<ECPoint> inv = ECPoint(group_, bn_ctx_);
  if (!inv.ok()) {
    return inv.status();
  }
  if (1 != EC_POINT_copy(inv->point_.get(), point_.get())) {
    return InternalError(
        absl::StrCat("EC_POINT_copy failed:", OpenSSLErrorString()));
  }
  // Invert the copy in-place.
  if (1 != EC_POINT_invert(group_, inv->point_.get(), bn_ctx_)) {
    return InternalError(
        absl::StrCat("EC_POINT_invert failed:", OpenSSLErrorString()));
  }
  return inv;
}

bool ECPoint::IsPointAtInfinity() const {
  return EC_POINT_is_at_infinity(group_, point_.get());
}

bool ECPoint::CompareTo(const ECPoint& point) const {
  return 0 == EC_POINT_cmp(group_, point_.get(), point.point_.get(), bn_ctx_);
}

}  // namespace private_join_and_compute

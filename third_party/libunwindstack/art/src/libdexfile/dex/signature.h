/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBDEXFILE_DEX_SIGNATURE_H_
#define ART_LIBDEXFILE_DEX_SIGNATURE_H_

#include <iosfwd>
#include <string>
#include <string_view>

#include <android-base/logging.h>

#include "base/value_object.h"

namespace art {

namespace dex {
struct ProtoId;
}  // namespace dex
class DexFile;

// Abstract the signature of a method.
class Signature : public ValueObject {
 public:
  std::string ToString() const;

  static Signature NoSignature() {
    return Signature();
  }

  bool IsVoid() const;
  uint32_t GetNumberOfParameters() const;

  bool operator==(const Signature& rhs) const;
  bool operator!=(const Signature& rhs) const {
    return !(*this == rhs);
  }

  bool operator==(std::string_view rhs) const;

  // Three-way compare.
  // Return a value >0 if `rhs` is higher than `*this`, <0 if lower and 0 if equal.
  //
  // The order is the same as the `ProtoId` order required by dex file specification if both
  // signatures were in the same dex file.
  int Compare(const Signature& rhs) const;

 private:
  Signature(const DexFile* dex, const dex::ProtoId& proto) : dex_file_(dex), proto_id_(&proto) {
  }

  Signature() = default;

  friend class DexFile;

  const DexFile* dex_file_ = nullptr;
  const dex::ProtoId* proto_id_ = nullptr;
};
std::ostream& operator<<(std::ostream& os, const Signature& sig);

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_SIGNATURE_H_

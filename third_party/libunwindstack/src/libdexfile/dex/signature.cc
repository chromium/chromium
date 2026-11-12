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

#include "signature-inl.h"

#include <string.h>

#include <ostream>
#include <type_traits>

#include "base/string_view_cpp20.h"

namespace art {

using dex::TypeList;

std::string Signature::ToString() const {
  if (dex_file_ == nullptr) {
    CHECK(proto_id_ == nullptr);
    return "<no signature>";
  }
  const TypeList* params = dex_file_->GetProtoParameters(*proto_id_);
  std::string result;
  if (params == nullptr) {
    result += "()";
  } else {
    result += "(";
    for (uint32_t i = 0; i < params->Size(); ++i) {
      result += dex_file_->StringByTypeIdx(params->GetTypeItem(i).type_idx_);
    }
    result += ")";
  }
  result += dex_file_->StringByTypeIdx(proto_id_->return_type_idx_);
  return result;
}

uint32_t Signature::GetNumberOfParameters() const {
  const TypeList* params = dex_file_->GetProtoParameters(*proto_id_);
  return (params != nullptr) ? params->Size() : 0;
}

bool Signature::IsVoid() const {
  const char* return_type = dex_file_->GetReturnTypeDescriptor(*proto_id_);
  return strcmp(return_type, "V") == 0;
}

bool Signature::operator==(std::string_view rhs) const {
  if (dex_file_ == nullptr) {
    return false;
  }
  std::string_view tail(rhs);
  if (!StartsWith(tail, "(")) {
    return false;  // Invalid signature
  }
  tail.remove_prefix(1);  // "(";
  const TypeList* params = dex_file_->GetProtoParameters(*proto_id_);
  if (params != nullptr) {
    for (uint32_t i = 0; i < params->Size(); ++i) {
      std::string_view param(dex_file_->StringByTypeIdx(params->GetTypeItem(i).type_idx_));
      if (!StartsWith(tail, param)) {
        return false;
      }
      tail.remove_prefix(param.length());
    }
  }
  if (!StartsWith(tail, ")")) {
    return false;
  }
  tail.remove_prefix(1);  // ")";
  return tail == dex_file_->StringByTypeIdx(proto_id_->return_type_idx_);
}

std::ostream& operator<<(std::ostream& os, const Signature& sig) {
  return os << sig.ToString();
}

}  // namespace art

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

#ifndef ART_LIBDEXFILE_DEX_SIGNATURE_INL_H_
#define ART_LIBDEXFILE_DEX_SIGNATURE_INL_H_

#include "signature.h"

#include "dex_file-inl.h"

namespace art {

inline bool Signature::operator==(const Signature& rhs) const {
  if (dex_file_ == nullptr) {
    return rhs.dex_file_ == nullptr;
  }
  if (rhs.dex_file_ == nullptr) {
    return false;
  }
  if (dex_file_ == rhs.dex_file_) {
    return proto_id_ == rhs.proto_id_;
  }
  std::string_view lhs_shorty = dex_file_->GetShortyView(*proto_id_);
  if (lhs_shorty != rhs.dex_file_->GetShortyView(*rhs.proto_id_)) {
    return false;  // Shorty mismatch.
  }
  if (lhs_shorty[0] == 'L') {
    const dex::TypeId& return_type_id = dex_file_->GetTypeId(proto_id_->return_type_idx_);
    const dex::TypeId& rhs_return_type_id =
        rhs.dex_file_->GetTypeId(rhs.proto_id_->return_type_idx_);
    if (!DexFile::StringEquals(dex_file_, return_type_id.descriptor_idx_,
                               rhs.dex_file_, rhs_return_type_id.descriptor_idx_)) {
      return false;  // Return type mismatch.
    }
  }
  if (lhs_shorty.find('L', 1) != std::string_view::npos) {
    const dex::TypeList* lhs_params = dex_file_->GetProtoParameters(*proto_id_);
    const dex::TypeList* rhs_params = rhs.dex_file_->GetProtoParameters(*rhs.proto_id_);
    // We found a reference parameter in the matching shorty, so both lists must be non-empty.
    DCHECK(lhs_params != nullptr);
    DCHECK(rhs_params != nullptr);
    uint32_t params_size = lhs_shorty.size() - 1u;
    DCHECK_EQ(params_size, lhs_params->Size());  // Parameter list sizes must match shorty.
    DCHECK_EQ(params_size, rhs_params->Size());
    for (uint32_t i = 0; i < params_size; ++i) {
      const dex::TypeId& lhs_param_id = dex_file_->GetTypeId(lhs_params->GetTypeItem(i).type_idx_);
      const dex::TypeId& rhs_param_id =
          rhs.dex_file_->GetTypeId(rhs_params->GetTypeItem(i).type_idx_);
      if (!DexFile::StringEquals(dex_file_, lhs_param_id.descriptor_idx_,
                                 rhs.dex_file_, rhs_param_id.descriptor_idx_)) {
        return false;  // Parameter type mismatch.
      }
    }
  }
  return true;
}

inline int Signature::Compare(const Signature& rhs) const {
  DCHECK(dex_file_ != nullptr);
  DCHECK(rhs.dex_file_ != nullptr);
  if (dex_file_ == rhs.dex_file_) {
    return static_cast<int>(dex_file_->GetIndexForProtoId(*proto_id_).index_) -
           static_cast<int>(rhs.dex_file_->GetIndexForProtoId(*rhs.proto_id_).index_);
  }
  // Use shorty to avoid looking at primitive type descriptors
  // as long as they are not compared with reference descriptors.
  std::string_view lhs_shorty = dex_file_->GetShortyView(*proto_id_);
  std::string_view rhs_shorty = rhs.dex_file_->GetShortyView(*rhs.proto_id_);
  // Note that 'L' in a shorty can represent an array type starting with '[',
  // so do the full type descriptor comparison when we see 'L' in either shorty.
  if (lhs_shorty[0] == 'L' || rhs_shorty[0] == 'L') {
    std::string_view lhs_return_type = dex_file_->GetTypeDescriptorView(
        dex_file_->GetTypeId(proto_id_->return_type_idx_));
    std::string_view rhs_return_type = rhs.dex_file_->GetTypeDescriptorView(
        rhs.dex_file_->GetTypeId(rhs.proto_id_->return_type_idx_));
    int cmp_result = lhs_return_type.compare(rhs_return_type);
    if (cmp_result != 0) {
      return cmp_result;
    }
  } else if (lhs_shorty[0] != rhs_shorty[0]) {
    return static_cast<int>(lhs_shorty[0]) - static_cast<int>(rhs_shorty[0]);
  }
  size_t min_shorty_size = std::min(lhs_shorty.size(), rhs_shorty.size());
  if (min_shorty_size != 1u) {  // If both shortys contain parameters, compare parameters.
    const dex::TypeList* lhs_params = dex_file_->GetProtoParameters(*proto_id_);
    const dex::TypeList* rhs_params = rhs.dex_file_->GetProtoParameters(*rhs.proto_id_);
    DCHECK(lhs_params != nullptr);
    DCHECK(rhs_params != nullptr);
    for (size_t i = 1u; i != min_shorty_size; ++i) {
      if (lhs_shorty[i] == 'L' || rhs_shorty[i] == 'L') {
        std::string_view lhs_param_type = dex_file_->GetTypeDescriptorView(
            dex_file_->GetTypeId(lhs_params->GetTypeItem(i - 1u).type_idx_));
        std::string_view rhs_param_type = rhs.dex_file_->GetTypeDescriptorView(
            rhs.dex_file_->GetTypeId(rhs_params->GetTypeItem(i - 1u).type_idx_));
        int cmp_result = lhs_param_type.compare(rhs_param_type);
        if (cmp_result != 0) {
          return cmp_result;
        }
      } else if (lhs_shorty[i] != rhs_shorty[i]) {
        return static_cast<int>(lhs_shorty[i]) - static_cast<int>(rhs_shorty[i]);
      }
    }
  }
  if (lhs_shorty.size() == rhs_shorty.size()) {
    return 0;
  } else if (lhs_shorty.size() < rhs_shorty.size()) {
    return -1;
  } else {
    return 1;
  }
}

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_SIGNATURE_INL_H_

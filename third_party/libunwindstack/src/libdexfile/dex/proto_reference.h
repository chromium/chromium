/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_PROTO_REFERENCE_H_
#define ART_LIBDEXFILE_DEX_PROTO_REFERENCE_H_

#include <stdint.h>

#include <android-base/logging.h>
#include <string_view>

#include "dex/dex_file-inl.h"
#include "dex/dex_file_reference.h"
#include "dex/dex_file_types.h"

namespace art {

// A proto is located by its DexFile and the proto_ids_ table index into that DexFile.
class ProtoReference : public DexFileReference {
 public:
  ProtoReference(const DexFile* file, dex::ProtoIndex index)
     : DexFileReference(file, index.index_) {}

  dex::ProtoIndex ProtoIndex() const {
    return dex::ProtoIndex(index);
  }

  const dex::ProtoId& ProtoId() const {
    return dex_file->GetProtoId(ProtoIndex());
  }

  std::string_view ReturnType() const {
    return dex_file->GetTypeDescriptorView(dex_file->GetTypeId(ProtoId().return_type_idx_));
  }
};

struct ProtoReferenceValueComparator {
  bool operator()(const ProtoReference& lhs, const ProtoReference& rhs) const {
    if (lhs.dex_file == rhs.dex_file) {
      DCHECK_EQ(lhs.index < rhs.index, SlowCompare(lhs, rhs));

      return lhs.index < rhs.index;
    } else {
      return SlowCompare(lhs, rhs);
    }
  }

  bool SlowCompare(const ProtoReference& lhs, const ProtoReference& rhs) const {
    // Compare return type first.
    const dex::ProtoId& prid1 = lhs.ProtoId();
    const dex::ProtoId& prid2 = rhs.ProtoId();
    int return_type_diff = lhs.ReturnType().compare(rhs.ReturnType());
    if (return_type_diff != 0) {
      return return_type_diff < 0;
    }
    // And then compare parameters lexicographically.
    const dex::TypeList* params1 = lhs.dex_file->GetProtoParameters(prid1);
    size_t param1_size = (params1 != nullptr) ? params1->Size() : 0u;
    const dex::TypeList* params2 = rhs.dex_file->GetProtoParameters(prid2);
    size_t param2_size = (params2 != nullptr) ? params2->Size() : 0u;
    for (size_t i = 0, num = std::min(param1_size, param2_size); i != num; ++i) {
      std::string_view l_param = lhs.dex_file->GetTypeDescriptorView(
          lhs.dex_file->GetTypeId(params1->GetTypeItem(i).type_idx_));
      std::string_view r_param = rhs.dex_file->GetTypeDescriptorView(
          rhs.dex_file->GetTypeId(params2->GetTypeItem(i).type_idx_));

      int param_diff = l_param.compare(r_param);
      if (param_diff != 0) {
        return param_diff < 0;
      }
    }
    return param1_size < param2_size;
  }
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_PROTO_REFERENCE_H_

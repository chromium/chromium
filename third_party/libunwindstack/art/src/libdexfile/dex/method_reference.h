/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_METHOD_REFERENCE_H_
#define ART_LIBDEXFILE_DEX_METHOD_REFERENCE_H_

#include <stdint.h>
#include <string>
#include "dex/dex_file.h"
#include "dex/dex_file_reference.h"
#include "dex/proto_reference.h"

namespace art {

// A method is uniquely located by its DexFile and the method_ids_ table index into that DexFile
class MethodReference : public DexFileReference {
 public:
  MethodReference(const DexFile* file, uint32_t index) : DexFileReference(file, index) {}
  std::string PrettyMethod(bool with_signature = true) const {
    return dex_file->PrettyMethod(index, with_signature);
  }
  const dex::MethodId& GetMethodId() const {
    return dex_file->GetMethodId(index);
  }
  const art::ProtoReference GetProtoReference() const {
    return ProtoReference(dex_file, GetMethodId().proto_idx_);
  }
};

// Compare the actual referenced method signatures. Used for method reference deduplication.
struct MethodReferenceValueComparator {
  bool operator()(MethodReference mr1, MethodReference mr2) const {
    if (mr1.dex_file == mr2.dex_file) {
      DCHECK_EQ(mr1.index < mr2.index, SlowCompare(mr1, mr2));
      return mr1.index < mr2.index;
    } else {
      return SlowCompare(mr1, mr2);
    }
  }

  bool SlowCompare(MethodReference mr1, MethodReference mr2) const {
    // The order is the same as for method ids in a single dex file.
    // Compare the class descriptors first.
    const dex::MethodId& mid1 = mr1.GetMethodId();
    const dex::MethodId& mid2 = mr2.GetMethodId();
    int descriptor_diff = strcmp(mr1.dex_file->StringByTypeIdx(mid1.class_idx_),
                                 mr2.dex_file->StringByTypeIdx(mid2.class_idx_));
    if (descriptor_diff != 0) {
      return descriptor_diff < 0;
    }
    // Compare names second.
    int name_diff = strcmp(mr1.dex_file->GetMethodName(mid1), mr2.dex_file->GetMethodName(mid2));
    if (name_diff != 0) {
      return name_diff < 0;
    }
    // Then compare protos.
    return ProtoReferenceValueComparator()(mr1.GetProtoReference(), mr2.GetProtoReference());
  }
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_METHOD_REFERENCE_H_

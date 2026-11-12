/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "art_api/dex_file_external.h"

#include <inttypes.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/mapped_file.h>
#include <android-base/stringprintf.h>

#include <dex/class_accessor-inl.h>
#include <dex/code_item_accessors-inl.h>
#include <dex/dex_file-inl.h>
#include <dex/dex_file_loader.h>

extern "C" {

struct ADexFile_Method {
  ADexFile* adex;
  uint32_t index;
  size_t offset;
  size_t size;
};

// Opaque implementation of ADexFile for the C interface.
struct ADexFile {
  explicit ADexFile(std::unique_ptr<const art::DexFile> dex_file)
      : dex_file_(std::move(dex_file)) {}

  inline bool FindMethod(uint32_t dex_offset, /*out*/ ADexFile_Method* result) {
    uint32_t class_def_index;
    if (GetClassDefIndex(dex_offset, &class_def_index)) {
      art::ClassAccessor accessor(*dex_file_, class_def_index);
      for (const art::ClassAccessor::Method& method : accessor.GetMethods()) {
        art::CodeItemInstructionAccessor code = method.GetInstructions();
        if (!code.HasCodeItem()) {
          continue;
        }
        size_t offset = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file_->Begin();
        size_t size = code.InsnsSizeInBytes();
        if (offset <= dex_offset && dex_offset < offset + size) {
          *result = ADexFile_Method {
            .adex = this,
            .index = method.GetIndex(),
            .offset = offset,
            .size = size,
          };
          return true;
        }
      }
    }
    return false;
  }

  void CreateClassCache() {
    // Create binary search table with (end_dex_offset, class_def_index) entries.
    // That is, we don't assume that dex code of given class is consecutive.
    std::deque<std::pair<uint32_t, uint32_t>> cache;
    for (art::ClassAccessor accessor : dex_file_->GetClasses()) {
      for (const art::ClassAccessor::Method& method : accessor.GetMethods()) {
        art::CodeItemInstructionAccessor code = method.GetInstructions();
        if (code.HasCodeItem()) {
          int32_t offset = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file_->Begin();
          DCHECK_NE(offset, 0);
          cache.emplace_back(offset + code.InsnsSizeInBytes(), accessor.GetClassDefIndex());
        }
      }
    }
    std::sort(cache.begin(), cache.end());

    // If two consecutive methods belong to same class, we can merge them.
    // This tends to reduce the number of entries (used memory) by 10x.
    size_t num_entries = cache.size();
    if (cache.size() > 1) {
      for (auto it = std::next(cache.begin()); it != cache.end(); it++) {
        if (std::prev(it)->second == it->second) {
          std::prev(it)->first = 0;  // Clear entry with lower end_dex_offset (mark to remove).
          num_entries--;
        }
      }
    }

    // The cache is immutable now. Store it as continuous vector to save space.
    class_cache_.reserve(num_entries);
    auto pred = [](auto it) { return it.first != 0; };  // Entries to copy (not cleared above).
    std::copy_if(cache.begin(), cache.end(), std::back_inserter(class_cache_), pred);
  }

  inline bool GetClassDefIndex(uint32_t dex_offset, uint32_t* class_def_index) {
    if (class_cache_.empty()) {
      CreateClassCache();
    }

    // Binary search in the class cache. First element of the pair is the key.
    auto comp = [](uint32_t value, const auto& it) { return value < it.first; };
    auto it = std::upper_bound(class_cache_.begin(), class_cache_.end(), dex_offset, comp);
    if (it != class_cache_.end()) {
      *class_def_index = it->second;
      return true;
    }
    return false;
  }

  // The underlying ART object.
  std::unique_ptr<const art::DexFile> dex_file_;

  // Binary search table with (end_dex_offset, class_def_index) entries.
  std::vector<std::pair<uint32_t, uint32_t>> class_cache_;

  // Used as short lived temporary when needed. Avoids alloc/free.
  std::string temporary_qualified_name_;
};

ADexFile_Error ADexFile_create(const void* _Nonnull address,
                               size_t size,
                               size_t* _Nullable new_size,
                               const char* _Nonnull location,
                               /*out*/ ADexFile* _Nullable * _Nonnull out_dex_file) {
  *out_dex_file = nullptr;

  if (size < sizeof(art::DexFile::Header)) {
    if (new_size != nullptr) {
      *new_size = sizeof(art::DexFile::Header);
    }
    return ADEXFILE_ERROR_NOT_ENOUGH_DATA;
  }

  const art::DexFile::Header* header = reinterpret_cast<const art::DexFile::Header*>(address);
  if (size < header->header_size_) {
    if (new_size != nullptr) {
      *new_size = header->header_size_;
    }
    return ADEXFILE_ERROR_NOT_ENOUGH_DATA;
  }

  uint32_t dex_size = header->file_size_;  // Size of "one dex file" excluding any shared data.
  uint32_t full_size = dex_size;           // Includes referenced shared data past the end of dex.
  if (art::CompactDexFile::IsMagicValid(header->magic_)) {
    // Compact dex files store the data section separately so that it can be shared.
    // Therefore we need to extend the read memory range to include it.
    // TODO: This might be wasteful as we might read data in between as well.
    //       In practice, this should be fine, as such sharing only happens on disk.
    uint32_t computed_file_size;
    if (__builtin_add_overflow(header->data_off_, header->data_size_, &computed_file_size)) {
      return ADEXFILE_ERROR_INVALID_HEADER;
    }
    if (computed_file_size > full_size) {
      full_size = computed_file_size;
    }
  } else if (art::StandardDexFile::IsMagicValid(header->magic_)) {
    full_size = header->ContainerSize() - header->HeaderOffset();
  } else {
    return ADEXFILE_ERROR_INVALID_HEADER;
  }

  if (size < full_size) {
    if (new_size != nullptr) {
      *new_size = full_size;
    }
    return ADEXFILE_ERROR_NOT_ENOUGH_DATA;
  }

  std::string loc_str(location);
  std::string error_msg;
  art::DexFileLoader loader(static_cast<const uint8_t*>(address), full_size, loc_str);
  std::unique_ptr<const art::DexFile> dex_file = loader.OpenOne(/*header_offset=*/0,
                                                                header->checksum_,
                                                                /*oat_dex_file=*/nullptr,
                                                                /*verify=*/false,
                                                                /*verify_checksum=*/false,
                                                                &error_msg);
  if (dex_file == nullptr) {
    LOG(ERROR) << "Can not open dex file " << loc_str << ": " << error_msg;
    return ADEXFILE_ERROR_INVALID_DEX;
  }

  *out_dex_file = new ADexFile(std::move(dex_file));
  return ADEXFILE_ERROR_OK;
}

void ADexFile_destroy(ADexFile* self) {
  delete self;
}

size_t ADexFile_findMethodAtOffset(ADexFile* self,
                                   size_t dex_offset,
                                   ADexFile_MethodCallback* callback,
                                   void* callback_data) {
  const art::DexFile* dex_file = self->dex_file_.get();
  if (!dex_file->IsInDataSection(dex_file->Begin() + dex_offset)) {
    return 0;  // The DEX offset is not within the bytecode of this dex file.
  }

  if (dex_file->IsCompactDexFile()) {
    // The data section of compact dex files might be shared.
    // Check the subrange unique to this compact dex.
    const art::CompactDexFile::Header& cdex_header =
        dex_file->AsCompactDexFile()->GetHeader();
    uint32_t begin = cdex_header.data_off_ + cdex_header.OwnedDataBegin();
    uint32_t end = cdex_header.data_off_ + cdex_header.OwnedDataEnd();
    if (dex_offset < begin || dex_offset >= end) {
      return 0;  // The DEX offset is not within the bytecode of this dex file.
    }
  }

  ADexFile_Method info;
  if (!self->FindMethod(dex_offset, &info)) {
    return 0;
  }

  callback(callback_data, &info);
  return 1;
}

size_t ADexFile_forEachMethod(ADexFile* self,
                              ADexFile_MethodCallback* callback,
                              void* callback_data) {
  size_t count = 0;
  for (art::ClassAccessor accessor : self->dex_file_->GetClasses()) {
    for (const art::ClassAccessor::Method& method : accessor.GetMethods()) {
      art::CodeItemInstructionAccessor code = method.GetInstructions();
      if (code.HasCodeItem()) {
        size_t offset = reinterpret_cast<const uint8_t*>(code.Insns()) - self->dex_file_->Begin();
        ADexFile_Method info {
          .adex = self,
          .index = method.GetIndex(),
          .offset = offset,
          .size = code.InsnsSizeInBytes(),
        };
        callback(callback_data, &info);
        count++;
      }
    }
  }
  return count;
}

size_t ADexFile_Method_getCodeOffset(const ADexFile_Method* self,
                                     size_t* out_size) {
  if (out_size != nullptr) {
    *out_size = self->size;
  }
  return self->offset;
}

const char* ADexFile_Method_getName(const ADexFile_Method* self,
                                    size_t* out_size) {
  const char* name = self->adex->dex_file_->GetMethodName(self->index);
  if (out_size != nullptr) {
    *out_size = strlen(name);
  }
  return name;
}

const char* ADexFile_Method_getQualifiedName(const ADexFile_Method* self,
                                             int with_params,
                                             size_t* out_size) {
  std::string& temp = self->adex->temporary_qualified_name_;
  temp.clear();
  self->adex->dex_file_->AppendPrettyMethod(self->index, with_params, &temp);
  if (out_size != nullptr) {
    *out_size = temp.size();
  }
  return temp.data();
}

const char* ADexFile_Method_getClassDescriptor(const ADexFile_Method* self,
                                               size_t* out_size) {
  const art::dex::MethodId& method_id = self->adex->dex_file_->GetMethodId(self->index);
  const char* name = self->adex->dex_file_->GetMethodDeclaringClassDescriptor(method_id);
  if (out_size != nullptr) {
    *out_size = strlen(name);
  }
  return name;
}

const char* ADexFile_Error_toString(ADexFile_Error self) {
  switch (self) {
    case ADEXFILE_ERROR_OK: return "Ok";
    case ADEXFILE_ERROR_INVALID_DEX: return "Dex file is invalid.";
    case ADEXFILE_ERROR_NOT_ENOUGH_DATA: return "Not enough data. Incomplete dex file.";
    case ADEXFILE_ERROR_INVALID_HEADER: return "Invalid dex file header.";
  }
  return nullptr;
}

}  // extern "C"

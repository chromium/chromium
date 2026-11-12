/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "dex_file_loader.h"

#include <sys/stat.h>

#include <memory>
#include <optional>

#include "android-base/stringprintf.h"
#include "base/bit_utils.h"
#include "base/file_magic.h"
#include "base/mem_map.h"
#include "base/os.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "compact_dex_file.h"
#include "dex_file.h"
#include "dex_file_verifier.h"
#include "standard_dex_file.h"

namespace art {
namespace {

using android::base::StringPrintf;

class VectorContainer : public DexFileContainer {
 public:
  explicit VectorContainer(std::vector<uint8_t>&& vector) : vector_(std::move(vector)) { }
  ~VectorContainer() override { }

  bool IsReadOnly() const override { return true; }

  bool EnableWrite() override { return true; }

  bool DisableWrite() override { return false; }

  const uint8_t* Begin() const override { return vector_.data(); }

  const uint8_t* End() const override { return vector_.data() + vector_.size(); }

 private:
  std::vector<uint8_t> vector_;
  DISALLOW_COPY_AND_ASSIGN(VectorContainer);
};

class MemMapContainer : public DexFileContainer {
 public:
  explicit MemMapContainer(MemMap&& mem_map, bool is_file_map = false)
      : mem_map_(std::move(mem_map)), is_file_map_(is_file_map) {}

  int GetPermissions() const {
    if (!mem_map_.IsValid()) {
      return 0;
    } else {
      return mem_map_.GetProtect();
    }
  }

  bool IsReadOnly() const override { return GetPermissions() == PROT_READ; }

  bool EnableWrite() override {
    if (!IsReadOnly()) {
      // We can already write to the container.
      // This method may be called multiple times by tests if DexFiles share container.
      return true;
    }
    if (!mem_map_.IsValid()) {
      return false;
    } else {
      return mem_map_.Protect(PROT_READ | PROT_WRITE);
    }
  }

  bool DisableWrite() override {
    CHECK(!IsReadOnly());
    if (!mem_map_.IsValid()) {
      return false;
    } else {
      return mem_map_.Protect(PROT_READ);
    }
  }

  const uint8_t* Begin() const override { return mem_map_.Begin(); }

  const uint8_t* End() const override { return mem_map_.End(); }

  bool IsFileMap() const override { return is_file_map_; }

 protected:
  MemMap mem_map_;
  bool is_file_map_;
  DISALLOW_COPY_AND_ASSIGN(MemMapContainer);
};

}  // namespace

bool DexFileLoader::IsMagicValid(uint32_t magic) {
  return IsMagicValid(reinterpret_cast<uint8_t*>(&magic));
}

bool DexFileLoader::IsMagicValid(const uint8_t* magic) {
  return StandardDexFile::IsMagicValid(magic) ||
      CompactDexFile::IsMagicValid(magic);
}

bool DexFileLoader::IsVersionAndMagicValid(const uint8_t* magic) {
  if (StandardDexFile::IsMagicValid(magic)) {
    return StandardDexFile::IsVersionValid(magic);
  }
  if (CompactDexFile::IsMagicValid(magic)) {
    return CompactDexFile::IsVersionValid(magic);
  }
  return false;
}

bool DexFileLoader::IsMultiDexLocation(std::string_view location) {
  return location.find(kMultiDexSeparator) != std::string_view::npos;
}

std::string DexFileLoader::GetMultiDexClassesDexName(size_t index) {
  return (index == 0) ? "classes.dex" : StringPrintf("classes%zu.dex", index + 1);
}

std::string DexFileLoader::GetMultiDexLocation(size_t index, const char* dex_location) {
  DCHECK(!IsMultiDexLocation(dex_location));
  if (index == 0) {
    return dex_location;
  }
  return StringPrintf("%s%cclasses%zu.dex", dex_location, kMultiDexSeparator, index + 1);
}

bool DexFileLoader::GetMultiDexChecksum(std::optional<uint32_t>* checksum,
                                        std::string* error_msg,
                                        bool* only_contains_uncompressed_dex) {
  CHECK(checksum != nullptr);
  checksum->reset();  // Return nullopt for an empty zip archive.

  uint32_t magic;
  if (!InitAndReadMagic(/*header_offset=*/0, &magic, error_msg)) {
    return false;
  }

  if (IsZipMagic(magic)) {
    return false;
  }
  if (!MapRootContainer(error_msg)) {
    return false;
  }
  const uint8_t* begin = root_container_->Begin();
  const uint8_t* end = root_container_->End();
  for (const uint8_t* ptr = begin; ptr < end;) {
    const auto* header = reinterpret_cast<const DexFile::Header*>(ptr);
    size_t size = dchecked_integral_cast<size_t>(end - ptr);
    if (size < sizeof(*header) || !IsMagicValid(ptr)) {
      *error_msg = StringPrintf("Invalid dex header: '%s'", filename_.c_str());
      return false;
    }
    if (size < header->file_size_) {
      *error_msg = StringPrintf("Truncated dex file: '%s'", filename_.c_str());
      return false;
    }
    *checksum = checksum->value_or(kEmptyMultiDexChecksum) ^ header->checksum_;
    ptr += header->file_size_;
  }
  return true;
}

std::string DexFileLoader::GetDexCanonicalLocation(const char* dex_location) {
  CHECK_NE(dex_location, static_cast<const char*>(nullptr));
  std::string base_location = GetBaseLocation(dex_location);
  const char* suffix = dex_location + base_location.size();
  DCHECK(suffix[0] == 0 || suffix[0] == kMultiDexSeparator);
#ifdef _WIN32
  // Warning: No symbolic link processing here.
  PLOG(WARNING) << "realpath is unsupported on Windows.";
#else
  // Warning: Bionic implementation of realpath() allocates > 12KB on the stack.
  // Do not run this code on a small stack, e.g. in signal handler.
  UniqueCPtr<const char[]> path(realpath(base_location.c_str(), nullptr));
  if (path != nullptr && path.get() != base_location) {
    return std::string(path.get()) + suffix;
  }
#endif
  if (suffix[0] == 0) {
    return base_location;
  } else {
    return dex_location;
  }
}

// All of the implementations here should be independent of the runtime.

DexFileLoader::DexFileLoader(const uint8_t* base, size_t size, const std::string& location)
    : DexFileLoader(std::make_shared<MemoryDexFileContainer>(base, base + size), location) {}

DexFileLoader::DexFileLoader(std::vector<uint8_t>&& memory, const std::string& location)
    : DexFileLoader(std::make_shared<VectorContainer>(std::move(memory)), location) {}

DexFileLoader::DexFileLoader(MemMap&& mem_map, const std::string& location)
    : DexFileLoader(std::make_shared<MemMapContainer>(std::move(mem_map)), location) {}

std::unique_ptr<const DexFile> DexFileLoader::OpenOne(size_t header_offset,
                                                      uint32_t location_checksum,
                                                      const OatDexFile* oat_dex_file,
                                                      bool verify,
                                                      bool verify_checksum,
                                                      std::string* error_msg) {
  uint32_t magic;
  if (!InitAndReadMagic(header_offset, &magic, error_msg) || !MapRootContainer(error_msg)) {
    DCHECK(!error_msg->empty());
    return {};
  }
  DCHECK(root_container_ != nullptr);
  DCHECK_LE(header_offset, root_container_->Size());
  std::unique_ptr<const DexFile> dex_file = OpenCommon(root_container_,
                                                       root_container_->Begin() + header_offset,
                                                       root_container_->Size() - header_offset,
                                                       location_,
                                                       location_checksum,
                                                       oat_dex_file,
                                                       verify,
                                                       verify_checksum,
                                                       error_msg,
                                                       nullptr);
  return dex_file;
}

bool DexFileLoader::InitAndReadMagic(size_t header_offset,
                                     uint32_t* magic,
                                     std::string* error_msg) {
  if (root_container_ != nullptr) {
    if (root_container_->Size() < header_offset ||
        root_container_->Size() - header_offset < sizeof(uint32_t)) {
      *error_msg = StringPrintf("Unable to open '%s' : Size is too small", location_.c_str());
      return false;
    }
    *magic = *reinterpret_cast<const uint32_t*>(root_container_->Begin() + header_offset);
  } else {
    // Open the file if we have not been given the file-descriptor directly before.
    if (!file_) {
      CHECK(!filename_.empty());
      owned_file_ = File(filename_, O_RDONLY, /* check_usage= */ false);
      if (!owned_file_->IsValid()) {
        *error_msg = StringPrintf("Unable to open '%s' : %s", filename_.c_str(), strerror(errno));
        return false;
      }
      file_ = &owned_file_.value();
    }
    CHECK_EQ(header_offset, 0u);  // We always expect to read from the start of physical file.
    if (!ReadMagicAndReset(file_->Fd(), magic, error_msg)) {
      return false;
    }
  }
  return true;
}

bool DexFileLoader::MapRootContainer(std::string* error_msg) {
  if (root_container_ != nullptr) {
    return true;
  }

  CHECK(MemMap::IsInitialized());
  CHECK(file_);
  struct stat sbuf;
  memset(&sbuf, 0, sizeof(sbuf));
  if (fstat(file_->Fd(), &sbuf) == -1) {
    *error_msg = StringPrintf("DexFile: fstat '%s' failed: %s", filename_.c_str(), strerror(errno));
    return false;
  }
  if (S_ISDIR(sbuf.st_mode)) {
    *error_msg = StringPrintf("Attempt to mmap directory '%s'", filename_.c_str());
    return false;
  }
  MemMap map = MemMap::MapFile(sbuf.st_size,
                               PROT_READ,
                               MAP_PRIVATE,
                               file_->Fd(),
                               0,
                               /*low_4gb=*/false,
                               filename_.c_str(),
                               error_msg);
  if (!map.IsValid()) {
    DCHECK(!error_msg->empty());
    return false;
  }
  root_container_ = std::make_shared<MemMapContainer>(std::move(map));
  return true;
}

bool DexFileLoader::Open(bool verify,
                         bool verify_checksum,
                         bool allow_no_dex_files,
                         DexFileLoaderErrorCode* error_code,
                         std::string* error_msg,
                         std::vector<std::unique_ptr<const DexFile>>* dex_files) {
  DCHECK(dex_files != nullptr) << "DexFile::Open: out-param is nullptr";

  uint32_t magic;
  if (!InitAndReadMagic(/*header_offset=*/0, &magic, error_msg)) {
    return false;
  }

  if (IsZipMagic(magic)) {
    return false;
  }
  if (IsMagicValid(magic)) {
    if (!MapRootContainer(error_msg)) {
      return false;
    }
    DCHECK(root_container_ != nullptr);
    size_t header_offset = 0;
    for (size_t i = 0;; i++) {
      std::string multidex_location = GetMultiDexLocation(i, location_.c_str());
      std::unique_ptr<const DexFile> dex_file =
          OpenCommon(root_container_,
                     root_container_->Begin() + header_offset,
                     root_container_->Size() - header_offset,
                     multidex_location,
                     /*location_checksum*/ {},  // Use default checksum from dex header.
                     /*oat_dex_file=*/nullptr,
                     verify,
                     verify_checksum,
                     error_msg,
                     error_code);
      if (dex_file == nullptr) {
        return false;
      }
      dex_files->push_back(std::move(dex_file));
      size_t file_size = dex_files->back()->GetHeader().file_size_;
      CHECK_LE(file_size, root_container_->Size() - header_offset);
      header_offset += file_size;
      if (dex_files->back()->IsDexContainerLastEntry()) {
        break;
      }
    }
    return true;
  }
  *error_msg = StringPrintf("Expected valid zip or dex file");
  return false;
}

std::unique_ptr<DexFile> DexFileLoader::OpenCommon(std::shared_ptr<DexFileContainer> container,
                                                   const uint8_t* base,
                                                   size_t app_compat_size,
                                                   const std::string& location,
                                                   std::optional<uint32_t> location_checksum,
                                                   const OatDexFile* oat_dex_file,
                                                   bool verify,
                                                   bool verify_checksum,
                                                   std::string* error_msg,
                                                   DexFileLoaderErrorCode* error_code) {
  if (container == nullptr) {
    // We should never pass null here, but use reasonable default for app compat anyway.
    container = std::make_shared<MemoryDexFileContainer>(base, app_compat_size);
  }
  CHECK_GE(base, container->Begin());
  CHECK_LE(base, container->End());
  const size_t size = container->End() - base;
  if (error_code != nullptr) {
    *error_code = DexFileLoaderErrorCode::kDexFileError;
  }
  std::unique_ptr<DexFile> dex_file;
  auto header = reinterpret_cast<const DexFile::Header*>(base);
  if (size >= sizeof(StandardDexFile::Header) && StandardDexFile::IsMagicValid(base)) {
    uint32_t checksum = location_checksum.value_or(header->checksum_);
    dex_file.reset(new StandardDexFile(base, location, checksum, oat_dex_file, container));
  } else if (size >= sizeof(CompactDexFile::Header) && CompactDexFile::IsMagicValid(base)) {
    uint32_t checksum = location_checksum.value_or(header->checksum_);
    dex_file.reset(new CompactDexFile(base, location, checksum, oat_dex_file, container));
  } else {
    *error_msg = StringPrintf("Invalid or truncated dex file '%s'", location.c_str());
  }
  if (dex_file == nullptr) {
    *error_msg =
        StringPrintf("Failed to open dex file '%s': %s", location.c_str(), error_msg->c_str());
    return nullptr;
  }
  if (!dex_file->Init(error_msg)) {
    dex_file.reset();
    return nullptr;
  }
  // NB: Dex verifier does not understand the compact dex format.
  if (verify && !dex_file->IsCompactDexFile()) {
    if (!dex::Verify(dex_file.get(), location.c_str(), verify_checksum, error_msg)) {
      if (error_code != nullptr) {
        *error_code = DexFileLoaderErrorCode::kVerifyError;
      }
      return nullptr;
    }
  }
  if (error_code != nullptr) {
    *error_code = DexFileLoaderErrorCode::kNoError;
  }
  return dex_file;
}

bool DexFileLoader::OpenFromZipEntry(const ZipArchive& zip_archive,
                                     const char* entry_name,
                                     const std::string& location,
                                     bool verify,
                                     bool verify_checksum,
                                     size_t* multidex_count,
                                     DexFileLoaderErrorCode* error_code,
                                     std::string* error_msg,
                                     std::vector<std::unique_ptr<const DexFile>>* dex_files) const {
  return false;
}

std::unique_ptr<const DexFile> DexFileLoader::Open(
    const uint8_t* base,
    size_t size,
    const std::string& location,
    uint32_t location_checksum,
    const OatDexFile* oat_dex_file,
    bool verify,
    bool verify_checksum,
    std::string* error_msg,
    std::unique_ptr<DexFileContainer> container) const {
  return OpenCommon(base,
                    size,
                    /*data_base=*/nullptr,
                    /*data_size=*/0,
                    location,
                    location_checksum,
                    oat_dex_file,
                    verify,
                    verify_checksum,
                    error_msg,
                    std::move(container),
                    /*verify_result=*/nullptr);
}

std::unique_ptr<DexFile> DexFileLoader::OpenCommon(const uint8_t* base,
                                                   size_t size,
                                                   const uint8_t* data_base,
                                                   size_t data_size,
                                                   const std::string& location,
                                                   uint32_t location_checksum,
                                                   const OatDexFile* oat_dex_file,
                                                   bool verify,
                                                   bool verify_checksum,
                                                   std::string* error_msg,
                                                   std::unique_ptr<DexFileContainer> old_container,
                                                   VerifyResult* verify_result) {
  CHECK(data_base == base || data_base == nullptr);
  CHECK(data_size == size || data_size == 0);
  CHECK(verify_result == nullptr);

  // The provided container probably does implent the new API.
  // We don't use it, but let's at least call its destructor.
  struct NewContainer : public MemoryDexFileContainer {
    using MemoryDexFileContainer::MemoryDexFileContainer;  // ctor.
    std::unique_ptr<DexFileContainer> old_container_ = nullptr;
  };
  auto new_container = std::make_shared<NewContainer>(base, size);
  new_container->old_container_ = std::move(old_container);

  return OpenCommon(std::move(new_container),
                    base,
                    size,
                    location,
                    location_checksum,
                    oat_dex_file,
                    verify,
                    verify_checksum,
                    error_msg,
                    /*error_code=*/nullptr);
}

}  // namespace art

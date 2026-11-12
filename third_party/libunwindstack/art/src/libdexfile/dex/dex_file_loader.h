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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_LOADER_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_LOADER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "dex_file.h"

namespace art {

class MemMap;
class OatDexFile;
class ZipArchive;

enum class DexFileLoaderErrorCode {
  kNoError,
  kEntryNotFound,
  kExtractToMemoryError,
  kDexFileError,
  kMakeReadOnlyError,
  kVerifyError
};

// Class that is used to open dex files and deal with corresponding multidex and location logic.
class DexFileLoader {
 public:
  // name of the DexFile entry within a zip archive
  static constexpr const char* kClassesDex = "classes.dex";

  // The separator character in MultiDex locations.
  static constexpr char kMultiDexSeparator = '!';

  // Return true if the magic is valid for dex or cdex.
  static bool IsMagicValid(uint32_t magic);
  static bool IsMagicValid(const uint8_t* magic);

  // Return true if the corresponding version and magic is valid.
  static bool IsVersionAndMagicValid(const uint8_t* magic);

  // Check whether a location denotes a multidex dex file. This is a very simple check: returns
  // whether the string contains the separator character.
  static bool IsMultiDexLocation(std::string_view location);

  // Return the name of the index-th classes.dex in a multidex zip file. This is classes.dex for
  // index == 0, and classes{index + 1}.dex else.
  static std::string GetMultiDexClassesDexName(size_t index);

  // Return the (possibly synthetic) dex location for a multidex entry. This is dex_location for
  // index == 0, and dex_location + multi-dex-separator + GetMultiDexClassesDexName(index) else.
  static std::string GetMultiDexLocation(size_t index, const char* dex_location);

  // Returns combined checksum of one or more dex files (one checksum for the whole multidex set).
  //
  // This uses the source path provided to DexFileLoader constructor.
  //
  // Returns false on error.  Sets *checksum to nullopt for an empty set.
  bool GetMultiDexChecksum(/*out*/ std::optional<uint32_t>* checksum,
                           /*out*/ std::string* error_msg,
                           /*out*/ bool* only_contains_uncompressed_dex = nullptr);

  // Returns combined checksum of one or more dex files (one checksum for the whole multidex set).
  //
  // This uses already open dex files.
  //
  // It starts iteration at index 'i', which must be a primary dex file,
  // and it sets 'i' to the next primary dex file or to end of the array.
  template <typename DexFilePtrVector>  // array|vector<unique_ptr|DexFile|OatDexFile*>.
  static uint32_t GetMultiDexChecksum(const DexFilePtrVector& dex_files,
                                      /*inout*/ size_t* i) {
    CHECK_LT(*i, dex_files.size()) << "No dex files";
    std::optional<uint32_t> checksum;
    for (; *i < dex_files.size(); ++(*i)) {
      const auto* dex_file = &*dex_files[*i];
      bool is_primary_dex = !IsMultiDexLocation(dex_file->GetLocation().c_str());
      if (!checksum.has_value()) {                         // First dex file.
        CHECK(is_primary_dex) << dex_file->GetLocation();  // Expect primary dex.
      } else if (is_primary_dex) {                         // Later dex file.
        break;  // Found another primary dex file, terminate iteration.
      }
      if (!is_primary_dex && dex_file->GetDexVersion() >= DexFile::kDexContainerVersion) {
        if (dex_file->GetLocationChecksum() == dex_files[*i - 1]->GetLocationChecksum() + 1) {
          continue;
        }
      }
      checksum = checksum.value_or(kEmptyMultiDexChecksum) ^ dex_file->GetLocationChecksum();
    }
    CHECK(checksum.has_value());
    return checksum.value();
  }

  // Calculate checksum of dex files in a vector, starting at index 0.
  // It will CHECK that the whole vector is consumed (i.e. there is just one primary dex file).
  template <typename DexFilePtrVector>
  static uint32_t GetMultiDexChecksum(const DexFilePtrVector& dex_files) {
    size_t i = 0;
    uint32_t checksum = GetMultiDexChecksum(dex_files, &i);
    CHECK_EQ(i, dex_files.size());
    return checksum;
  }

  // Non-zero initial value for multi-dex to catch bugs if multi-dex checksum is compared
  // directly to DexFile::GetLocationChecksum without going through GetMultiDexChecksum.
  static constexpr uint32_t kEmptyMultiDexChecksum = 1;

  // Returns the canonical form of the given dex location.
  //
  // There are different flavors of "dex locations" as follows:
  // the file name of a dex file:
  //     The actual file path that the dex file has on disk.
  // dex_location:
  //     This acts as a key for the class linker to know which dex file to load.
  //     It may correspond to either an old odex file or a particular dex file
  //     inside an oat file. In the first case it will also match the file name
  //     of the dex file. In the second case (oat) it will include the file name
  //     and possibly some multidex annotation to uniquely identify it.
  // canonical_dex_location:
  //     the dex_location where its file name part has been made canonical.
  static std::string GetDexCanonicalLocation(const char* dex_location);

  // For normal dex files, location and base location coincide. If a dex file is part of a multidex
  // archive, the base location is the name of the originating jar/apk, stripped of any internal
  // classes*.dex path.
  static std::string GetBaseLocation(const char* location) {
    const char* pos = strrchr(location, kMultiDexSeparator);
    return (pos == nullptr) ? location : std::string(location, pos - location);
  }

  static std::string GetBaseLocation(const std::string& location) {
    return GetBaseLocation(location.c_str());
  }

  // Returns the '!classes*.dex' part of the dex location. Returns an empty
  // string if there is no multidex suffix for the given location.
  // The kMultiDexSeparator is included in the returned suffix.
  static std::string GetMultiDexSuffix(const std::string& location) {
    size_t pos = location.rfind(kMultiDexSeparator);
    return (pos == std::string::npos) ? std::string() : location.substr(pos);
  }

  DexFileLoader(const char* filename, const File* file, const std::string& location)
      : filename_(filename), file_(file), location_(location) {
  }

  DexFileLoader(std::shared_ptr<DexFileContainer> container, const std::string& location)
      : root_container_(std::move(container)), location_(location) {
    CHECK(root_container_ != nullptr);
  }

  DexFileLoader(const uint8_t* base, size_t size, const std::string& location);

  DexFileLoader(std::vector<uint8_t>&& memory, const std::string& location);

  DexFileLoader(MemMap&& mem_map, const std::string& location);

  DexFileLoader(File* file, const std::string& location)
      : DexFileLoader(/*filename=*/location.c_str(), file, location) {}

  DexFileLoader(const char* filename, const std::string& location)
      : DexFileLoader(filename, /*file=*/nullptr, location) {}

  explicit DexFileLoader(const std::string& location)
      : DexFileLoader(location.c_str(), /*file=*/nullptr, location) {}

  virtual ~DexFileLoader() {}

  // Open singe dex file at the given offset within the container (usually 0).
  // This intentionally ignores all other dex files in the container
  std::unique_ptr<const DexFile> OpenOne(size_t header_offset,
                                         uint32_t location_checksum,
                                         const OatDexFile* oat_dex_file,
                                         bool verify,
                                         bool verify_checksum,
                                         std::string* error_msg);

  // Open single dex file (starting at offset 0 of the container).
  // It expects only single dex file to be present and will fail otherwise.
  std::unique_ptr<const DexFile> Open(uint32_t location_checksum,
                                      const OatDexFile* oat_dex_file,
                                      bool verify,
                                      bool verify_checksum,
                                      std::string* error_msg) {
    std::unique_ptr<const DexFile> dex_file = OpenOne(
        /*header_offset=*/0, location_checksum, oat_dex_file, verify, verify_checksum, error_msg);
    // This API returns only singe DEX file, so check there is just single dex in the container.
    CHECK(dex_file == nullptr || dex_file->IsDexContainerLastEntry()) << location_;
    return dex_file;
  }

  std::unique_ptr<const DexFile> Open(uint32_t location_checksum,
                                      bool verify,
                                      bool verify_checksum,
                                      std::string* error_msg) {
    return Open(location_checksum,
                /*oat_dex_file=*/nullptr,
                verify,
                verify_checksum,
                error_msg);
  }

  // Opens all dex files, guessing the container format based on file magic.
  bool Open(bool verify,
            bool verify_checksum,
            bool allow_no_dex_files,
            DexFileLoaderErrorCode* error_code,
            std::string* error_msg,
            std::vector<std::unique_ptr<const DexFile>>* dex_files);

  bool Open(bool verify,
            bool verify_checksum,
            DexFileLoaderErrorCode* error_code,
            std::string* error_msg,
            std::vector<std::unique_ptr<const DexFile>>* dex_files) {
    return Open(verify,
                verify_checksum,
                /*allow_no_dex_files=*/false,
                error_code,
                error_msg,
                dex_files);
  }

  bool Open(bool verify,
            bool verify_checksum,
            bool allow_no_dex_files,
            std::string* error_msg,
            std::vector<std::unique_ptr<const DexFile>>* dex_files) {
    DexFileLoaderErrorCode error_code;
    return Open(verify, verify_checksum, allow_no_dex_files, &error_code, error_msg, dex_files);
  }

  bool Open(bool verify,
            bool verify_checksum,
            std::string* error_msg,
            std::vector<std::unique_ptr<const DexFile>>* dex_files) {
    DexFileLoaderErrorCode error_code;
    return Open(verify,
                verify_checksum,
                /*allow_no_dex_files=*/false,
                &error_code,
                error_msg,
                dex_files);
  }

 protected:
  bool InitAndReadMagic(size_t header_offset, uint32_t* magic, std::string* error_msg);

  // Ensure we have root container.  If we are backed by a file, memory-map it.
  // We can only do this for dex files since zip files might be too big to map.
  bool MapRootContainer(std::string* error_msg);

  static std::unique_ptr<DexFile> OpenCommon(std::shared_ptr<DexFileContainer> container,
                                             const uint8_t* base,
                                             size_t size,
                                             const std::string& location,
                                             std::optional<uint32_t> location_checksum,
                                             const OatDexFile* oat_dex_file,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg,
                                             DexFileLoaderErrorCode* error_code);

  // Old signature preserved for app-compat.
  std::unique_ptr<const DexFile> Open(const uint8_t* base,
                                      size_t size,
                                      const std::string& location,
                                      uint32_t location_checksum,
                                      const OatDexFile* oat_dex_file,
                                      bool verify,
                                      bool verify_checksum,
                                      std::string* error_msg,
                                      std::unique_ptr<DexFileContainer> container) const;

  // Old signature preserved for app-compat.
  enum VerifyResult {};
  static std::unique_ptr<DexFile> OpenCommon(const uint8_t* base,
                                             size_t size,
                                             const uint8_t* data_base,
                                             size_t data_size,
                                             const std::string& location,
                                             uint32_t location_checksum,
                                             const OatDexFile* oat_dex_file,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg,
                                             std::unique_ptr<DexFileContainer> container,
                                             VerifyResult* verify_result);

  // Open .dex files from the entry_name in a zip archive.
  bool OpenFromZipEntry(const ZipArchive& zip_archive,
                        const char* entry_name,
                        const std::string& location,
                        bool verify,
                        bool verify_checksum,
                        /*inout*/ size_t* multidex_count,
                        /*out*/ DexFileLoaderErrorCode* error_code,
                        /*out*/ std::string* error_msg,
                        /*out*/ std::vector<std::unique_ptr<const DexFile>>* dex_files) const;

  // The DexFileLoader can be backed either by file or by memory (i.e. DexFileContainer).
  // We can not just mmap the file since APKs might be unreasonably large for 32-bit system.
  std::string filename_;
  const File* file_ = nullptr;
  std::optional<File> owned_file_;  // May be used as backing storage for 'file_'.
  std::shared_ptr<DexFileContainer> root_container_;
  const std::string location_;
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_LOADER_H_

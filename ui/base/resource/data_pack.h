// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DataPack represents a read-only view onto an on-disk file that contains
// (key, value) pairs of data.  It's used to store static resources like
// translation strings and images.

#ifndef UI_BASE_RESOURCE_DATA_PACK_H_
#define UI_BASE_RESOURCE_DATA_PACK_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_span.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_handle.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace base {
class FilePath;
class RefCountedStaticMemory;
}

namespace ui {
enum ResourceScaleFactor : int;

class COMPONENT_EXPORT(UI_DATA_PACK) DataPack : public ResourceHandle {
 public:
  explicit DataPack(ResourceScaleFactor resource_scale_factor);

  DataPack(const DataPack&) = delete;
  DataPack& operator=(const DataPack&) = delete;

  ~DataPack() override;

// Pack Entry and Alias. This removes padding between fields, and alignment
// requirements, which makes the structs usable for aliasing into the input
// buffer directly.
//
// TODO(davidben): Ideally we would load these structures through memcpy, or
// a little-endian variant of base/big_endian.h, rather than type-punning
// pointers. This code currently depends on Chromium disabling strict aliasing.
#pragma pack(push, 1)
  struct Entry {
    // ID corresponding with each resources.
    uint16_t resource_id;
    // The offset of the resource in .pak file.
    uint32_t file_offset;
  };
  struct Alias {
    // ID corresponding with each resources.
    uint16_t resource_id;
    // The index of the entry which has the same resource to `resource_id`'s
    // resource.
    uint16_t entry_index;
  };
#pragma pack(pop)

  // Abstraction of a data source (memory mapped file or in-memory buffer).
  class DataSource {
   public:
    virtual ~DataSource() = default;

    virtual base::span<const uint8_t> bytes() const = 0;
  };

  // Load a pack file from |path|, returning false on error. If the final
  // extension of |path| is .gz, the file will be uncompressed and stored in
  // memory owned by |data_source_|. Otherwise the file will be mapped to
  // memory, with the mapping owned by |data_source_|.
  bool LoadFromPath(const base::FilePath& path);

  enum class FailureReason {
    kOpenFile,
    kMapFile,
    kUnzip,
    kIncompleteHeader,
    kBadPakVersion,
    kBadEncodingType,
    kTooShort,
    kBoundsExceeded,
    kOrderingViolation,
    kAliasTableCorrupt,
    kEmptyFile,
  };

  struct ErrorState {
    FailureReason reason;
#if BUILDFLAG(IS_WIN)
    DWORD error;
#else
    int error;
#endif
    base::File::Error file_error;

    friend bool operator==(const ErrorState& lhs,
                           const ErrorState& rhs) = default;
  };

  // As LoadFromPath, but returns an ErrorState on failure.
  base::expected<void, DataPack::ErrorState> LoadFromPathWithError(
      const base::FilePath& path);

  // The static part of the implementation in LoadFromPath().
  static base::expected<std::unique_ptr<DataPack::DataSource>,
                        DataPack::ErrorState>
  LoadFromPathInternal(const base::FilePath& path);

  // Invokes LoadFromFileRegion with the entire contents of |file|. Compressed
  // files are not supported.
  bool LoadFromFile(base::File file);

  // Loads a pack file from |region| of |file|, returning false on error.
  // The file region will be mapped to memory with the mapping owned by
  // |data_source_|.
  bool LoadFromFileRegion(base::File file,
                          const base::MemoryMappedFile::Region& region);

  // Loads a pack file from |buffer|, returning false on error.
  // Data is not copied, |buffer| should stay alive during |DataPack| lifetime.
  bool LoadFromBuffer(base::span<const uint8_t> buffer);

  // Writes a pack file containing |resources| to |path|. If there are any
  // text resources to be written, their encoding must already agree to the
  // |textEncodingType| specified. If no text resources are present, please
  // indicate BINARY.
  static bool WritePack(const base::FilePath& path,
                        const std::map<uint16_t, std::string_view>& resources,
                        TextEncodingType textEncodingType);

  // ResourceHandle implementation:
  bool HasResource(uint16_t resource_id) const override;
  std::optional<std::string_view> GetStringView(
      uint16_t resource_id) const override;
  base::RefCountedStaticMemory* GetStaticMemory(
      uint16_t resource_id) const override;
  TextEncodingType GetTextEncodingType() const override;
  ResourceScaleFactor GetResourceScaleFactor() const override;
#if DCHECK_IS_ON()
  // Checks to see if any resource in this DataPack already exists in the list
  // of resources.
  void CheckForDuplicateResources(
      const std::vector<std::unique_ptr<ResourceHandle>>& packs) override;
#endif

  // Return the size of the alias table.
  size_t GetAliasTableSize() const { return alias_table_.size(); }

  // Return the size of the resource Should only be used for unit-testing
  // (more specifically checking that alias table generation removes entries
  // for the resources table), as this is an implementation detail.
  size_t GetResourceTableSizeForTesting() const { return resource_count(); }

 private:
  class BufferDataSource;
  class MemoryMappedDataSource;
  class StringDataSource;

  // Does the actual loading of a pack file.
  // Called by Load and LoadFromFile and LoadFromBuffer.
  base::expected<void, DataPack::FailureReason> LoadImpl(
      std::unique_ptr<DataSource> data_source);

  // Returns the index into `resource_table_` of the entry for `resource_id`,
  // resolving aliases, or std::nullopt if the pack has no such resource.
  std::optional<size_t> LookupEntryIndexById(uint16_t resource_id) const;

  // The resource table carries one extra sentinel entry at the end, which gives
  // the end offset of the last resource; it is not itself a resource.
  size_t resource_count() const {
    return resource_table_.empty() ? 0u : resource_table_.size() - 1u;
  }
  // The resource table without its trailing sentinel entry. Only these entries
  // are ordered by `resource_id`, so lookups must search this rather than the
  // whole table.
  base::span<const Entry> resource_entries() const {
    return resource_table_.first(resource_count());
  }

  // Sanity check the file. If it passed the check, register `resource_table_`
  // and `alias_table_`.
  // `margin_to_skip` represents the size of the margin in bytes before
  // resource_table information starts.
  // If there is no extra data in data pack, `margin_to_skip` is equal to the
  // length of file header. `resource_count` and `alias_count` are the table
  // lengths declared by the file header.
  base::expected<void, DataPack::FailureReason>
  SanityCheckFileAndRegisterResources(size_t margin_to_skip,
                                      base::span<const uint8_t> data,
                                      size_t resource_count,
                                      size_t alias_count);

  // Owns the bytes that `resource_table_` and `alias_table_` view, so it must
  // be declared before them.
  std::unique_ptr<DataSource> data_source_;

  base::raw_span<const Entry> resource_table_;
  base::raw_span<const Alias> alias_table_;

  // Type of encoding for text resources.
  TextEncodingType text_encoding_type_;

  // The scale of the image in this resource pack relative to images in the 1x
  // resource pak.
  ResourceScaleFactor resource_scale_factor_;
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_DATA_PACK_H_

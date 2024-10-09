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
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/resource/resource_handle.h"

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
    static int CompareById(const void* void_key, const void* void_entry);

    // ID corresponding with each resources.
    uint16_t resource_id;
    // The offset of the resource in .pak file.
    uint32_t file_offset;
  };
  struct Alias {
    static int CompareById(const void* void_key, const void* void_entry);

    // ID corresponding with each resources.
    uint16_t resource_id;
    // The index of the entry which has the same resource to `resource_id`'s
    // resource.
    uint16_t entry_index;
  };
#pragma pack(pop)

  // Pair of resource id and string view data.
  struct ResourceData {
    explicit ResourceData(uint16_t id, std::string_view data)
        : id(id), data(data) {}

    // Resource ID.
    uint16_t id;
    // Resource data.
    std::string_view data;
  };

  // Iterator for ResourceData in `resource_table_`.
  // Note that this Iterator doesn't include Alias members in `alias_table_`.
  class Iterator {
   public:
    Iterator() = default;
    ~Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator& operator=(const Iterator&) = default;

    const ResourceData& operator*() { return *resource_data_; }
    Iterator& operator++() {
      ++entry_;
      UpdateResourceData();
      return *this;
    }
    bool operator==(const Iterator& other) const {
      return entry_ == other.entry_;
    }
    bool operator!=(const Iterator& other) const {
      return entry_ != other.entry_;
    }

   private:
    friend class DataPack;
    explicit Iterator(const uint8_t* data_source, const Entry* entry)
        : data_source_(data_source), entry_(entry) {
      UpdateResourceData();
    }

    void UpdateResourceData();

    raw_ptr<const uint8_t> data_source_;
    raw_ptr<ResourceData> resource_data_;
    raw_ptr<const Entry, AllowPtrArithmetic> entry_;
  };

  Iterator begin() const;
  Iterator end() const;

  // Abstraction of a data source (memory mapped file or in-memory buffer).
  class DataSource {
   public:
    virtual ~DataSource() = default;

    virtual size_t GetLength() const = 0;
    virtual const uint8_t* GetData() const = 0;
  };

  // Load a pack file from |path|, returning false on error. If the final
  // extension of |path| is .gz, the file will be uncompressed and stored in
  // memory owned by |data_source_|. Otherwise the file will be mapped to
  // memory, with the mapping owned by |data_source_|.
  bool LoadFromPath(const base::FilePath& path);

  // The static part of the implementation in LoadFromPath().
  static std::unique_ptr<DataSource> LoadFromPathInternal(
      const base::FilePath& path);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Load a pack file for shared resource from |path|, returning false on error.
  // Similar to LoadFromPath(), but the file format is different.
  bool LoadSharedResourceFromPath(const base::FilePath& path);
#endif

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

  // Return Entry or Alias by index of resource or alias table.
  const Entry* GetEntryByResourceTableIndex(size_t index) const {
    return &resource_table_[index];
  }
  const Alias* GetAliasByAliasTableIndex(size_t index) const {
    return &alias_table_[index];
  }
  // Return the size of the alias table.
  size_t GetAliasTableSize() const { return alias_count_; }

  // Return the size of the resource Should only be used for unit-testing
  // (more specifically checking that alias table generation removes entries
  // for the resources table), as this is an implementation detail.
  size_t GetResourceTableSizeForTesting() const { return resource_count_; }

 private:
  class BufferDataSource;
  class MemoryMappedDataSource;
  class StringDataSource;

  // Does the actual loading of a pack file.
  // Called by Load and LoadFromFile and LoadFromBuffer.
  bool LoadImpl(std::unique_ptr<DataSource> data_source);
  const Entry* LookupEntryById(uint16_t resource_id) const;

  // Sanity check the file. If it passed the check, register `resource_table_`
  // and `alias_table_`.
  // `margin_to_skip` represents the size of the margin in bytes before
  // resource_table information starts.
  // If there is no extra data in data pack, `margin_to_skip` is equal to the
  // length of file header.
  bool SanityCheckFileAndRegisterResources(size_t margin_to_skip,
                                           const uint8_t* data,
                                           size_t data_length);

  // Returns the string between `target_offset` and `next_offset` in data pack.
  static std::string_view GetStringViewFromOffset(uint32_t target_offset,
                                                  uint32_t next_offset,
                                                  const uint8_t* data_source);

  std::unique_ptr<DataSource> data_source_;

  raw_ptr<const Entry, AllowPtrArithmetic> resource_table_;
  size_t resource_count_;
  raw_ptr<const Alias, AllowPtrArithmetic> alias_table_;
  size_t alias_count_;

  // Type of encoding for text resources.
  TextEncodingType text_encoding_type_;

  // The scale of the image in this resource pack relative to images in the 1x
  // resource pak.
  ResourceScaleFactor resource_scale_factor_;
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_DATA_PACK_H_

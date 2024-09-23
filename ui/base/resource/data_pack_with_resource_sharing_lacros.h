// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_DATA_PACK_WITH_RESOURCE_SHARING_LACROS_H_
#define UI_BASE_RESOURCE_DATA_PACK_WITH_RESOURCE_SHARING_LACROS_H_

#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/resource/scoped_file_writer.h"

namespace ui {

// This class is used in lacros to reduce memory usage. The majority
// of the resources that are needed by lacros-chrome ship with ash-chrome.
// To avoid memory mapping similar (large) files (resulting in increased memory
// usage), this class shares resources with ash-chrome. Sharing is
// done by determining which resources are identical and maintaining a
// mapping file that maps from lacros-chrome resource id to ash-chrome
// resource id. Resources that are unique to lacros-chrome are placed
// in a fallback file.
//
// The mapping file is recreated as necessary.
class COMPONENT_EXPORT(UI_DATA_PACK) DataPackWithResourceSharing
    : public ResourceHandle {
 public:
  explicit DataPackWithResourceSharing(
      ResourceScaleFactor resource_scale_factor);

  DataPackWithResourceSharing(const DataPackWithResourceSharing&) = delete;
  DataPackWithResourceSharing& operator=(const DataPackWithResourceSharing&) =
      delete;

  ~DataPackWithResourceSharing() override;

  // Data of Lacros version and timestamp.
  struct LacrosVersionData {
    // The size of version components is 4. Version of Lacros. Each element
    // corredponds to major, minor, build and patch of CHROME_VERSION.
    static constexpr size_t kVersionComponentsSize = 4;

    // Version of Lacros.
    uint32_t version[kVersionComponentsSize];

    // Last updated time of lacros-chrome.
    int64_t timestamp;
  };

  // Maps resource id in Lacros resources to Ash resources.
  // Data assigned to `lacros_resource_id` and `ash_resource_id` must be same.
  struct Mapping {
    Mapping(uint16_t lacros_resource_id, uint16_t ash_resource_id);

    // Comparison function used for sorting.
    static int CompareById(const void* void_key, const void* void_entry);

    // `lacros_resource_id` is applicable in Lacros resources.
    uint16_t lacros_resource_id;
    // `ash_resource_id` is applicable in Ash resources `ash_data_pack_`.
    uint16_t ash_resource_id;
  };

  // Loads a pack file from `shared_resource_path` with ash resources
  // `ash_path`. Each resources for Lacros are included in eitehr of `ash_path`
  // or `shared_resource_path`.
  // Lacros resources are mpped to Ash resources if duplicated. The mapping tble
  // can be obtained from `shared_resource_path`. As for Lacros resources which
  // are not included in Ash resources, add them to `shared_resource_path` as
  // fallback resources.
  // `shared_resource_path` is expected to be already generated from lacros
  // resources and ash resources. If the file was not successfully generated,
  // `is_valid` in `shared_resource_path` is set to 0.
  bool LoadFromPathWithAshResource(const base::FilePath& shared_resource_path,
                                   const base::FilePath& ash_path);

  // ResourceHandle implementation:
  bool HasResource(uint16_t resource_id) const override;
  std::optional<std::string_view> GetStringPiece(
      uint16_t resource_id) const override;
  base::RefCountedStaticMemory* GetStaticMemory(
      uint16_t resource_id) const override;
  TextEncodingType GetTextEncodingType() const override;
  ResourceScaleFactor GetResourceScaleFactor() const override;
#if DCHECK_IS_ON()
  // Checks to see if any resource in this DataPack already exists in the list
  // of resources.
  void CheckForDuplicateResources(
      const std::vector<std::unique_ptr<ResourceHandle>>& handles) override;
#endif

  // If either of |shared_resource_path| is not valid or doesn't exit, generate
  // them from |ash_path| and |lacros_path|.
  static bool MaybeGenerateFallbackAndMapping(
      const base::FilePath& ash_path,
      const base::FilePath& lacros_path,
      const base::FilePath& shared_resource_path,
      ResourceScaleFactor resource_scale_factor);

  // Should only be used for unit-testing, as this is an implementation detail.
  size_t GetMappingTableSizeForTesting() const { return mapping_count_; }
  const Mapping* GetMappingByMappingTableIndexForTesting(size_t index) const {
    return &mapping_table_[index];
  }
  // Generate file with specific data. Shared resource file is expected to be
  // created only by MaybeGenerateFallbackAdnMapping, so DO NOT use this except
  // for unit-testing.
  static bool WriteSharedResourceFileForTesting(
      const base::FilePath& path,
      std::vector<Mapping> mapping = std::vector<Mapping>(),
      std::vector<uint16_t> resource_ids = std::vector<uint16_t>(),
      std::map<uint16_t, uint16_t> aliases = std::map<uint16_t, uint16_t>(),
      std::map<uint16_t, std::string_view> fallback_resources =
          std::map<uint16_t, std::string_view>());

 private:
  // Loads mapping_table_ from mapping file.
  bool LoadMappingTable(const base::FilePath& path);
  // Returns mapped resource ID if |resource_id| is in |mapping_table_|.
  // Return null if not.
  const std::optional<uint16_t> LookupMappingTable(uint16_t resource_id) const;

  // Check the shared resource `path` version is valid. If Lacros version used
  // to generate `path` is not the same with the current Lacros, return false.
  // We consider the versions are the same iff
  // 1. Lacros chrome version is same.
  // 2. Last updated time of Lacros file is same.
  // Here, we check not only chrome version but also the time since there might
  // be a change in Lacros which affects resource file. This is mostly used by
  // developers.
  static bool IsSharedResourceValid(const base::FilePath& path);

  // Writes a version and its updated time of Lacros used to generate file.
  static bool WriteLacrosVersion(ScopedFileWriter& file);
  // Writes a mapping table `mapping` to `file`.
  static bool WriteMappingTable(std::vector<Mapping> mapping,
                                ScopedFileWriter& file);
  // Writes a fallback resource table, alias table and string data to `file`.
  // `margin_to_skip` represents the size of the margin in bytes before
  // resouce_table information starts.
  static bool WriteFallbackResources(
      std::vector<uint16_t> resource_ids,
      std::map<uint16_t, uint16_t> aliases,
      std::map<uint16_t, std::string_view> fallback_resources,
      size_t margin_to_skip,
      ScopedFileWriter& file);
  // Close and delete temp shared resource file used for generating.
  // This should be called when failing during generating a shared resource.
  static void OnFailedToGenerate(ScopedFileWriter& file,
                                 const base::FilePath& shared_resource_path);

  std::unique_ptr<DataPack::DataSource> data_source_;

  // Each Mapping maps lacros resource id to ash resource id if the same data
  // exists in ash resources .pak.
  // Lacros resource id registered in `mapping_table_` as a key should not be
  // included in fallback_data_pack_.
  raw_ptr<const Mapping, AllowPtrArithmetic> mapping_table_;
  size_t mapping_count_ = 0;

  // Stores DataPacks of fallback resources and ash resources for each.
  std::unique_ptr<DataPack> fallback_data_pack_;
  std::unique_ptr<DataPack> ash_data_pack_;

  // The scale of the image in this resource pack relative to images in the 1x
  // resource pak.
  ResourceScaleFactor resource_scale_factor_;
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_DATA_PACK_WITH_RESOURCE_SHARING_LACROS_H_

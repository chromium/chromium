// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/resource/data_pack_with_resource_sharing_lacros.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/version.h"
#include "components/version_info/version_info.h"
#include "ui/base/resource/temporary_shared_resource_path_chromeos.h"

namespace {

static const uint32_t kFileFormatV1 = 1;
#pragma pack(push, 2)
struct FileHeaderV1 {
  uint32_t version;
  uint16_t mapping_count;
  uint16_t fallback_resource_count;
  uint16_t fallback_alias_count;
};
#pragma pack(pop)

}  // namespace

namespace ui {

bool DataPack::LoadSharedResourceFromPath(const base::FilePath& path) {
  std::unique_ptr<DataSource> data_source = LoadFromPathInternal(path);
  if (!data_source)
    return false;

  const uint8_t* data = data_source->GetData();
  size_t data_length = data_source->GetLength();
  // Parse the version and check for truncated header.
  uint32_t version = 0;
  if (data_length > sizeof(version))
    version = reinterpret_cast<const FileHeaderV1*>(data)[0].version;
  size_t header_length = sizeof(FileHeaderV1);
  if (version == 0 || data_length < header_length) {
    // TODO(crbug.com/40221977): Add LogDataPackError as DataPack.
    LOG(ERROR) << "Data pack file corruption: incomplete file header.";
    return false;
  }

  // In shared resource data pack, information of mapping_table is included.
  // Ignore mapping_table since it is stored in DataPackWithResourceSharing.
  size_t mapping_count = 0;

  // Parse the header of the file.
  if (version == kFileFormatV1) {
    FileHeaderV1 header = reinterpret_cast<const FileHeaderV1*>(data)[0];
    mapping_count = header.mapping_count;
    resource_count_ = header.fallback_resource_count;
    alias_count_ = header.fallback_alias_count;
  } else {
    // TODO(crbug.com/40221977): Add LogDataPackError as DataPack.
    LOG(ERROR) << "Bad shared resource data pack version: got " << version
               << ", expected " << kFileFormatV1;
    return false;
  }

  // Sanity check the file.
  size_t mapping_table_size =
      mapping_count * sizeof(DataPackWithResourceSharing::Mapping);
  if (!SanityCheckFileAndRegisterResources(
          header_length + mapping_table_size +
              sizeof(DataPackWithResourceSharing::LacrosVersionData),
          data, data_length)) {
    return false;
  }

  data_source_ = std::move(data_source);
  return true;
}

DataPackWithResourceSharing::Mapping::Mapping(uint16_t lacros_resource_id,
                                              uint16_t ash_resource_id)
    : lacros_resource_id(lacros_resource_id),
      ash_resource_id(ash_resource_id) {}

// static
int DataPackWithResourceSharing::Mapping::CompareById(const void* void_key,
                                                      const void* void_entry) {
  uint16_t key = *reinterpret_cast<const uint16_t*>(void_key);
  const Mapping* entry = reinterpret_cast<const Mapping*>(void_entry);
  return key - entry->lacros_resource_id;
}

DataPackWithResourceSharing::DataPackWithResourceSharing(
    ResourceScaleFactor resource_scale_factor)
    : resource_scale_factor_(resource_scale_factor) {
  static_assert(sizeof(Mapping) == 4, "size of Mapping must be 4");
  static_assert(sizeof(LacrosVersionData) == 24,
                "size of LacrosVersionData must be 24");
}

DataPackWithResourceSharing::~DataPackWithResourceSharing() {}

bool DataPackWithResourceSharing::LoadFromPathWithAshResource(
    const base::FilePath& shared_resource_path,
    const base::FilePath& ash_path) {
  fallback_data_pack_ = std::make_unique<DataPack>(resource_scale_factor_);
  ash_data_pack_ = std::make_unique<DataPack>(resource_scale_factor_);
  if (!fallback_data_pack_->LoadSharedResourceFromPath(shared_resource_path) ||
      !ash_data_pack_->LoadFromPath(ash_path) ||
      !LoadMappingTable(shared_resource_path)) {
    LOG(ERROR) << "Failed to load shared resource or Ash resource data pack.";
    return false;
  }

  return true;
}

bool DataPackWithResourceSharing::LoadMappingTable(const base::FilePath& path) {
  std::unique_ptr<DataPack::DataSource> data_source =
      DataPack::LoadFromPathInternal(path);
  if (!data_source)
    return false;

  const uint8_t* data = data_source->GetData();
  size_t data_length = data_source->GetLength();
  // Parse the version and check for truncated header.
  uint32_t version = 0;
  if (data_length > sizeof(version))
    version = reinterpret_cast<const FileHeaderV1*>(data)[0].version;
  size_t header_length = sizeof(FileHeaderV1);
  if (version == 0 || data_length < header_length) {
    // TODO(crbug.com/40221977): Add LogDataPackError as DataPack.
    LOG(ERROR) << "Data pack file corruption: incomplete file header.";
    return false;
  }

  // Parse the header of the tile.
  if (version == kFileFormatV1) {
    FileHeaderV1 header = reinterpret_cast<const FileHeaderV1*>(data)[0];
    mapping_count_ = header.mapping_count;
  } else {
    // TODO(crbug.com/40221977): Add LogDataPackError as DataPack.
    LOG(ERROR) << "Bad shared resource data pack version: got " << version
               << ", expected " << kFileFormatV1;
    return false;
  }

  // Sanity check the file.
  // 1) Check we have correct number of entries.
  size_t mapping_table_size = mapping_count_ * sizeof(Mapping);
  if (header_length + mapping_table_size > data_length) {
    LOG(ERROR) << "Mapping file corruption: "
               << "too short for number of entries.";
    return false;
  }

  mapping_table_ = reinterpret_cast<const Mapping*>(
      &data[header_length + sizeof(LacrosVersionData)]);

  // 2) Verify mapping table is sorted by lacros resource id and all lacros
  // resource ids are unique.
  if (mapping_count_ > 1) {
    for (size_t i = 0; i < mapping_count_ - 1; ++i) {
      if (mapping_table_[i].lacros_resource_id >
          mapping_table_[i + 1].lacros_resource_id) {
        LOG(ERROR) << "Mapping fie corruption: "
                   << "mapping table is not sorted by lacros resource id.";
        return false;
      } else if (mapping_table_[i].lacros_resource_id ==
                 mapping_table_[i + 1].lacros_resource_id) {
        LOG(ERROR) << "Mapping file corruption: "
                   << "lacros resource id must be unique.";
        return false;
      }
    }
  }

  // 3) Verify the ash resource ids are valid.
  for (size_t i = 0; i < mapping_count_; ++i) {
    if (!ash_data_pack_->HasResource(mapping_table_[i].ash_resource_id)) {
      LOG(ERROR) << "Mapping file corruption: "
                 << "Ash ID " << mapping_table_[i].ash_resource_id
                 << " mapped to Lacros ID "
                 << mapping_table_[i].lacros_resource_id
                 << " is not included in Ash resources.";
      return false;
    }
  }

  data_source_ = std::move(data_source);
  return true;
}

const std::optional<uint16_t> DataPackWithResourceSharing::LookupMappingTable(
    uint16_t resource_id) const {
  // Look up `resource_id` in `mapping_table_`|.
  // If mapped ash resource id is not found, return null.
  const Mapping* ret = reinterpret_cast<const Mapping*>(
      bsearch(&resource_id, mapping_table_, mapping_count_, sizeof(Mapping),
              Mapping::CompareById));
  if (!ret)
    return std::nullopt;

  return ret->ash_resource_id;
}

bool DataPackWithResourceSharing::HasResource(uint16_t resource_id) const {
  std::optional<uint16_t> ash_resource_id = LookupMappingTable(resource_id);
  if (ash_resource_id.has_value()) {
    // If ash resource id is found in mapping table, its resource must exists.
    DCHECK(ash_data_pack_->HasResource(ash_resource_id.value()));
    return true;
  }

  return fallback_data_pack_->HasResource(resource_id);
}

std::optional<std::string_view> DataPackWithResourceSharing::GetStringPiece(
    uint16_t resource_id) const {
  std::optional<uint16_t> ash_resource_id = LookupMappingTable(resource_id);
  if (ash_resource_id.has_value())
    return ash_data_pack_->GetStringPiece(ash_resource_id.value());

  return fallback_data_pack_->GetStringPiece(resource_id);
}

base::RefCountedStaticMemory* DataPackWithResourceSharing::GetStaticMemory(
    uint16_t resource_id) const {
  if (auto piece = GetStringPiece(resource_id); piece.has_value()) {
    return new base::RefCountedStaticMemory(base::as_byte_span(*piece));
  }
  return nullptr;
}

ResourceHandle::TextEncodingType
DataPackWithResourceSharing::GetTextEncodingType() const {
  return BINARY;
}

ResourceScaleFactor DataPackWithResourceSharing::GetResourceScaleFactor()
    const {
  return resource_scale_factor_;
}

#if DCHECK_IS_ON()
void DataPackWithResourceSharing::CheckForDuplicateResources(
    const std::vector<std::unique_ptr<ResourceHandle>>& handles) {
  fallback_data_pack_->CheckForDuplicateResources(handles);
}
#endif  // DCHECK_IS_ON()

// static
bool DataPackWithResourceSharing::IsSharedResourceValid(
    const base::FilePath& path) {
  std::unique_ptr<DataPack::DataSource> data_source =
      DataPack::LoadFromPathInternal(path);
  if (!data_source)
    return false;

  const uint8_t* data = data_source->GetData();
  size_t data_length = data_source->GetLength();
  // Parse the version and check for truncated header.
  uint32_t version = 0;
  if (data_length > sizeof(version))
    version = reinterpret_cast<const FileHeaderV1*>(data)[0].version;
  size_t header_length = sizeof(FileHeaderV1);
  if (version != kFileFormatV1 || data_length < header_length)
    return false;

  // Get LacrosVersionData.
  const LacrosVersionData* lacros_version;
  lacros_version =
      reinterpret_cast<const LacrosVersionData*>(&data[header_length]);

  // Compare `lacros_version` with the current Lacros version.
  std::vector<uint32_t> lacros_version_components(
      lacros_version->version,
      lacros_version->version + LacrosVersionData::kVersionComponentsSize);
  if (base::Version(std::move(lacros_version_components)) !=
      version_info::GetVersion()) {
    return false;
  }

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  base::File::Info file_info;
  if (!GetFileInfo(command_line->GetProgram(), &file_info)) {
    return false;
  }
  if (lacros_version->timestamp !=
      file_info.last_modified.ToDeltaSinceWindowsEpoch().InMicroseconds()) {
    LOG(WARNING) << "Removing cached shared resource and regenerate it since "
                 << "lacros-chrome has been modified after last shared "
                 << "resource generation. This is expected to happen for "
                 << "developers when updating lacros-chrome.";
    return false;
  }

  return true;
}

// static
void DataPackWithResourceSharing::OnFailedToGenerate(
    ScopedFileWriter& file,
    const base::FilePath& shared_resource_path) {
  // Close and delete temp shared resource file.
  file.Close();
  base::FilePath temp_shared_resource_path =
      GetPathForTemporarySharedResourceFile(shared_resource_path);
  // Skip error checking since this DeleteFile() is for abandoning a file which
  // is no longer useful and it won't affect the behavior itself even if
  // DeleteFile() fails.
  base::DeleteFile(temp_shared_resource_path);
}

// static
bool DataPackWithResourceSharing::WriteLacrosVersion(ScopedFileWriter& file) {
  // Write Lacros version and timestamp.
  LacrosVersionData lacros_version;

  // Version components must have 4 elements.
  DCHECK_EQ(version_info::GetVersion().components().size(),
            LacrosVersionData::kVersionComponentsSize);
  for (size_t i = 0; i < LacrosVersionData::kVersionComponentsSize; ++i)
    lacros_version.version[i] = version_info::GetVersion().components()[i];

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  base::File::Info file_info;
  if (!GetFileInfo(command_line->GetProgram(), &file_info)) {
    LOG(WARNING) << "Failed to get last modified time of lacros-chrome.";
    return false;
  }

  lacros_version.timestamp =
      file_info.last_modified.ToDeltaSinceWindowsEpoch().InMicroseconds();
  file.Write(&lacros_version, sizeof(LacrosVersionData));
  return true;
}

// static
bool DataPackWithResourceSharing::WriteMappingTable(
    std::vector<Mapping> mapping_table,
    ScopedFileWriter& file) {
  // Sort `mapping` so that `mapping_table_` can be bsearched.
  std::sort(mapping_table.begin(), mapping_table.end(),
            [](Mapping map1, Mapping map2) {
              return map1.lacros_resource_id < map2.lacros_resource_id;
            });

  if (static_cast<uint16_t>(mapping_table.size()) != mapping_table.size()) {
    LOG(ERROR) << "Too many mappings (" << mapping_table.size() << ")";
    return false;
  }

  for (size_t i = 0; i < mapping_table.size(); ++i)
    file.Write(&mapping_table[i], sizeof(Mapping));
  return true;
}

// static
bool DataPackWithResourceSharing::WriteFallbackResources(
    std::vector<uint16_t> resource_ids,
    std::map<uint16_t, uint16_t> aliases,
    std::map<uint16_t, std::string_view> fallback_resources,
    size_t margin_to_skip,
    ScopedFileWriter& file) {
  DCHECK(std::is_sorted(resource_ids.begin(), resource_ids.end()));

  if (static_cast<uint16_t>(fallback_resources.size()) !=
      fallback_resources.size()) {
    LOG(ERROR) << "Too many resources (" << fallback_resources.size() << ")";
    return false;
  }

  // Each entry is a uint16_t + a uint32_t. We have an extra entry after the
  // last item so we can compute the size of the list item.
  const uint32_t index_length =
      (resource_ids.size() + 1) * sizeof(DataPack::Entry);
  const uint32_t alias_table_length = aliases.size() * sizeof(DataPack::Alias);
  uint32_t data_offset = margin_to_skip + index_length + alias_table_length;
  for (const uint16_t resource_id : resource_ids) {
    file.Write(&resource_id, sizeof(resource_id));
    file.Write(&data_offset, sizeof(data_offset));
    data_offset += fallback_resources.find(resource_id)->second.length();
  }

  // We place an extra entry after the last item that allows us to read the
  // size of the last item.
  const uint16_t extra_resource_id = 0;
  file.Write(&extra_resource_id, sizeof(extra_resource_id));
  file.Write(&data_offset, sizeof(data_offset));

  // Write the aliases table, if any. Note: |aliases| is an std::map,
  // ensuring values are written in increasing order.
  for (const std::pair<const uint16_t, uint16_t>& alias : aliases) {
    file.Write(&alias, sizeof(alias));
  }

  for (const auto& resource_id : resource_ids) {
    const std::string_view data = fallback_resources.find(resource_id)->second;
    file.Write(data.data(), data.length());
  }

  return true;
}

// static
bool DataPackWithResourceSharing::MaybeGenerateFallbackAndMapping(
    const base::FilePath& ash_path,
    const base::FilePath& lacros_path,
    const base::FilePath& shared_resource_path,
    ResourceScaleFactor resource_scale_factor) {
  // Shared resource file should be moved to .temp on Lacros launch by
  // ash-chrome, so `shared_resource_path` must be empty.
  // If this is not empty, it might fail to load resources asynchronously by
  // zygote or utility process.
  DCHECK(!base::PathExists(shared_resource_path));

  // Get the loction of cached shared resource file. This file does not exist
  // if this is the first time to generate it after ash reboot.
  base::FilePath temp_shared_resource_path =
      GetPathForTemporarySharedResourceFile(shared_resource_path);

  // If `temp_shared_resource_path` is already valid, move it back to
  // `shared_resource_path` and skip regenerating.
  if (base::PathExists(temp_shared_resource_path) &&
      IsSharedResourceValid(temp_shared_resource_path) &&
      base::Move(temp_shared_resource_path, shared_resource_path)) {
    return true;
  }

  // Write fallback_resources and mapping_table to `shared_resource_path`.
  // Generate file even if the data generation fails. If the file creation
  // fails, leave the file as empty instead of removing the fie itself.
  // This is required because other processes which try to load the generated
  // file asynchronously will distinguish the cases where the file is under
  // construction or the case where the file generation fails by checking the
  // existence of `shared_resource_path` file.
  ScopedFileWriter file(temp_shared_resource_path);
  if (!file.valid()) {
    LOG(ERROR) << "Failed to open shared resource data pack file. In this "
               << "case, the file does not exist.";
    return false;
  }

  auto ash_data_pack = std::make_unique<DataPack>(resource_scale_factor);
  auto lacros_data_pack = std::make_unique<DataPack>(resource_scale_factor);

  if (!ash_data_pack->LoadFromPath(ash_path) ||
      !lacros_data_pack->LoadFromPath(lacros_path)) {
    LOG(ERROR) << "Failed to load ash or lacros data pack.";
    OnFailedToGenerate(file, shared_resource_path);
    return false;
  }

  std::vector<Mapping> mapping_table;
  std::map<uint16_t, std::string_view> fallback_resources;
  std::unordered_map<std::string_view, uint16_t> ash_resources;
  for (const DataPack::ResourceData& resource_data : *ash_data_pack) {
    ash_resources.emplace(resource_data.data, resource_data.id);
  }

  // Determine if lacros resource Entry exists in ash resources. Add to
  // `mapping_table` if exists. Add to `fallback_resources` instead if not.
  for (const DataPack::ResourceData& resource_data : *lacros_data_pack) {
    auto iter = ash_resources.find(resource_data.data);
    if (iter == ash_resources.end()) {
      fallback_resources[resource_data.id] = resource_data.data;
    } else {
      mapping_table.push_back(Mapping(resource_data.id, iter->second));
    }
  }

  // Determine if lacros resource Alias exists in ash resources similarly to
  // Entry. If the aliased resource id exists in `mapping_tabe` generated above,
  // it implies that its resource is included in ash resources, so add the
  // resource id to `mapping_table` paired with the mapped ash resource id. If
  // not, add the resource id to `fallback_resources` instead paired with the
  // aliased resource data.
  // Note that both mapping table and alias table is sorted by the resource id,
  // so we can check if a resource id exists in mapping table
  size_t mapping_idx = 0;
  size_t alias_idx = 0;
  // Store the current size of mapping_table.
  const size_t mapping_size = mapping_table.size();
  while (mapping_idx < mapping_size &&
         alias_idx < lacros_data_pack->GetAliasTableSize()) {
    uint16_t resource_id =
        lacros_data_pack
            ->GetEntryByResourceTableIndex(
                lacros_data_pack->GetAliasByAliasTableIndex(alias_idx)
                    ->entry_index)
            ->resource_id;

    if (mapping_table[mapping_idx].lacros_resource_id == resource_id) {
      mapping_table.push_back(Mapping(
          lacros_data_pack->GetAliasByAliasTableIndex(alias_idx)->resource_id,
          mapping_table[mapping_idx].ash_resource_id));
      ++mapping_idx;
      ++alias_idx;
    } else if (mapping_table[mapping_idx].lacros_resource_id < resource_id) {
      ++mapping_idx;
    } else {
      if (auto piece = lacros_data_pack->GetStringPiece(resource_id); piece) {
        fallback_resources[lacros_data_pack
                               ->GetAliasByAliasTableIndex(alias_idx)
                               ->resource_id] = piece.value();
      }
      ++alias_idx;
    }
  }

  for (; alias_idx < lacros_data_pack->GetAliasTableSize(); ++alias_idx) {
    if (auto piece = lacros_data_pack->GetStringPiece(
            lacros_data_pack->GetAliasByAliasTableIndex(alias_idx)
                ->resource_id);
        piece) {
      fallback_resources[lacros_data_pack->GetAliasByAliasTableIndex(alias_idx)
                             ->resource_id] = piece.value();
    }
  }

  // Build a list of final resource aliases, and an alias map at the same time.
  std::vector<uint16_t> fallback_resource_ids;
  std::map<uint16_t, uint16_t> fallback_aliases;  // resource_id -> entry_index
  if (fallback_resources.size() > 0) {
    // A reverse map from string pieces to the index of the corresponding
    // original id in the final resource list.
    std::map<std::string_view, uint16_t> rev_map;
    for (const auto& entry : fallback_resources) {
      auto it = rev_map.find(entry.second);
      if (it != rev_map.end()) {
        // Found an alias here!
        fallback_aliases.emplace(entry.first, it->second);
      } else {
        // Found a final resource.
        const auto entry_index =
            static_cast<uint16_t>(fallback_resource_ids.size());
        rev_map.emplace(entry.second, entry_index);
        fallback_resource_ids.push_back(entry.first);
      }
    }
  }

  FileHeaderV1 header;
  header.version = kFileFormatV1;

  // These values are guaranteed to fit in a uint16_t due to the earlier
  // check of |resources_count|.
  header.mapping_count = static_cast<uint16_t>(mapping_table.size());
  header.fallback_resource_count =
      static_cast<uint16_t>(fallback_resource_ids.size());
  header.fallback_alias_count = static_cast<uint16_t>(fallback_aliases.size());

  DCHECK_EQ(static_cast<size_t>(header.fallback_resource_count) +
                static_cast<size_t>(header.fallback_alias_count),
            fallback_resources.size());

  // Write file header.
  file.Write(&header, sizeof(FileHeaderV1));

  // Write Lacros version and timestamp.
  if (!WriteLacrosVersion(file)) {
    OnFailedToGenerate(file, shared_resource_path);
    return false;
  }

  // Write mapping table.
  if (!WriteMappingTable(std::move(mapping_table), file)) {
    OnFailedToGenerate(file, shared_resource_path);
    return false;
  }

  // Write resource table and alias table.
  if (!WriteFallbackResources(std::move(fallback_resource_ids),
                              std::move(fallback_aliases),
                              std::move(fallback_resources),
                              sizeof(FileHeaderV1) + sizeof(LacrosVersionData) +
                                  header.mapping_count * sizeof(Mapping),
                              file)) {
    OnFailedToGenerate(file, shared_resource_path);
    return false;
  }

  // As shared resource file is successfully generated at
  // `temp_shared_resource_path`, move the contents to `shared_resource_path`.
  file.Close();
  return base::Move(temp_shared_resource_path, shared_resource_path);
}

// statoc
bool DataPackWithResourceSharing::WriteSharedResourceFileForTesting(
    const base::FilePath& path,
    std::vector<Mapping> mapping,
    std::vector<uint16_t> resource_ids,
    std::map<uint16_t, uint16_t> aliases,
    std::map<uint16_t, std::string_view> fallback_resources) {
  ScopedFileWriter file(path);
  if (!file.valid()) {
    LOG(ERROR) << "Failed to open path for test: " << path;
    return false;
  }

  // Write file header.
  FileHeaderV1 header;
  header.version = kFileFormatV1;
  header.mapping_count = static_cast<uint16_t>(mapping.size());
  header.fallback_resource_count = static_cast<uint16_t>(resource_ids.size());
  header.fallback_alias_count = static_cast<uint16_t>(aliases.size());
  file.Write(&header, sizeof(FileHeaderV1));

  // Write dummy Lacros version and timestamp.
  if (!WriteLacrosVersion(file)) {
    OnFailedToGenerate(file, path);
    return false;
  }

  // Write mapping table.
  if (!WriteMappingTable(std::move(mapping), file)) {
    OnFailedToGenerate(file, path);
    return false;
  }

  // Write resource table, alias table and string data.
  if (!WriteFallbackResources(std::move(resource_ids), std::move(aliases),
                              std::move(fallback_resources),
                              sizeof(FileHeaderV1) + sizeof(LacrosVersionData) +
                                  mapping.size() * sizeof(Mapping),
                              file)) {
    OnFailedToGenerate(file, path);
    return false;
  }

  return file.Close();
}

}  // namespace ui

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/resource/data_pack.h"

#include <errno.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/synchronization/lock.h"
#include "net/filter/gzip_header.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/resource/scoped_file_writer.h"

// For details of the file layout, see
// http://dev.chromium.org/developers/design-documents/linuxresourcesandlocalizedstrings

namespace {

static const uint32_t kFileFormatV4 = 4;
static const uint32_t kFileFormatV5 = 5;
// uint32(version), uint32(resource_count), uint8(encoding)
static const size_t kHeaderLengthV4 = 2 * sizeof(uint32_t) + sizeof(uint8_t);
// uint32(version), uint8(encoding), 3 bytes padding,
// uint16(resource_count), uint16(alias_count)
static const size_t kHeaderLengthV5 =
    sizeof(uint32_t) + sizeof(uint8_t) * 4 + sizeof(uint16_t) * 2;

// Prints the given resource id the first time it's loaded if Chrome has been
// started with --print-resource-ids. This output is then used to generate a
// more optimal resource renumbering to improve startup speed. See
// tools/gritsettings/README.md for more info.
void MaybePrintResourceId(uint16_t resource_id) {
  static const bool print_resource_ids = [] {
    // This code is run in other binaries than Chrome which do not initialize
    // the CommandLine object. Note: This switch isn't in
    // ui/base/ui_base_switches.h because ui/base depends on ui/base/resource
    // and thus it would cause a circular dependency.
    return base::CommandLine::InitializedForCurrentProcess() &&
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               "print-resource-ids");
  }();
  if (!print_resource_ids)
    return;

  // Note: These are leaked intentionally. However, it's only allocated if the
  // above command line is specified, so it shouldn't affect regular users.
  static std::set<uint16_t>* resource_ids_logged = new std::set<uint16_t>();
  // DataPack doesn't require single-threaded access, so use a lock.
  static base::Lock* lock = new base::Lock;
  base::AutoLock auto_lock(*lock);
  if (!base::Contains(*resource_ids_logged, resource_id)) {
    printf("Resource=%d\n", resource_id);
    resource_ids_logged->insert(resource_id);
  }
}

bool MmapHasGzipHeader(const base::MemoryMappedFile* mmap) {
  net::GZipHeader header;
  const char* header_end = nullptr;
  net::GZipHeader::Status header_status = header.ReadMore(
      reinterpret_cast<const char*>(mmap->data()), mmap->length(), &header_end);
  return header_status == net::GZipHeader::COMPLETE_HEADER;
}

}  // namespace

namespace ui {

// static
int DataPack::Entry::CompareById(const void* void_key, const void* void_entry) {
  uint16_t key = *reinterpret_cast<const uint16_t*>(void_key);
  const Entry* entry = reinterpret_cast<const Entry*>(void_entry);
  return key - entry->resource_id;
}

// static
int DataPack::Alias::CompareById(const void* void_key, const void* void_entry) {
  uint16_t key = *reinterpret_cast<const uint16_t*>(void_key);
  const Alias* entry = reinterpret_cast<const Alias*>(void_entry);
  return key - entry->resource_id;
}

void DataPack::Iterator::UpdateResourceData() {
  const Entry* const next_entry = entry_ + 1;
  resource_data_ = new ResourceData(
      entry_->resource_id,
      GetStringViewFromOffset(entry_->file_offset, next_entry->file_offset,
                              data_source_));
}

DataPack::Iterator DataPack::begin() const {
  return Iterator(data_source_->GetData(), &resource_table_[0]);
}

DataPack::Iterator DataPack::end() const {
  return Iterator(data_source_->GetData(), &resource_table_[resource_count_]);
}

class DataPack::MemoryMappedDataSource : public DataPack::DataSource {
 public:
  explicit MemoryMappedDataSource(std::unique_ptr<base::MemoryMappedFile> mmap)
      : mmap_(std::move(mmap)) {}

  MemoryMappedDataSource(const MemoryMappedDataSource&) = delete;
  MemoryMappedDataSource& operator=(const MemoryMappedDataSource&) = delete;

  ~MemoryMappedDataSource() override {}

  // DataPack::DataSource:
  size_t GetLength() const override { return mmap_->length(); }
  const uint8_t* GetData() const override { return mmap_->data(); }

 private:
  std::unique_ptr<base::MemoryMappedFile> mmap_;
};

// Takes ownership of a string of uncompressed pack data.
class DataPack::StringDataSource : public DataPack::DataSource {
 public:
  explicit StringDataSource(std::string&& data) : data_(std::move(data)) {}

  StringDataSource(const StringDataSource&) = delete;
  StringDataSource& operator=(const StringDataSource&) = delete;

  ~StringDataSource() override {}

  // DataPack::DataSource:
  size_t GetLength() const override { return data_.size(); }
  const uint8_t* GetData() const override {
    return reinterpret_cast<const uint8_t*>(data_.c_str());
  }

 private:
  const std::string data_;
};

class DataPack::BufferDataSource : public DataPack::DataSource {
 public:
  explicit BufferDataSource(base::span<const uint8_t> buffer)
      : buffer_(buffer) {}

  BufferDataSource(const BufferDataSource&) = delete;
  BufferDataSource& operator=(const BufferDataSource&) = delete;

  ~BufferDataSource() override {}

  // DataPack::DataSource:
  size_t GetLength() const override { return buffer_.size(); }
  const uint8_t* GetData() const override { return buffer_.data(); }

 private:
  base::raw_span<const uint8_t> buffer_;
};

DataPack::DataPack(ResourceScaleFactor resource_scale_factor)
    : resource_table_(nullptr),
      resource_count_(0),
      alias_table_(nullptr),
      alias_count_(0),
      text_encoding_type_(BINARY),
      resource_scale_factor_(resource_scale_factor) {
  // Static assert must be within a DataPack member to appease visiblity rules.
  static_assert(sizeof(Entry) == 6, "size of Entry must be 6");
  static_assert(sizeof(Alias) == 4, "size of Alias must be 4");
}

DataPack::~DataPack() {
}

// static
std::unique_ptr<DataPack::DataSource> DataPack::LoadFromPathInternal(
    const base::FilePath& path) {
  std::unique_ptr<base::MemoryMappedFile> mmap =
      std::make_unique<base::MemoryMappedFile>();
  // Open the file for reading; allowing other consumers to also open it for
  // reading and deleting. Do not allow others to write to it.
  base::File data_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                 base::File::FLAG_WIN_SHARE_DELETE);
  if (!data_file.IsValid()) {
    DLOG(ERROR) << "Failed to open datapack with base::File::Error "
                << data_file.error_details();
    return nullptr;
  }
  if (!mmap->Initialize(std::move(data_file))) {
    DLOG(ERROR) << "Failed to mmap datapack";
    return nullptr;
  }
  if (MmapHasGzipHeader(mmap.get())) {
    std::string_view compressed(reinterpret_cast<char*>(mmap->data()),
                                mmap->length());
    std::string data;
    if (!compression::GzipUncompress(compressed, &data)) {
      LOG(ERROR) << "Failed to unzip compressed datapack: " << path;
      return nullptr;
    }
    return std::make_unique<StringDataSource>(std::move(data));
  }
  return std::make_unique<MemoryMappedDataSource>(std::move(mmap));
}

bool DataPack::LoadFromPath(const base::FilePath& path) {
  std::unique_ptr<DataSource> data_source = LoadFromPathInternal(path);
  if (!data_source)
    return false;

  return LoadImpl(std::move(data_source));
}

bool DataPack::LoadFromFile(base::File file) {
  return LoadFromFileRegion(std::move(file),
                            base::MemoryMappedFile::Region::kWholeFile);
}

bool DataPack::LoadFromFileRegion(
    base::File file,
    const base::MemoryMappedFile::Region& region) {
  std::unique_ptr<base::MemoryMappedFile> mmap =
      std::make_unique<base::MemoryMappedFile>();
  if (!mmap->Initialize(std::move(file), region)) {
    DLOG(ERROR) << "Failed to mmap datapack";
    mmap.reset();
    return false;
  }
  return LoadImpl(std::make_unique<MemoryMappedDataSource>(std::move(mmap)));
}

bool DataPack::LoadFromBuffer(base::span<const uint8_t> buffer) {
  return LoadImpl(std::make_unique<BufferDataSource>(buffer));
}

bool DataPack::SanityCheckFileAndRegisterResources(size_t margin_to_skip,
                                                   const uint8_t* data,
                                                   size_t data_length) {
  // 1) Check we have enough entries. There's an extra entry after the last item
  // which gives the length of the last item.
  size_t resource_table_size = (resource_count_ + 1) * sizeof(Entry);
  size_t alias_table_size = alias_count_ * sizeof(Alias);
  if (margin_to_skip + resource_table_size + alias_table_size > data_length) {
    // TODO(crbug.com/40221977): Add more information to LOG. Ditto below.
    LOG(ERROR) << "Data pack file corruption: "
               << "too short for number of entries. "
               << "data length is " << data_length
               << " bytes, expected longer than "
               << margin_to_skip + resource_table_size + alias_table_size
               << " bytes.";
    return false;
  }

  resource_table_ = reinterpret_cast<const Entry*>(&data[margin_to_skip]);
  alias_table_ = reinterpret_cast<const Alias*>(
      &data[margin_to_skip + resource_table_size]);

  // 2) Verify the entries are within the appropriate bounds. There's an extra
  // entry after the last item which gives us the length of the last item.
  for (size_t i = 0; i < resource_count_ + 1; ++i) {
    if (resource_table_[i].file_offset > data_length) {
      LOG(ERROR) << "Data pack file corruption: "
                 << "Entry #" << i << " past end.";
      return false;
    }
  }

  // 3) Verify the entries are ordered correctly.
  for (size_t i = 0; i < resource_count_; ++i) {
    if (resource_table_[i].file_offset > resource_table_[i + 1].file_offset) {
      LOG(ERROR) << "Data pack file corruption: " << "Entry #" << i + 1
                 << " before Entry #" << i << ".";
      return false;
    }
  }

  // 4) Verify the aliases are within the appropriate bounds.
  for (size_t i = 0; i < alias_count_; ++i) {
    if (alias_table_[i].entry_index >= resource_count_) {
      LOG(ERROR) << "Data pack file corruption: "
                 << "Alias #" << i << " past end.";
      return false;
    }
  }

  return true;
}

bool DataPack::LoadImpl(std::unique_ptr<DataPack::DataSource> data_source) {
  const uint8_t* data = data_source->GetData();
  size_t data_length = data_source->GetLength();
  // Parse the version and check for truncated header.
  uint32_t version = 0;
  if (data_length > sizeof(version)) {
    memcpy(&version, data, sizeof(uint32_t));
  }
  size_t header_length =
      version == kFileFormatV4 ? kHeaderLengthV4 : kHeaderLengthV5;
  if (version == 0 || data_length < header_length) {
    DLOG(ERROR) << "Data pack file corruption: incomplete file header.";
    return false;
  }

  // Parse the header of the file.
  if (version == kFileFormatV4) {
    memcpy(&resource_count_, data + 4, sizeof(uint32_t));
    alias_count_ = 0;
    text_encoding_type_ = static_cast<TextEncodingType>(data[8]);
  } else if (version == kFileFormatV5) {
    // Version 5 added the alias table and changed the header format.
    text_encoding_type_ = static_cast<TextEncodingType>(data[4]);
    memcpy(&resource_count_, data + 8, sizeof(uint16_t));
    memcpy(&alias_count_, data + 10, sizeof(uint16_t));
  } else {
    LOG(ERROR) << "Bad data pack version: got " << version << ", expected "
               << kFileFormatV4 << " or " << kFileFormatV5;
    return false;
  }

  if (text_encoding_type_ != UTF8 && text_encoding_type_ != UTF16 &&
      text_encoding_type_ != BINARY) {
    LOG(ERROR) << "Bad data pack text encoding: got " << text_encoding_type_
               << ", expected between " << BINARY << " and " << UTF16;
    return false;
  }

  // Sanity check the file.
  if (!SanityCheckFileAndRegisterResources(header_length, data, data_length))
    return false;

  data_source_ = std::move(data_source);
  return true;
}

const DataPack::Entry* DataPack::LookupEntryById(uint16_t resource_id) const {
  // Search the resource table first as most resources will be in there.
  const Entry* ret = reinterpret_cast<const Entry*>(
      bsearch(&resource_id, resource_table_, resource_count_, sizeof(Entry),
              Entry::CompareById));
  if (ret == nullptr) {
    // Search the alias table for the ~10% of entries which are aliases.
    const Alias* alias = reinterpret_cast<const Alias*>(
        bsearch(&resource_id, alias_table_, alias_count_, sizeof(Alias),
                Alias::CompareById));
    if (alias != nullptr) {
      ret = &resource_table_[alias->entry_index];
    }
  }
  return ret;
}

bool DataPack::HasResource(uint16_t resource_id) const {
  return !!LookupEntryById(resource_id);
}

// static
std::string_view DataPack::GetStringViewFromOffset(uint32_t target_offset,
                                                   uint32_t next_offset,
                                                   const uint8_t* data_source) {
  size_t length = next_offset - target_offset;
  return {reinterpret_cast<const char*>(data_source + target_offset), length};
}

std::optional<std::string_view> DataPack::GetStringView(
    uint16_t resource_id) const {
  const Entry* target = LookupEntryById(resource_id);
  if (!target)
    return std::nullopt;

  const Entry* next_entry = target + 1;
  // If the next entry points beyond the end of the file this data pack's entry
  // table is corrupt. Log an error and return false. See
  // http://crbug.com/371301.
  size_t entry_offset =
      reinterpret_cast<const uint8_t*>(next_entry) - data_source_->GetData();
  size_t pak_size = data_source_->GetLength();
  if (entry_offset > pak_size || next_entry->file_offset > pak_size) {
    size_t entry_index = target - resource_table_;
    LOG(ERROR) << "Entry #" << entry_index << " in data pack points off end "
               << "of file. This should have been caught when loading. Was the "
               << "file modified?";
    return std::nullopt;
  }
  if (target->file_offset > next_entry->file_offset) {
    size_t entry_index = target - resource_table_;
    size_t next_index = next_entry - resource_table_;
    LOG(ERROR) << "Entry #" << next_index << " in data pack is before Entry #"
               << entry_index << ". This should have been caught when loading. "
               << "Was the file modified?";
    return std::nullopt;
  }

  MaybePrintResourceId(resource_id);
  return GetStringViewFromOffset(target->file_offset, next_entry->file_offset,
                                 data_source_->GetData());
}

base::RefCountedStaticMemory* DataPack::GetStaticMemory(
    uint16_t resource_id) const {
  if (auto view = GetStringView(resource_id); view.has_value()) {
    return new base::RefCountedStaticMemory(base::as_byte_span(*view));
  }
  return nullptr;
}

ResourceHandle::TextEncodingType DataPack::GetTextEncodingType() const {
  return text_encoding_type_;
}

ResourceScaleFactor DataPack::GetResourceScaleFactor() const {
  return resource_scale_factor_;
}

#if DCHECK_IS_ON()
void DataPack::CheckForDuplicateResources(
    const std::vector<std::unique_ptr<ResourceHandle>>& packs) {
  for (size_t i = 0; i < resource_count_ + 1; ++i) {
    const uint16_t resource_id = resource_table_[i].resource_id;
    const float resource_scale =
        GetScaleForResourceScaleFactor(resource_scale_factor_);
    for (const auto& handle : packs) {
      if (GetScaleForResourceScaleFactor(handle->GetResourceScaleFactor()) !=
          resource_scale)
        continue;
      DCHECK(!handle->HasResource(resource_id)) << "Duplicate resource "
                                                << resource_id << " with scale "
                                                << resource_scale;
    }
  }
}
#endif  // DCHECK_IS_ON()

// static
bool DataPack::WritePack(const base::FilePath& path,
                         const std::map<uint16_t, std::string_view>& resources,
                         TextEncodingType text_encoding_type) {
  if (text_encoding_type != UTF8 && text_encoding_type != UTF16 &&
      text_encoding_type != BINARY) {
    LOG(ERROR) << "Invalid text encoding type, got " << text_encoding_type
               << ", expected between " << BINARY << " and " << UTF16;
    return false;
  }

  size_t resources_count = resources.size();
  if (static_cast<uint16_t>(resources_count) != resources_count) {
    LOG(ERROR) << "Too many resources (" << resources_count << ")";
    return false;
  }

  ScopedFileWriter file(path);
  if (!file.valid())
    return false;

  uint32_t encoding = static_cast<uint32_t>(text_encoding_type);

  // Build a list of final resource aliases, and an alias map at the same time.
  std::vector<uint16_t> resource_ids;
  std::map<uint16_t, uint16_t> aliases;  // resource_id -> entry_index
  if (resources_count > 0) {
    // A reverse map from string view to the index of the corresponding
    // original id in the final resource list.
    std::map<std::string_view, uint16_t> rev_map;
    for (const auto& entry : resources) {
      auto it = rev_map.find(entry.second);
      if (it != rev_map.end()) {
        // Found an alias here!
        aliases.emplace(entry.first, it->second);
      } else {
        // Found a final resource.
        const auto entry_index = static_cast<uint16_t>(resource_ids.size());
        rev_map.emplace(entry.second, entry_index);
        resource_ids.push_back(entry.first);
      }
    }
  }

  DCHECK(std::is_sorted(resource_ids.begin(), resource_ids.end()));

  // These values are guaranteed to fit in a uint16_t due to the earlier
  // check of |resources_count|.
  const uint16_t alias_count = static_cast<uint16_t>(aliases.size());
  const uint16_t entry_count = static_cast<uint16_t>(resource_ids.size());
  DCHECK_EQ(static_cast<size_t>(entry_count) + static_cast<size_t>(alias_count),
            resources_count);

  file.Write(&kFileFormatV5, sizeof(kFileFormatV5));
  file.Write(&encoding, sizeof(uint32_t));
  file.Write(&entry_count, sizeof(entry_count));
  file.Write(&alias_count, sizeof(alias_count));

  // Each entry is a uint16_t + a uint32_t. We have an extra entry after the
  // last item so we can compute the size of the list item.
  const uint32_t index_length = (entry_count + 1) * sizeof(Entry);
  const uint32_t alias_table_length = alias_count * sizeof(Alias);
  uint32_t data_offset = kHeaderLengthV5 + index_length + alias_table_length;
  for (const uint16_t resource_id : resource_ids) {
    file.Write(&resource_id, sizeof(resource_id));
    file.Write(&data_offset, sizeof(data_offset));
    data_offset += resources.find(resource_id)->second.length();
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
    const std::string_view data = resources.find(resource_id)->second;
    file.Write(data.data(), data.length());
  }

  return file.Close();
}

}  // namespace ui

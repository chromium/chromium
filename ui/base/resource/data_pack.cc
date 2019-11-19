// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/data_pack.h"

#include <errno.h>
#include <algorithm>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/filter/gzip_header.h"
#include "third_party/zlib/google/compression_utils.h"

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

// We're crashing when trying to load a pak file on Windows.  Add some error
// codes for logging.
// http://crbug.com/58056
enum LoadErrors {
  INIT_FAILED = 1,
  BAD_VERSION,
  INDEX_TRUNCATED,
  ENTRY_NOT_FOUND,
  HEADER_TRUNCATED,
  WRONG_ENCODING,
  INIT_FAILED_FROM_FILE,
  UNZIP_FAILED,

  LOAD_ERRORS_COUNT,
};

void LogDataPackError(LoadErrors error) {
  UMA_HISTOGRAM_ENUMERATION("DataPack.Load", error, LOAD_ERRORS_COUNT);
}

// Prints the given resource id the first time it's loaded if Chrome has been
// started with --print-resource-ids. This output is then used to generate a
// more optimal resource renumbering to improve startup speed. See
// tools/gritsettings/README.md for more info.
void MaybePrintResourceId(uint16_t resource_id) {
  // This code is run in other binaries than Chrome which do not initialize the
  // CommandLine object. Early return in those cases.
  if (!base::CommandLine::InitializedForCurrentProcess())
    return;

  // Note: This switch isn't in ui/base/ui_base_switches.h because ui/base
  // depends on ui/base/resource and thus it would cause a circular dependency.
  static bool print_resource_ids =
      base::CommandLine::ForCurrentProcess()->HasSwitch("print-resource-ids");
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

// Convenience class to write data to a file. Usage is the following:
// 1) Create a new instance, passing a base::FilePath.
// 2) Call Write() repeatedly to write all desired data to the file.
// 3) Call valid() whenever you want to know if something failed.
// 4) The file is closed automatically on destruction. Though it is possible
//    to call the Close() method before that.
//
// If an I/O error happens, a PLOG(ERROR) message will be generated, and
// a flag will be set in the writer, telling it to ignore future Write()
// requests. This allows the caller to ignore error handling until the
// very end, as in:
//
//   {
//     base::ScopedFileWriter  writer(<some-path>);
//     writer.Write(&foo, sizeof(foo));
//     writer.Write(&bar, sizeof(bar));
//     ....
//     writer.Write(&zoo, sizeof(zoo));
//     if (!writer.valid()) {
//        // An error happened.
//     }
//   }   // closes the file.
//
class ScopedFileWriter {
 public:
  // Constructor takes a |path| parameter and tries to open the file.
  // Call valid() to check if the operation was succesful.
  explicit ScopedFileWriter(const base::FilePath& path)
      : valid_(true), file_(base::OpenFile(path, "wb")) {
    if (!file_) {
      PLOG(ERROR) << "Could not open pak file for writing";
      valid_ = false;
    }
  }

  // Destructor.
  ~ScopedFileWriter() { Close(); }

  // Return true if the last i/o operation was succesful.
  bool valid() const { return valid_; }

  // Try to write |data_size| bytes from |data| into the file, if a previous
  // operation didn't already failed.
  void Write(const void* data, size_t data_size) {
    if (valid_ && fwrite(data, data_size, 1, file_) != 1) {
      PLOG(ERROR) << "Could not write to pak file";
      valid_ = false;
    }
  }

  // Close the file explicitly. Return true if all previous operations
  // succeeded, including the close, or false otherwise.
  bool Close() {
    if (file_) {
      valid_ = (fclose(file_) == 0);
      file_ = nullptr;
      if (!valid_) {
        PLOG(ERROR) << "Could not close pak file";
      }
    }
    return valid_;
  }

 private:
  bool valid_ = false;
  FILE* file_ = nullptr;  // base::ScopedFILE doesn't check errors on close.

  DISALLOW_COPY_AND_ASSIGN(ScopedFileWriter);
};

bool MmapHasGzipHeader(const base::MemoryMappedFile* mmap) {
  net::GZipHeader header;
  const char* header_end = nullptr;
  net::GZipHeader::Status header_status = header.ReadMore(
      reinterpret_cast<const char*>(mmap->data()), mmap->length(), &header_end);
  return header_status == net::GZipHeader::COMPLETE_HEADER;
}

}  // namespace

namespace ui {

#pragma pack(push, 2)
struct DataPack::Entry {
  uint16_t resource_id;
  uint32_t file_offset;

  static int CompareById(const void* void_key, const void* void_entry) {
    uint16_t key = *reinterpret_cast<const uint16_t*>(void_key);
    const Entry* entry = reinterpret_cast<const Entry*>(void_entry);
    return key - entry->resource_id;
  }
};

struct DataPack::Alias {
  uint16_t resource_id;
  uint16_t entry_index;

  static int CompareById(const void* void_key, const void* void_entry) {
    uint16_t key = *reinterpret_cast<const uint16_t*>(void_key);
    const Alias* entry = reinterpret_cast<const Alias*>(void_entry);
    return key - entry->resource_id;
  }
};
#pragma pack(pop)

// Abstraction of a data source (memory mapped file or in-memory buffer).
class DataPack::DataSource {
 public:
  virtual ~DataSource() {}

  virtual size_t GetLength() const = 0;
  virtual const uint8_t* GetData() const = 0;
};

class DataPack::MemoryMappedDataSource : public DataPack::DataSource {
 public:
  explicit MemoryMappedDataSource(std::unique_ptr<base::MemoryMappedFile> mmap)
      : mmap_(std::move(mmap)) {}

  ~MemoryMappedDataSource() override {}

  // DataPack::DataSource:
  size_t GetLength() const override { return mmap_->length(); }
  const uint8_t* GetData() const override { return mmap_->data(); }

 private:
  std::unique_ptr<base::MemoryMappedFile> mmap_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMappedDataSource);
};

// Takes ownership of a string of uncompressed pack data.
class DataPack::StringDataSource : public DataPack::DataSource {
 public:
  explicit StringDataSource(std::string&& data) : data_(std::move(data)) {}

  ~StringDataSource() override {}

  // DataPack::DataSource:
  size_t GetLength() const override { return data_.size(); }
  const uint8_t* GetData() const override {
    return reinterpret_cast<const uint8_t*>(data_.c_str());
  }

 private:
  const std::string data_;

  DISALLOW_COPY_AND_ASSIGN(StringDataSource);
};

class DataPack::BufferDataSource : public DataPack::DataSource {
 public:
  explicit BufferDataSource(base::StringPiece buffer) : buffer_(buffer) {}

  ~BufferDataSource() override {}

  // DataPack::DataSource:
  size_t GetLength() const override { return buffer_.length(); }
  const uint8_t* GetData() const override {
    return reinterpret_cast<const uint8_t*>(buffer_.data());
  }

 private:
  base::StringPiece buffer_;

  DISALLOW_COPY_AND_ASSIGN(BufferDataSource);
};

DataPack::DataPack(ui::ScaleFactor scale_factor)
    : resource_table_(nullptr),
      resource_count_(0),
      alias_table_(nullptr),
      alias_count_(0),
      text_encoding_type_(BINARY),
      scale_factor_(scale_factor) {
  // Static assert must be within a DataPack member to appease visiblity rules.
  static_assert(sizeof(Entry) == 6, "size of Entry must be 6");
  static_assert(sizeof(Alias) == 4, "size of Alias must be 4");
}

DataPack::~DataPack() {
}

bool DataPack::LoadFromPath(const base::FilePath& path) {
  std::unique_ptr<base::MemoryMappedFile> mmap =
      std::make_unique<base::MemoryMappedFile>();
  if (!mmap->Initialize(path)) {
    DLOG(ERROR) << "Failed to mmap datapack";
    LogDataPackError(INIT_FAILED);
    return false;
  }
  if (MmapHasGzipHeader(mmap.get())) {
    base::StringPiece compressed(reinterpret_cast<char*>(mmap->data()),
                                 mmap->length());
    std::string data;
    if (!compression::GzipUncompress(compressed, &data)) {
      LOG(ERROR) << "Failed to unzip compressed datapack: " << path;
      LogDataPackError(UNZIP_FAILED);
      return false;
    }
    return LoadImpl(std::make_unique<StringDataSource>(std::move(data)));
  }
  return LoadImpl(std::make_unique<MemoryMappedDataSource>(std::move(mmap)));
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
    LogDataPackError(INIT_FAILED_FROM_FILE);
    mmap.reset();
    return false;
  }
  return LoadImpl(std::make_unique<MemoryMappedDataSource>(std::move(mmap)));
}

bool DataPack::LoadFromBuffer(base::StringPiece buffer) {
  return LoadImpl(std::make_unique<BufferDataSource>(buffer));
}

bool DataPack::LoadImpl(std::unique_ptr<DataPack::DataSource> data_source) {
  const uint8_t* data = data_source->GetData();
  size_t data_length = data_source->GetLength();
  // Parse the version and check for truncated header.
  uint32_t version = 0;
  if (data_length > sizeof(version))
    version = reinterpret_cast<const uint32_t*>(data)[0];
  size_t header_length =
      version == kFileFormatV4 ? kHeaderLengthV4 : kHeaderLengthV5;
  if (version == 0 || data_length < header_length) {
    DLOG(ERROR) << "Data pack file corruption: incomplete file header.";
    LogDataPackError(HEADER_TRUNCATED);
    return false;
  }

  // Parse the header of the file.
  if (version == kFileFormatV4) {
    resource_count_ = reinterpret_cast<const uint32_t*>(data)[1];
    alias_count_ = 0;
    text_encoding_type_ = static_cast<TextEncodingType>(data[8]);
  } else if (version == kFileFormatV5) {
    // Version 5 added the alias table and changed the header format.
    text_encoding_type_ = static_cast<TextEncodingType>(data[4]);
    resource_count_ = reinterpret_cast<const uint16_t*>(data)[4];
    alias_count_ = reinterpret_cast<const uint16_t*>(data)[5];
  } else {
    LOG(ERROR) << "Bad data pack version: got " << version << ", expected "
               << kFileFormatV4 << " or " << kFileFormatV5;
    LogDataPackError(BAD_VERSION);
    return false;
  }

  if (text_encoding_type_ != UTF8 && text_encoding_type_ != UTF16 &&
      text_encoding_type_ != BINARY) {
    LOG(ERROR) << "Bad data pack text encoding: got " << text_encoding_type_
               << ", expected between " << BINARY << " and " << UTF16;
    LogDataPackError(WRONG_ENCODING);
    return false;
  }

  // Sanity check the file.
  // 1) Check we have enough entries. There's an extra entry after the last item
  // which gives the length of the last item.
  size_t resource_table_size = (resource_count_ + 1) * sizeof(Entry);
  size_t alias_table_size = alias_count_ * sizeof(Alias);
  if (header_length + resource_table_size + alias_table_size > data_length) {
    LOG(ERROR) << "Data pack file corruption: "
               << "too short for number of entries.";
    LogDataPackError(INDEX_TRUNCATED);
    return false;
  }

  resource_table_ = reinterpret_cast<const Entry*>(&data[header_length]);
  alias_table_ = reinterpret_cast<const Alias*>(
      &data[header_length + resource_table_size]);

  // 2) Verify the entries are within the appropriate bounds. There's an extra
  // entry after the last item which gives us the length of the last item.
  for (size_t i = 0; i < resource_count_ + 1; ++i) {
    if (resource_table_[i].file_offset > data_length) {
      LOG(ERROR) << "Data pack file corruption: "
                 << "Entry #" << i << " past end.";
      LogDataPackError(ENTRY_NOT_FOUND);
      return false;
    }
  }

  // 3) Verify the aliases are within the appropriate bounds.
  for (size_t i = 0; i < alias_count_; ++i) {
    if (alias_table_[i].entry_index >= resource_count_) {
      LOG(ERROR) << "Data pack file corruption: "
                 << "Alias #" << i << " past end.";
      LogDataPackError(ENTRY_NOT_FOUND);
      return false;
    }
  }

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

bool DataPack::GetStringPiece(uint16_t resource_id,
                              base::StringPiece* data) const {
  // It won't be hard to make this endian-agnostic, but it's not worth
  // bothering to do right now.
#if !defined(ARCH_CPU_LITTLE_ENDIAN)
#error "datapack assumes little endian"
#endif

  const Entry* target = LookupEntryById(resource_id);
  if (!target)
    return false;

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
    return false;
  }

  MaybePrintResourceId(resource_id);
  size_t length = next_entry->file_offset - target->file_offset;
  data->set(reinterpret_cast<const char*>(data_source_->GetData() +
                                          target->file_offset),
            length);
  return true;
}

base::RefCountedStaticMemory* DataPack::GetStaticMemory(
    uint16_t resource_id) const {
  base::StringPiece piece;
  if (!GetStringPiece(resource_id, &piece))
    return NULL;

  return new base::RefCountedStaticMemory(piece.data(), piece.length());
}

ResourceHandle::TextEncodingType DataPack::GetTextEncodingType() const {
  return text_encoding_type_;
}

ui::ScaleFactor DataPack::GetScaleFactor() const {
  return scale_factor_;
}

#if DCHECK_IS_ON()
void DataPack::CheckForDuplicateResources(
    const std::vector<std::unique_ptr<ResourceHandle>>& packs) {
  for (size_t i = 0; i < resource_count_ + 1; ++i) {
    const uint16_t resource_id = resource_table_[i].resource_id;
    const float resource_scale = GetScaleForScaleFactor(scale_factor_);
    for (const auto& handle : packs) {
      if (GetScaleForScaleFactor(handle->GetScaleFactor()) != resource_scale)
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
                         const std::map<uint16_t, base::StringPiece>& resources,
                         TextEncodingType text_encoding_type) {
#if !defined(ARCH_CPU_LITTLE_ENDIAN)
#error "datapack assumes little endian"
#endif
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
    // A reverse map from string pieces to the index of the corresponding
    // original id in the final resource list.
    std::map<base::StringPiece, uint16_t> rev_map;
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
  const uint16_t resource_id = 0;
  file.Write(&resource_id, sizeof(resource_id));
  file.Write(&data_offset, sizeof(data_offset));

  // Write the aliases table, if any. Note: |aliases| is an std::map,
  // ensuring values are written in increasing order.
  for (const std::pair<uint16_t, uint16_t>& alias : aliases) {
    file.Write(&alias, sizeof(alias));
  }

  for (const auto& resource_id : resource_ids) {
    const base::StringPiece data = resources.find(resource_id)->second;
    file.Write(data.data(), data.length());
  }

  return file.Close();
}

}  // namespace ui

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/data_pack.h"

#include <errno.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_view_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
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
  if (!resource_ids_logged->contains(resource_id)) {
    printf("Resource=%d\n", resource_id);
    resource_ids_logged->insert(resource_id);
  }
}

}  // namespace

namespace ui {

class DataPack::MemoryMappedDataSource : public DataPack::DataSource {
 public:
  explicit MemoryMappedDataSource(std::unique_ptr<base::MemoryMappedFile> mmap)
      : mmap_(std::move(mmap)) {}

  MemoryMappedDataSource(const MemoryMappedDataSource&) = delete;
  MemoryMappedDataSource& operator=(const MemoryMappedDataSource&) = delete;

  ~MemoryMappedDataSource() override {}

  // DataPack::DataSource:
  base::span<const uint8_t> bytes() const override { return mmap_->bytes(); }

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
  base::span<const uint8_t> bytes() const override {
    return base::as_byte_span(data_);
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
  base::span<const uint8_t> bytes() const override { return buffer_; }

 private:
  base::raw_span<const uint8_t> buffer_;
};

DataPack::DataPack(ResourceScaleFactor resource_scale_factor)
    : text_encoding_type_(BINARY),
      resource_scale_factor_(resource_scale_factor) {
  // Static assert must be within a DataPack member to appease visiblity rules.
  static_assert(sizeof(Entry) == 6, "size of Entry must be 6");
  static_assert(sizeof(Alias) == 4, "size of Alias must be 4");
  // `#pragma pack(1)` on Entry/Alias means the tables can be overlaid onto the
  // input buffer at any offset, which is what lets base::subtle::
  // reinterpret_span() accept them below.
  static_assert(alignof(Entry) == 1, "Entry must not require alignment");
  static_assert(alignof(Alias) == 1, "Alias must not require alignment");
}

DataPack::~DataPack() {
}

namespace {

#if BUILDFLAG(IS_WIN)
inline DWORD GetLastErrorOrErrno() {
  return ::GetLastError();
}
#else
inline int GetLastErrorOrErrno() {
  return errno;
}
#endif

// Opens `path`, retrying after a short delay at most three extra times (four
// attempts in total) if the file cannot be opened due to it being in use.
base::expected<base::File, DataPack::ErrorState> OpenDataPack(
    const base::FilePath& path) {
  // Retry until at most 300ms has passed.
  static constexpr base::TimeDelta kMaxRetryDelay = base::Milliseconds(300);
  // Sleep 100ms between retries.
  static constexpr base::TimeDelta kRetryPause = base::Milliseconds(100);
  // The total number of attempts, including the first without delay.
  static constexpr int kFileSystemAttempts = kMaxRetryDelay / kRetryPause + 1;
  int i = 0;
  while (true) {
    // Open the file for reading; allowing other consumers to also open it for
    // reading and deleting. Do not allow others to write to it.
    base::File data_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                   base::File::FLAG_WIN_SHARE_DELETE);
    if (data_file.IsValid()) {
      if (i > 0) {
        // Record the number of retries if the file wasn't opened on the first
        // attempt.
        base::UmaHistogramExactLinear("DataPack.BusyOpenRetryCount", i,
                                      kFileSystemAttempts);
      }
      return data_file;
    }

    const auto error = GetLastErrorOrErrno();
    if (data_file.error_details() == base::File::FILE_ERROR_IN_USE) {
      // crbug.com/394631579: On Windows, it is not uncommon to get
      // ERROR_SHARING_VIOLATION due to some other program holding the file
      // open. Retry up to three more times in this case in the hope that this
      // is a transient issue.
      if (++i < kFileSystemAttempts) {
        base::PlatformThread::Sleep(kRetryPause);
        continue;
      }
      // Otherwise, record that all retries failed.
      base::UmaHistogramBoolean("DataPack.BusyOpenRetriesFailed", true);
    }

    DPLOG(ERROR) << "Failed to open datapack";
    return base::unexpected(DataPack::ErrorState{
        DataPack::FailureReason::kOpenFile, error, data_file.error_details()});
  }
}

}  // namespace

// static
base::expected<std::unique_ptr<DataPack::DataSource>, DataPack::ErrorState>
DataPack::LoadFromPathInternal(const base::FilePath& path) {
  ASSIGN_OR_RETURN(base::File data_file, OpenDataPack(path));
  if (data_file.GetLength() == 0) {
    // A zero-length file cannot be mapped as read-only.
    return base::unexpected(ErrorState{FailureReason::kEmptyFile});
  }
  std::unique_ptr<base::MemoryMappedFile> mmap =
      std::make_unique<base::MemoryMappedFile>();
  if (!mmap->Initialize(std::move(data_file))) {
    const auto error = GetLastErrorOrErrno();
    DPLOG(ERROR) << "Failed to mmap datapack";
    return base::unexpected(ErrorState{FailureReason::kMapFile, error});
  }
  if (net::GZipHeader::HasGZipHeader(mmap->bytes())) {
    std::string_view compressed = base::as_string_view(mmap->bytes());
    std::string data;
    if (!compression::GzipUncompress(compressed, &data)) {
      const auto error = GetLastErrorOrErrno();
      LOG(ERROR) << "Failed to unzip compressed datapack: " << path;
      return base::unexpected(ErrorState{FailureReason::kUnzip, error});
    }
    return base::ok(std::make_unique<StringDataSource>(std::move(data)));
  }
  return base::ok(std::make_unique<MemoryMappedDataSource>(std::move(mmap)));
}

bool DataPack::LoadFromPath(const base::FilePath& path) {
  return LoadFromPathWithError(path).has_value();
}

base::expected<void, DataPack::ErrorState> DataPack::LoadFromPathWithError(
    const base::FilePath& path) {
  std::unique_ptr<DataPack::DataSource> data_source;
  ASSIGN_OR_RETURN(data_source, LoadFromPathInternal(path));
  RETURN_IF_ERROR(LoadImpl(std::move(data_source)),
                  [](DataPack::FailureReason failure_reason) {
                    return ErrorState{failure_reason};
                  });
  return base::ok();
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
  return LoadImpl(std::make_unique<MemoryMappedDataSource>(std::move(mmap)))
      .has_value();
}

bool DataPack::LoadFromBuffer(base::span<const uint8_t> buffer) {
  return LoadImpl(std::make_unique<BufferDataSource>(buffer)).has_value();
}

base::expected<void, DataPack::FailureReason>
DataPack::SanityCheckFileAndRegisterResources(size_t margin_to_skip,
                                              base::span<const uint8_t> data,
                                              size_t resource_count,
                                              size_t alias_count) {
  // 1) Check we have enough entries. There's an extra entry after the last item
  // which gives the length of the last item. `resource_count` comes straight
  // from the file, so compute the table extents with overflow checking.
  const base::CheckedNumeric<size_t> checked_resource_table_size =
      (base::CheckedNumeric<size_t>(resource_count) + 1u) * sizeof(Entry);
  const base::CheckedNumeric<size_t> checked_alias_table_size =
      base::CheckedNumeric<size_t>(alias_count) * sizeof(Alias);
  // An overflow here yields SIZE_MAX, which always fails the check below.
  const size_t min_data_length =
      (checked_resource_table_size + checked_alias_table_size + margin_to_skip)
          .ValueOrDefault(std::numeric_limits<size_t>::max());
  if (min_data_length > data.size()) {
    // TODO(crbug.com/40221977): Add more information to LOG. Ditto below.
    LOG(ERROR) << "Data pack file corruption: "
               << "too short for number of entries. "
               << "data length is " << data.size()
               << " bytes, expected longer than " << min_data_length
               << " bytes.";
    return base::unexpected(FailureReason::kTooShort);
  }
  // Both extents are at most `min_data_length`, so neither overflowed.
  const size_t resource_table_size = checked_resource_table_size.ValueOrDie();
  const size_t alias_table_size = checked_alias_table_size.ValueOrDie();

  // Overlay the tables onto the file. The entry structs are packed and so have
  // no alignment requirement (see the static_asserts in the constructor), and
  // both extents are exact multiples of their element size.
  const base::span<const Entry> resource_table =
      base::subtle::reinterpret_span<const Entry>(
          data.subspan(margin_to_skip, resource_table_size));
  const base::span<const Alias> alias_table =
      base::subtle::reinterpret_span<const Alias>(
          data.subspan(margin_to_skip + resource_table_size, alias_table_size));

  // 2) Verify the entries are within the appropriate bounds. There's an extra
  // entry after the last item which gives us the length of the last item.
  for (size_t i = 0; i < resource_table.size(); ++i) {
    if (resource_table[i].file_offset > data.size()) {
      LOG(ERROR) << "Data pack file corruption: "
                 << "Entry #" << i << " past end.";
      return base::unexpected(FailureReason::kBoundsExceeded);
    }
  }

  // 3) Verify the entries are ordered correctly.
  for (size_t i = 0; i + 1 < resource_table.size(); ++i) {
    if (resource_table[i].file_offset > resource_table[i + 1].file_offset) {
      LOG(ERROR) << "Data pack file corruption: " << "Entry #" << i + 1
                 << " before Entry #" << i << ".";
      return base::unexpected(FailureReason::kOrderingViolation);
    }
  }

  // 4) Verify the aliases are within the appropriate bounds.
  // LookupEntryIndexById relies on this to use `entry_index` as an index into
  // `resource_table_`.
  for (size_t i = 0; i < alias_table.size(); ++i) {
    if (alias_table[i].entry_index >= resource_count) {
      LOG(ERROR) << "Data pack file corruption: "
                 << "Alias #" << i << " past end.";
      return base::unexpected(FailureReason::kAliasTableCorrupt);
    }
  }

  // Register the tables only once every check has passed, so that a rejected
  // pack leaves any previously loaded tables untouched rather than dangling
  // into the `DataSource` that LoadImpl is about to drop.
  resource_table_ = resource_table;
  alias_table_ = alias_table;

  return base::ok();
}

base::expected<void, DataPack::FailureReason> DataPack::LoadImpl(
    std::unique_ptr<DataPack::DataSource> data_source) {
  const base::span<const uint8_t> data = data_source->bytes();
  // Parse the version and check for truncated header.
  uint32_t version = 0;
  if (data.size() > sizeof(version)) {
    version = base::U32FromNativeEndian(data.first<4u>());
  }
  size_t header_length =
      version == kFileFormatV4 ? kHeaderLengthV4 : kHeaderLengthV5;
  if (version == 0 || data.size() < header_length) {
    DLOG(ERROR) << "Data pack file corruption: incomplete file header.";
    return base::unexpected(FailureReason::kIncompleteHeader);
  }

  // Parse the header of the file. The check above guarantees the whole header
  // is present, so every fixed-extent read below is in range.
  size_t resource_count = 0;
  size_t alias_count = 0;
  if (version == kFileFormatV4) {
    resource_count = base::U32FromNativeEndian(data.subspan<4u, 4u>());
    text_encoding_type_ = static_cast<TextEncodingType>(data[8u]);
  } else if (version == kFileFormatV5) {
    // Version 5 added the alias table and changed the header format.
    text_encoding_type_ = static_cast<TextEncodingType>(data[4u]);
    resource_count = base::U16FromNativeEndian(data.subspan<8u, 2u>());
    alias_count = base::U16FromNativeEndian(data.subspan<10u, 2u>());
  } else {
    LOG(ERROR) << "Bad data pack version: got " << version << ", expected "
               << kFileFormatV4 << " or " << kFileFormatV5;
    return base::unexpected(FailureReason::kBadPakVersion);
  }

  if (text_encoding_type_ != UTF8 && text_encoding_type_ != UTF16 &&
      text_encoding_type_ != BINARY) {
    LOG(ERROR) << "Bad data pack text encoding: got " << text_encoding_type_
               << ", expected between " << BINARY << " and " << UTF16;
    return base::unexpected(FailureReason::kBadEncodingType);
  }

  // Sanity check the file.
  RETURN_IF_ERROR(SanityCheckFileAndRegisterResources(
      header_length, data, resource_count, alias_count));

  data_source_ = std::move(data_source);
  return base::ok();
}

std::optional<size_t> DataPack::LookupEntryIndexById(
    uint16_t resource_id) const {
  // Search the resource table first as most resources will be in there.
  const base::span<const Entry> entries = resource_entries();
  const auto entry =
      std::ranges::lower_bound(entries, resource_id, {}, &Entry::resource_id);
  if (entry != entries.end() && entry->resource_id == resource_id) {
    return static_cast<size_t>(entry - entries.begin());
  }

  // Search the alias table for the ~10% of entries which are aliases.
  const base::span<const Alias> aliases = alias_table_;
  const auto alias =
      std::ranges::lower_bound(aliases, resource_id, {}, &Alias::resource_id);
  if (alias != aliases.end() && alias->resource_id == resource_id) {
    // Check 4 in SanityCheckFileAndRegisterResources() bounded `entry_index`.
    return alias->entry_index;
  }
  return std::nullopt;
}

bool DataPack::HasResource(uint16_t resource_id) const {
  return LookupEntryIndexById(resource_id).has_value();
}

std::optional<std::string_view> DataPack::GetStringView(
    uint16_t resource_id) const {
  const std::optional<size_t> index = LookupEntryIndexById(resource_id);
  if (!index.has_value()) {
    return std::nullopt;
  }

  const Entry& target = resource_table_[*index];
  // LookupEntryIndexById() only yields indices of real resources, so the
  // following entry - the one that gives `target`'s length - always exists.
  const Entry& next_entry = resource_table_[*index + 1u];
  const base::span<const uint8_t> data = data_source_->bytes();
  // If the next entry points beyond the end of the file this data pack's entry
  // table is corrupt. Log an error and return false. See
  // http://crbug.com/371301.
  if (next_entry.file_offset > data.size()) {
    LOG(ERROR) << "Entry #" << *index << " in data pack points off end "
               << "of file. This should have been caught when loading. Was the "
               << "file modified?";
    return std::nullopt;
  }
  if (target.file_offset > next_entry.file_offset) {
    LOG(ERROR) << "Entry #" << *index + 1u << " in data pack is before Entry #"
               << *index << ". This should have been caught when loading. "
               << "Was the file modified?";
    return std::nullopt;
  }

  MaybePrintResourceId(resource_id);
  return base::as_string_view(data.subspan(
      target.file_offset, next_entry.file_offset - target.file_offset));
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
  // Note this also visits the trailing sentinel entry, whose id is always 0.
  for (const Entry& entry : resource_table_) {
    const uint16_t resource_id = entry.resource_id;
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

  file.Write(base::byte_span_from_ref(kFileFormatV5));
  file.Write(base::byte_span_from_ref(encoding));
  file.Write(base::byte_span_from_ref(entry_count));
  file.Write(base::byte_span_from_ref(alias_count));

  // Each entry is a uint16_t + a uint32_t. We have an extra entry after the
  // last item so we can compute the size of the list item.
  const uint32_t index_length = (entry_count + 1) * sizeof(Entry);
  const uint32_t alias_table_length = alias_count * sizeof(Alias);
  uint32_t data_offset = kHeaderLengthV5 + index_length + alias_table_length;
  for (const uint16_t resource_id : resource_ids) {
    file.Write(base::byte_span_from_ref(resource_id));
    file.Write(base::byte_span_from_ref(data_offset));
    data_offset += resources.find(resource_id)->second.length();
  }

  // We place an extra entry after the last item that allows us to read the
  // size of the last item.
  const uint16_t extra_resource_id = 0;
  file.Write(base::byte_span_from_ref(extra_resource_id));
  file.Write(base::byte_span_from_ref(data_offset));

  // Write the aliases table, if any. Note: |aliases| is an std::map,
  // ensuring values are written in increasing order.
  for (const std::pair<const uint16_t, uint16_t>& alias : aliases) {
    file.Write(base::byte_span_from_ref(alias));
  }

  for (const auto& resource_id : resource_ids) {
    const std::string_view data = resources.find(resource_id)->second;
    file.Write(base::as_byte_span(data));
  }

  return file.Close();
}

}  // namespace ui

// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "third_party/leveldatabase/env_chromium.h"

#include <atomic>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_error_or.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "third_party/leveldatabase/chromium_logger.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/re2/src/re2/re2.h"

using base::FilePath;
using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::ProcessMemoryDump;
using leveldb::FileLock;
using leveldb::Slice;
using leveldb::Status;

namespace leveldb_env {
namespace {

template <typename ValueType>
using FileErrorOr = base::FileErrorOr<ValueType>;

// After this limit we don't bother doing file eviction for leveldb for speed,
// memory usage, and simplicity.
const constexpr size_t kFileLimitToDisableEviction = 10'000;

// The maximum time for the |Retrier| to indicate that an operation should
// be retried.
constexpr auto kMaxRetryDuration = base::Milliseconds(1000);

const FilePath::CharType table_extension[] = FILE_PATH_LITERAL(".ldb");

static const FilePath::CharType kLevelDBTestDirectoryPrefix[] =
    FILE_PATH_LITERAL("leveldb-test-");

// This name should not be changed or users involved in a crash might not be
// able to recover data.
static const char kDatabaseNameSuffixForRebuildDB[] = "__tmp_for_rebuild";

DBFactoryMethod& GetDBFactoryOverride() {
  static base::NoDestructor<DBFactoryMethod> instance;
  return *instance;
}

class ChromiumFileLock : public FileLock {
 public:
  ChromiumFileLock(std::unique_ptr<storage::FilesystemProxy::FileLock> lock,
                   const std::string& name)
      : lock(std::move(lock)), name(name) {}
  ChromiumFileLock(const ChromiumFileLock&) = delete;
  ChromiumFileLock& operator=(const ChromiumFileLock&) = delete;

  const std::unique_ptr<storage::FilesystemProxy::FileLock> lock;
  const std::string name;
};

class Retrier {
 public:
  Retrier()
      // TODO(crbug.com/1059965): figure out a better way to handle time for
      // tests.
      : start_(base::subtle::TimeTicksNowIgnoringOverride()),
        limit_(start_ + kMaxRetryDuration),
        last_(start_),
        time_to_sleep_(base::Milliseconds(10)) {}

  Retrier(const Retrier&) = delete;
  Retrier& operator=(const Retrier&) = delete;

  ~Retrier() = default;
  bool ShouldKeepTrying() {
    if (last_ < limit_) {
      base::PlatformThread::Sleep(time_to_sleep_);
      // TODO(crbug.com/1059965): figure out a better way to handle time for
      // tests.
      last_ = base::subtle::TimeTicksNowIgnoringOverride();
      return true;
    }
    return false;
  }

 private:
  const base::TimeTicks start_;
  base::TimeTicks limit_;
  base::TimeTicks last_;
  base::TimeDelta time_to_sleep_;
};

class ChromiumSequentialFile : public leveldb::SequentialFile {
 public:
  ChromiumSequentialFile(const std::string& fname, base::File f)
      : filename_(fname), file_(std::move(f)) {}

  ChromiumSequentialFile(const ChromiumSequentialFile&) = delete;
  ChromiumSequentialFile& operator=(const ChromiumSequentialFile&) = delete;

  ~ChromiumSequentialFile() override = default;

  // Note: This method is relatively hot during leveldb database
  // compaction. Please avoid making them slower.
  Status Read(size_t n, Slice* result, char* scratch) override {
    std::optional<size_t> bytes_read = file_.ReadAtCurrentPosNoBestEffort(
        base::as_writable_bytes(UNSAFE_TODO(base::span(scratch, n))));
    if (!bytes_read.has_value()) {
      base::File::Error error = base::File::GetLastFileError();
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         kSequentialFileRead, error);
    }
    *result = Slice(scratch, *bytes_read);
    return Status::OK();
  }

  Status Skip(uint64_t n) override {
    if (file_.Seek(base::File::FROM_CURRENT, n) == -1) {
      base::File::Error error = base::File::GetLastFileError();
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         kSequentialFileSkip, error);
    } else {
      return Status::OK();
    }
  }

 private:
  std::string filename_;
  base::File file_;
};

void RemoveFile(const Slice& key, void* value) {
  delete static_cast<base::File*>(value);
}

Status ReadFromFileToScratch(uint64_t offset,
                             size_t n,
                             Slice* result,
                             char* scratch,
                             base::File* file,
                             const base::FilePath& file_path) {
  int bytes_read = file->Read(offset, scratch, n);
  if (bytes_read < 0) {
    return MakeIOError(file_path.AsUTF8Unsafe(), "Could not perform read",
                       kRandomAccessFileRead);
  }
  *result = Slice(scratch, (bytes_read < 0) ? 0 : bytes_read);

  return Status::OK();
}

// The cache mechanism uses leveldb's LRU cache, which is a threadsafe sharded
// LRU cache. The keys use the pointer value of |this|, and the values are file
// objects.
// Each object will only use its own cache entry, so the |Erase| call in the
// object destructor should synchronously delete the file from the cache. This
// ensures that pointer location re-use won't re-use an entry in the cache as
// the entry at |this| will always have been deleted.
// Files are always cleaned up with |RemoveFile|, which will be called when the
// ChromiumEvictableRandomAccessFile is deleted, the cache is deleted, or the
// file is evicted.
class ChromiumEvictableRandomAccessFile : public leveldb::RandomAccessFile {
 public:
  ChromiumEvictableRandomAccessFile(base::FilePath file_path,
                                    base::File file,
                                    storage::FilesystemProxy* filesystem,
                                    leveldb::Cache* file_cache)
      : filepath_(std::move(file_path)),
        filesystem_(filesystem),
        file_cache_(file_cache),
        cache_key_data_(this),
        cache_key_(
            leveldb::Slice(reinterpret_cast<const char*>(&cache_key_data_),
                           sizeof(cache_key_data_))) {
    DCHECK(file_cache_);
    base::File* heap_file = new base::File(std::move(file));
    // A |charge| of '1' is used because the capacity is the file handle limit,
    // and each entry is one file.
    file_cache_->Release(file_cache_->Insert(cache_key_, heap_file,
                                             1 /* charge */, &RemoveFile));
  }

  ChromiumEvictableRandomAccessFile(const ChromiumEvictableRandomAccessFile&) =
      delete;
  ChromiumEvictableRandomAccessFile& operator=(
      const ChromiumEvictableRandomAccessFile&) = delete;

  virtual ~ChromiumEvictableRandomAccessFile() {
    file_cache_->Erase(cache_key_);
  }

  // Note: This method is relatively hot during leveldb database
  // compaction. Please avoid making them slower.
  Status Read(uint64_t offset,
              size_t n,
              Slice* result,
              char* scratch) const override {
    leveldb::Cache::Handle* handle = file_cache_->Lookup(cache_key_);
    if (!handle) {
      int flags = base::File::FLAG_READ | base::File::FLAG_OPEN;
      ASSIGN_OR_RETURN(
          base::File file, filesystem_->OpenFile(filepath_, flags), [&](auto) {
            return MakeIOError(filepath_.AsUTF8Unsafe(),
                               "Could not perform read", kRandomAccessFileRead);
          });
      handle = file_cache_->Insert(cache_key_, new base::File(std::move(file)),
                                   sizeof(base::File), &RemoveFile);
    }
    base::File* file = static_cast<base::File*>(file_cache_->Value(handle));
    Status status =
        ReadFromFileToScratch(offset, n, result, scratch, file, filepath_);
    file_cache_->Release(handle);
    return status;
  }

 private:
  const base::FilePath filepath_;
  storage::FilesystemProxy* const filesystem_;
  mutable leveldb::Cache* file_cache_;
  const ChromiumEvictableRandomAccessFile* cache_key_data_;
  leveldb::Slice cache_key_;
};

class ChromiumRandomAccessFile : public leveldb::RandomAccessFile {
 public:
  ChromiumRandomAccessFile(base::FilePath file_path, base::File file)
      : filepath_(std::move(file_path)), file_(std::move(file)) {}

  ChromiumRandomAccessFile(const ChromiumRandomAccessFile&) = delete;
  ChromiumRandomAccessFile& operator=(const ChromiumRandomAccessFile&) = delete;

  virtual ~ChromiumRandomAccessFile() {}

  // Note: This method is relatively hot during leveldb database
  // compaction. Please avoid making them slower.
  Status Read(uint64_t offset,
              size_t n,
              Slice* result,
              char* scratch) const override {
    return ReadFromFileToScratch(offset, n, result, scratch, &file_, filepath_);
  }

 private:
  const base::FilePath filepath_;
  mutable base::File file_;
};

class ChromiumWritableFile : public leveldb::WritableFile {
 public:
  ChromiumWritableFile(const std::string& fname,
                       base::File f,
                       storage::FilesystemProxy* filesystem);

  ChromiumWritableFile(const ChromiumWritableFile&) = delete;
  ChromiumWritableFile& operator=(const ChromiumWritableFile&) = delete;

  ~ChromiumWritableFile() override = default;
  leveldb::Status Append(const leveldb::Slice& data) override;
  leveldb::Status Close() override;
  leveldb::Status Flush() override;
  leveldb::Status Sync() override;

 private:
  enum Type { kManifest, kTable, kOther };
  leveldb::Status SyncParent();

  std::string filename_;
  base::File file_;
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  storage::FilesystemProxy* const filesystem_;
#endif
  Type file_type_;
  std::string parent_dir_;
};

ChromiumWritableFile::ChromiumWritableFile(const std::string& fname,
                                           base::File f,
                                           storage::FilesystemProxy* filesystem)
    : filename_(fname),
      file_(std::move(f)),
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
      filesystem_(filesystem),
#endif
      file_type_(kOther) {
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  if (path.BaseName().AsUTF8Unsafe().find("MANIFEST") == 0) {
    file_type_ = kManifest;
  } else if (base::EqualsCaseInsensitiveASCII(path.Extension().c_str(),
                                              table_extension)) {
    file_type_ = kTable;
  }
  parent_dir_ = FilePath::FromUTF8Unsafe(fname).DirName().AsUTF8Unsafe();
}

Status ChromiumWritableFile::SyncParent() {
  TRACE_EVENT0("leveldb", "SyncParent");
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  FilePath path = FilePath::FromUTF8Unsafe(parent_dir_);
  FileErrorOr<base::File> result = filesystem_->OpenFile(
      path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!result.has_value()) {
    return MakeIOError(parent_dir_, "Unable to open directory", kSyncParent,
                       result.error());
  }
  if (!result->Flush()) {
    base::File::Error error = base::File::GetLastFileError();
    return MakeIOError(parent_dir_, base::File::ErrorToString(error),
                       kSyncParent, error);
  }
#endif
  return Status::OK();
}

Status ChromiumWritableFile::Append(const Slice& data) {
  DCHECK(file_.IsValid());
  int bytes_written = file_.WriteAtCurrentPos(data.data(), data.size());
  if (static_cast<size_t>(bytes_written) != data.size()) {
    base::File::Error error = base::File::GetLastFileError();
    return MakeIOError(filename_, base::File::ErrorToString(error),
                       kWritableFileAppend, error);
  }
  return Status::OK();
}

Status ChromiumWritableFile::Close() {
  file_.Close();
  return Status::OK();
}

Status ChromiumWritableFile::Flush() {
  // base::File doesn't do buffered I/O (i.e. POSIX FILE streams) so nothing to
  // flush.
  return Status::OK();
}

Status ChromiumWritableFile::Sync() {
  TRACE_EVENT0("leveldb", "WritableFile::Sync");

  base::File::Error error = base::File::FILE_OK;
  Status status = Status::OK();

  // leveldb's implicit contract for Sync() is that if this instance is for a
  // manifest file then the directory is also sync'ed, to ensure new files
  // referred to by the manifest are in the filesystem.
  //
  // This needs to happen before the manifest file is flushed to disk, to
  // avoid crashing in a state where the manifest refers to files that are not
  // yet on disk.
  //
  // See leveldb's env_posix.cc.
  if (file_type_ == kManifest) {
    status = SyncParent();
    if (!status.ok()) {
      error = base::File::GetLastFileError();
    }
  }

  if (status.ok() && !file_.Flush()) {
    error = base::File::GetLastFileError();
    status = MakeIOError(filename_, base::File::ErrorToString(error),
                         kWritableFileSync, error);
  }

  base::UmaHistogramExactLinear("LevelDBEnv.SyncResult", -error,
                                -base::File::FILE_ERROR_MAX);
  return status;
}

// Return the maximum number of read-only files to keep open.
size_t GetLevelDBFileLimit(size_t max_file_descriptors) {
  // Allow use of 20% of available file descriptors for read-only files.
  return max_file_descriptors / 5;
}

std::string GetDumpNameForDB(const leveldb::DB* db) {
  return base::StringPrintf("leveldatabase/db_0x%" PRIXPTR,
                            reinterpret_cast<uintptr_t>(db));
}

std::string GetDumpNameForCache(DBTracker::SharedReadCacheUse cache) {
  switch (cache) {
    case DBTracker::SharedReadCacheUse_Browser:
      return "leveldatabase/block_cache/browser";
    case DBTracker::SharedReadCacheUse_Web:
      return "leveldatabase/block_cache/web";
    case DBTracker::SharedReadCacheUse_Unified:
      return "leveldatabase/block_cache/unified";
    case DBTracker::SharedReadCacheUse_InMemory:
      return "leveldatabase/block_cache/in_memory";
    case DBTracker::SharedReadCacheUse_NumCacheUses:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

MemoryAllocatorDump* CreateDumpMalloced(ProcessMemoryDump* pmd,
                                        const std::string& name,
                                        size_t size) {
  auto* dump = pmd->CreateAllocatorDump(name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, size);
  static const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_name)
    pmd->AddSuballocation(dump->guid(), system_allocator_name);
  return dump;
}

void RecordCacheUsageInTracing(ProcessMemoryDump* pmd,
                               DBTracker::SharedReadCacheUse cache) {
  std::string name = GetDumpNameForCache(cache);
  leveldb::Cache* cache_ptr = nullptr;
  switch (cache) {
    case DBTracker::SharedReadCacheUse_Browser:
      cache_ptr = leveldb_chrome::GetSharedBrowserBlockCache();
      break;
    case DBTracker::SharedReadCacheUse_Web:
      cache_ptr = leveldb_chrome::GetSharedWebBlockCache();
      break;
    case DBTracker::SharedReadCacheUse_Unified:
      cache_ptr = leveldb_chrome::GetSharedBrowserBlockCache();
      break;
    case DBTracker::SharedReadCacheUse_InMemory:
      cache_ptr = leveldb_chrome::GetSharedInMemoryBlockCache();
      break;
    case DBTracker::SharedReadCacheUse_NumCacheUses:
      NOTREACHED_IN_MIGRATION();
  }
  if (!cache_ptr)
    return;
  CreateDumpMalloced(pmd, name, cache_ptr->TotalCharge());
}

}  // namespace

Options::Options() {
// Note: Ensure that these default values correspond to those in
// components/services/leveldb/public/mojom/leveldb.mojom.
// TODO(cmumford) Create struct-trait for leveldb.mojom.OpenOptions to force
// users to pass in a leveldb_env::Options instance (and it's defaults).
//
// Currently log reuse is an experimental feature in leveldb. More info at:
// https://github.com/google/leveldb/commit/251ebf5dc70129ad3
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Reusing logs on Chrome OS resulted in an unacceptably high leveldb
  // corruption rate (at least for Indexed DB). More info at
  // https://crbug.com/460568
  reuse_logs = false;
#else
  // Low end devices have limited RAM. Reusing logs will prevent the database
  // from being compacted on open and instead load the log file back into the
  // memory buffer which won't be written until it hits the maximum size
  // (leveldb::Options::write_buffer_size - 4MB by default). The downside here
  // is that databases opens take longer as the open is blocked on compaction.
  reuse_logs = !base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled();
#endif
  // By default use a single shared block cache to conserve memory. The owner of
  // this object can create their own, or set to NULL to have leveldb create a
  // new db-specific block cache.
  block_cache = leveldb_chrome::GetSharedBrowserBlockCache();
}

const char* MethodIDToString(MethodID method) {
  switch (method) {
    case kSequentialFileRead:
      return "SequentialFileRead";
    case kSequentialFileSkip:
      return "SequentialFileSkip";
    case kRandomAccessFileRead:
      return "RandomAccessFileRead";
    case kWritableFileAppend:
      return "WritableFileAppend";
    case kWritableFileClose:
      return "WritableFileClose";
    case kWritableFileFlush:
      return "WritableFileFlush";
    case kWritableFileSync:
      return "WritableFileSync";
    case kNewSequentialFile:
      return "NewSequentialFile";
    case kNewRandomAccessFile:
      return "NewRandomAccessFile";
    case kNewWritableFile:
      return "NewWritableFile";
    case kNewAppendableFile:
      return "NewAppendableFile";
    case kCreateDir:
      return "CreateDir";
    case kGetFileSize:
      return "GetFileSize";
    case kRenameFile:
      return "RenameFile";
    case kLockFile:
      return "LockFile";
    case kUnlockFile:
      return "UnlockFile";
    case kGetTestDirectory:
      return "GetTestDirectory";
    case kNewLogger:
      return "NewLogger";
    case kSyncParent:
      return "SyncParent";
    case kGetChildren:
      return "GetChildren";
    case kRemoveFile:
      return "RemoveFile";
    case kRemoveDir:
      return "RemoveDir";
    case kObsoleteDeleteFile:
    case kObsoleteDeleteDir:
    case kNumEntries:
      NOTREACHED_IN_MIGRATION();
      return "Unknown";
  }
  NOTREACHED_IN_MIGRATION();
  return "Unknown";
}

Status MakeIOError(Slice filename,
                   const std::string& message,
                   MethodID method,
                   base::File::Error error) {
  // TOOD(crbug.com/760362): Map base::File::FILE_ERROR_NOT_FOUND to
  //                         Status::NotFound, after fixing LevelDB to handle
  //                         the NotFound correctly.
  return Status::IOError(
      filename,
      base::StrCat(
          {message, " (ChromeMethod",
           error == base::File::FILE_OK ? "Only" : "BFE", ": ",
           base::NumberToString(method), "::", MethodIDToString(method),
           error == base::File::FILE_OK ? std::string()
                                        : "::" + base::NumberToString(-error),
           ")"}));
}

ErrorParsingResult ParseMethodAndError(const leveldb::Status& status,
                                       MethodID* method_param,
                                       base::File::Error* error) {
  const std::string status_string = status.ToString();
  int method;
  if (RE2::PartialMatch(status_string.c_str(), "ChromeMethodOnly: (\\d+)",
                        &method)) {
    *method_param = static_cast<MethodID>(method);
    return METHOD_ONLY;
  }
  int parsed_error;
  if (RE2::PartialMatch(status_string.c_str(),
                        "ChromeMethodBFE: (\\d+)::.*::(\\d+)", &method,
                        &parsed_error)) {
    *method_param = static_cast<MethodID>(method);
    *error = static_cast<base::File::Error>(-parsed_error);
    DCHECK_LT(*error, base::File::FILE_OK);
    DCHECK_GT(*error, base::File::FILE_ERROR_MAX);
    return METHOD_AND_BFE;
  }
  return NONE;
}

// Keep in sync with LevelDBCorruptionTypes in histograms.xml. Also, don't
// change the order because indices into this array have been recorded in uma
// histograms.
const char* patterns[] = {
  "missing files",
  "log record too small",
  "corrupted internal key",
  "partial record",
  "missing start of fragmented record",
  "error in middle of record",
  "unknown record type",
  "truncated record at end",
  "bad record length",
  "VersionEdit",
  "FileReader invoked with unexpected value",
  "corrupted key",
  "CURRENT file does not end with newline",
  "no meta-nextfile entry",
  "no meta-lognumber entry",
  "no last-sequence-number entry",
  "malformed WriteBatch",
  "bad WriteBatch Put",
  "bad WriteBatch Delete",
  "unknown WriteBatch tag",
  "WriteBatch has wrong count",
  "bad entry in block",
  "bad block contents",
  "bad block handle",
  "truncated block read",
  "block checksum mismatch",
  "checksum mismatch",
  "corrupted compressed block contents",
  "bad block type",
  "bad magic number",
  "file is too short",
};

// Returns 1-based index into the above array or 0 if nothing matches.
int GetCorruptionCode(const leveldb::Status& status) {
  DCHECK(!status.IsIOError());
  DCHECK(!status.ok());
  const int kOtherError = 0;
  int error = kOtherError;
  const std::string& str_error = status.ToString();
  const size_t kNumPatterns = std::size(patterns);
  for (size_t i = 0; i < kNumPatterns; ++i) {
    if (str_error.find(patterns[i]) != std::string::npos) {
      error = i + 1;
      break;
    }
  }
  return error;
}

int GetNumCorruptionCodes() {
  // + 1 for the "other" error that is returned when a corruption message
  // doesn't match any of the patterns.
  return std::size(patterns) + 1;
}

std::string GetCorruptionMessage(const leveldb::Status& status) {
  int code = GetCorruptionCode(status);
  if (code == 0)
    return "Unknown corruption";
  return patterns[code - 1];
}

bool IndicatesDiskFull(const leveldb::Status& status) {
  if (status.ok())
    return false;
  leveldb_env::MethodID method;
  base::File::Error error = base::File::FILE_OK;
  leveldb_env::ErrorParsingResult result =
      leveldb_env::ParseMethodAndError(status, &method, &error);
  return (result == leveldb_env::METHOD_AND_BFE &&
          static_cast<base::File::Error>(error) ==
              base::File::FILE_ERROR_NO_SPACE);
}

std::string DatabaseNameForRewriteDB(const std::string& original_name) {
  return original_name + kDatabaseNameSuffixForRebuildDB;
}

// Given the size of the disk, identified by |disk_size| in bytes, determine the
// appropriate write_buffer_size. Ignoring snapshots, if the current set of
// tables in a database contains a set of key/value pairs identified by {A}, and
// a set of key/value pairs identified by {B} has been written and is in the log
// file, then during compaction you will have {A} + {B} + {A, B} = 2A + 2B.
// There is no way to know the size of A, so minimizing the size of B will
// maximize the likelihood of a successful compaction.
size_t WriteBufferSize(int64_t disk_size) {
  const leveldb_env::Options default_options;
  const int64_t kMinBufferSize = 1024 * 1024;
  const int64_t kMaxBufferSize = default_options.write_buffer_size;
  const int64_t kDiskMinBuffSize = 10 * 1024 * 1024;
  const int64_t kDiskMaxBuffSize = 40 * 1024 * 1024;

  if (disk_size == -1)
    return default_options.write_buffer_size;

  if (disk_size <= kDiskMinBuffSize)
    return kMinBufferSize;

  if (disk_size >= kDiskMaxBuffSize)
    return kMaxBufferSize;

  // A linear equation to intersect (kDiskMinBuffSize, kMinBufferSize) and
  // (kDiskMaxBuffSize, kMaxBufferSize).
  return static_cast<size_t>(
      kMinBufferSize +
      ((kMaxBufferSize - kMinBufferSize) * (disk_size - kDiskMinBuffSize)) /
          (kDiskMaxBuffSize - kDiskMinBuffSize));
}

ChromiumEnv::ChromiumEnv()
    : ChromiumEnv(std::make_unique<storage::FilesystemProxy>(
          storage::FilesystemProxy::UNRESTRICTED,
          base::FilePath())) {}

ChromiumEnv::ChromiumEnv(bool log_lock_errors) : ChromiumEnv() {
  log_lock_errors_ = log_lock_errors;
}

ChromiumEnv::ChromiumEnv(std::unique_ptr<storage::FilesystemProxy> filesystem)
    : filesystem_(std::move(filesystem)) {
  DCHECK(filesystem_);

  size_t max_open_files = base::GetMaxFds();
  if (max_open_files < kFileLimitToDisableEviction) {
    file_cache_.reset(
        leveldb::NewLRUCache(GetLevelDBFileLimit(max_open_files)));
  }
}

ChromiumEnv::~ChromiumEnv() {
  // In chromium, ChromiumEnv is leaked. It'd be nice to add NOTREACHED here to
  // ensure that behavior isn't accidentally changed, but there's an instance in
  // a unit test that is deleted.
}

bool ChromiumEnv::FileExists(const std::string& fname) {
  return filesystem_->PathExists(FilePath::FromUTF8Unsafe(fname));
}

const char* ChromiumEnv::FileErrorString(base::File::Error error) {
  switch (error) {
    case base::File::FILE_ERROR_FAILED:
      return "No further details.";
    case base::File::FILE_ERROR_IN_USE:
      return "File currently in use.";
    case base::File::FILE_ERROR_EXISTS:
      return "File already exists.";
    case base::File::FILE_ERROR_NOT_FOUND:
      return "File not found.";
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return "Access denied.";
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return "Too many files open.";
    case base::File::FILE_ERROR_NO_MEMORY:
      return "Out of memory.";
    case base::File::FILE_ERROR_NO_SPACE:
      return "No space left on drive.";
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return "Not a directory.";
    case base::File::FILE_ERROR_INVALID_OPERATION:
      return "Invalid operation.";
    case base::File::FILE_ERROR_SECURITY:
      return "Security error.";
    case base::File::FILE_ERROR_ABORT:
      return "File operation aborted.";
    case base::File::FILE_ERROR_NOT_A_FILE:
      return "The supplied path was not a file.";
    case base::File::FILE_ERROR_NOT_EMPTY:
      return "The file was not empty.";
    case base::File::FILE_ERROR_INVALID_URL:
      return "Invalid URL.";
    case base::File::FILE_ERROR_IO:
      return "OS or hardware error.";
    case base::File::FILE_OK:
      return "OK.";
    case base::File::FILE_ERROR_MAX:
      NOTREACHED_IN_MIGRATION();
  }
  NOTIMPLEMENTED();
  return "Unknown error.";
}

// Delete unused table backup files - a feature no longer supported.
// TODO(cmumford): Delete this function once found backup files drop below some
//                 very small (TBD) number.
void ChromiumEnv::RemoveBackupFiles(const FilePath& dir) {
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      "LevelDBEnv.DeleteTableBackupFile",
      base::Histogram::kUmaTargetedHistogramFlag);

  ASSIGN_OR_RETURN(
      std::vector<base::FilePath> result,
      filesystem_->GetDirectoryEntries(
          dir, storage::FilesystemProxy::DirectoryEntryType::kFilesOnly),
      [](auto) {});

  for (const auto& path : result) {
    if (path.Extension() == FILE_PATH_LITERAL(".bak"))
      histogram->AddBoolean(filesystem_->DeleteFile(path));
  }
}

// Test must call this *before* opening any random-access files.
void ChromiumEnv::SetReadOnlyFileLimitForTesting(int max_open_files) {
  DCHECK(!file_cache_ || file_cache_->TotalCharge() == 0);
  file_cache_.reset(leveldb::NewLRUCache(max_open_files));
}

Status ChromiumEnv::GetChildren(const std::string& dir,
                                std::vector<std::string>* result) {
  FilePath dir_path = FilePath::FromUTF8Unsafe(dir);
  RemoveBackupFiles(dir_path);

  ASSIGN_OR_RETURN(
      std::vector<FilePath> entries,
      filesystem_->GetDirectoryEntries(
          dir_path,
          storage::FilesystemProxy::DirectoryEntryType::kFilesAndDirectories),
      MakeIOError, dir, "Could not open/read directory", kGetChildren);

  result->clear();
  for (const auto& entry : entries) {
    result->push_back(entry.BaseName().AsUTF8Unsafe());
  }

  return Status::OK();
}

Status ChromiumEnv::RemoveFile(const std::string& fname) {
  Status result;
  FilePath fname_filepath = FilePath::FromUTF8Unsafe(fname);
  if (!filesystem_->DeleteFile(fname_filepath)) {
    result = MakeIOError(fname, "Could not delete file.", kRemoveFile);
  }
  return result;
}

Status ChromiumEnv::CreateDir(const std::string& name) {
  Status result;
  base::File::Error error = base::File::FILE_OK;
  Retrier retrier;
  do {
    error = filesystem_->CreateDirectory(base::FilePath::FromUTF8Unsafe(name));
    if (error == base::File::FILE_OK)
      return result;
  } while (retrier.ShouldKeepTrying());
  return MakeIOError(name, "Could not create directory.", kCreateDir, error);
}

Status ChromiumEnv::RemoveDir(const std::string& name) {
  Status result;
  if (!filesystem_->DeleteFile(FilePath::FromUTF8Unsafe(name))) {
    result = MakeIOError(name, "Could not delete directory.", kRemoveDir);
  }
  return result;
}

Status ChromiumEnv::GetFileSize(const std::string& fname, uint64_t* size) {
  Status s;
  std::optional<base::File::Info> info =
      filesystem_->GetFileInfo(base::FilePath::FromUTF8Unsafe(fname));
  if (!info) {
    *size = 0;
    s = MakeIOError(fname, "Could not determine file size.", kGetFileSize);
  } else {
    *size = static_cast<uint64_t>(info->size);
  }
  return s;
}

Status ChromiumEnv::RenameFile(const std::string& src, const std::string& dst) {
  Status result;
  FilePath src_file_path = FilePath::FromUTF8Unsafe(src);
  if (!filesystem_->PathExists(src_file_path))
    return result;
  FilePath destination = FilePath::FromUTF8Unsafe(dst);

  Retrier retrier;
  base::File::Error error = base::File::FILE_OK;
  do {
    error = filesystem_->RenameFile(src_file_path, destination);
    if (error == base::File::FILE_OK)
      return result;
  } while (retrier.ShouldKeepTrying());

  DCHECK(error != base::File::FILE_OK);
  char buf[100];
  base::snprintf(buf,
           sizeof(buf),
           "Could not rename file: %s",
           FileErrorString(error));
  return MakeIOError(src, buf, kRenameFile, error);
}

Status ChromiumEnv::LockFile(const std::string& fname, FileLock** lock) {
  *lock = nullptr;
  Status result;
  const base::FilePath path = base::FilePath::FromUTF8Unsafe(fname);
  Retrier retrier;
  FileErrorOr<std::unique_ptr<storage::FilesystemProxy::FileLock>> lock_result;
  bool same_process_held_lock = false;
  size_t tries = 0;
  do {
    tries++;
    same_process_held_lock = false;
    lock_result = filesystem_->LockFile(path, &same_process_held_lock);
  } while (!lock_result.has_value() && retrier.ShouldKeepTrying());

  if (!lock_result.has_value()) {
    if (log_lock_errors_ &&
        lock_result.error() == base::File::FILE_ERROR_IN_USE) {
      base::UmaHistogramBoolean("LevelDBEnv.LockFileInUseByThisProcess",
                                same_process_held_lock);
    }

    return MakeIOError(fname, FileErrorString(lock_result.error()), kLockFile,
                       lock_result.error());
  }

  if (log_lock_errors_) {
    // 100 because the retrier tries every ~10ms for ~1000ms.
    base::UmaHistogramCounts100("LevelDBEnv.LockFileSuccessAttempts", tries);
  }

  *lock = new ChromiumFileLock(std::move(lock_result.value()), fname);
  return result;
}

Status ChromiumEnv::UnlockFile(FileLock* lock) {
  std::unique_ptr<ChromiumFileLock> my_lock(
      reinterpret_cast<ChromiumFileLock*>(lock));
  Status result = Status::OK();

  base::File::Error error_code = my_lock->lock->Release();
  if (error_code != base::File::FILE_OK) {
    result =
        MakeIOError(my_lock->name, "Could not unlock lock file.", kUnlockFile);
  }
  return result;
}

Status ChromiumEnv::GetTestDirectory(std::string* path) {
  mu_.Acquire();
  if (test_directory_.empty()) {
    if (!base::CreateNewTempDirectory(kLevelDBTestDirectoryPrefix,
                                      &test_directory_)) {
      mu_.Release();
      return MakeIOError(
          "Could not create temp directory.", "", kGetTestDirectory);
    }
  }
  *path = test_directory_.AsUTF8Unsafe();
  mu_.Release();
  return Status::OK();
}

Status ChromiumEnv::NewLogger(const std::string& fname,
                              leveldb::Logger** result) {
  *result = nullptr;
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  ASSIGN_OR_RETURN(base::File open_result,
                   filesystem_->OpenFile(path, base::File::FLAG_CREATE_ALWAYS |
                                                   base::File::FLAG_WRITE),
                   MakeIOError, fname, "Unable to create log file", kNewLogger);
  *result = new leveldb::ChromiumLogger(std::move(open_result));
  return Status::OK();
}

Status ChromiumEnv::NewSequentialFile(const std::string& fname,
                                      leveldb::SequentialFile** result) {
  *result = nullptr;
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  ASSIGN_OR_RETURN(base::File open_result,
                   filesystem_->OpenFile(
                       path, base::File::FLAG_OPEN | base::File::FLAG_READ),
                   MakeIOError, fname, "Unable to create sequential file",
                   kNewSequentialFile);
  *result = new ChromiumSequentialFile(fname, std::move(open_result));
  return Status::OK();
}

Status ChromiumEnv::NewRandomAccessFile(const std::string& fname,
                                        leveldb::RandomAccessFile** result) {
  *result = nullptr;
  base::FilePath file_path = FilePath::FromUTF8Unsafe(fname);
  ASSIGN_OR_RETURN(base::File file,
                   filesystem_->OpenFile(file_path, base::File::FLAG_READ |
                                                        base::File::FLAG_OPEN),
                   [&](base::File::Error error) {
                     return MakeIOError(fname, FileErrorString(error),
                                        kNewRandomAccessFile, error);
                   });
  if (file_cache_) {
    *result = new ChromiumEvictableRandomAccessFile(
        std::move(file_path), std::move(file), filesystem_.get(),
        file_cache_.get());
  } else {
    *result =
        new ChromiumRandomAccessFile(std::move(file_path), std::move(file));
  }
  return Status::OK();
}

Status ChromiumEnv::NewWritableFile(const std::string& fname,
                                    leveldb::WritableFile** result) {
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  FileErrorOr<base::File> open_result = filesystem_->OpenFile(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!open_result.has_value()) {
    *result = nullptr;
    return MakeIOError(fname, "Unable to create writable file",
                       kNewWritableFile, open_result.error());
  }
  *result = new ChromiumWritableFile(fname, std::move(open_result.value()),
                                     filesystem_.get());
  return Status::OK();
}

Status ChromiumEnv::NewAppendableFile(const std::string& fname,
                                      leveldb::WritableFile** result) {
  *result = nullptr;
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  ASSIGN_OR_RETURN(base::File open_result,
                   filesystem_->OpenFile(path, base::File::FLAG_OPEN_ALWAYS |
                                                   base::File::FLAG_APPEND),
                   MakeIOError, fname, "Unable to create appendable file",
                   kNewAppendableFile);
  *result = new ChromiumWritableFile(fname, std::move(open_result),
                                     filesystem_.get());
  return Status::OK();
}

uint64_t ChromiumEnv::NowMicros() {
  return base::TimeTicks::Now().ToInternalValue();
}

void ChromiumEnv::SleepForMicroseconds(int micros) {
  // Round up to the next millisecond.
  base::PlatformThread::Sleep(base::Microseconds(micros));
}

class Thread : public base::PlatformThread::Delegate {
 public:
  Thread(void (*function)(void* arg), void* arg)
      : function_(function), arg_(arg) {
    base::PlatformThreadHandle handle;
    bool success = base::PlatformThread::Create(0, this, &handle);
    DCHECK(success);
  }

  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  virtual ~Thread() {}
  void ThreadMain() override {
    (*function_)(arg_);
    delete this;
  }

 private:
  void (*function_)(void* arg);
  void* arg_;
};

void ChromiumEnv::Schedule(ScheduleFunc* function, void* arg) {
  // The BLOCK_SHUTDOWN is required to avoid shutdown hangs. The scheduled
  // tasks may be blocking foreground threads waiting for their completions.
  // see: https://crbug.com/1086185.
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::WithBaseSyncPrimitives(),
                              base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                             base::BindOnce(function, arg));
}

void ChromiumEnv::StartThread(void (*function)(void* arg), void* arg) {
  new Thread(function, arg);  // Will self-delete.
}

LevelDBStatusValue GetLevelDBStatusUMAValue(const leveldb::Status& s) {
  if (s.ok())
    return LEVELDB_STATUS_OK;
  if (s.IsNotFound())
    return LEVELDB_STATUS_NOT_FOUND;
  if (s.IsCorruption())
    return LEVELDB_STATUS_CORRUPTION;
  if (s.IsNotSupportedError())
    return LEVELDB_STATUS_NOT_SUPPORTED;
  if (s.IsIOError())
    return LEVELDB_STATUS_IO_ERROR;
  // TODO(cmumford): IsInvalidArgument() was just added to leveldb. Use this
  // function once that change goes to the public repository.
  return LEVELDB_STATUS_INVALID_ARGUMENT;
}

// Forwards all calls to the underlying leveldb::DB instance.
// Adds / removes itself in the DBTracker it's created with.
class DBTracker::TrackedDBImpl : public base::LinkNode<TrackedDBImpl>,
                                 public TrackedDB {
 public:
  TrackedDBImpl(DBTracker* tracker,
                const std::string& name,
                leveldb::DB* db,
                const leveldb::Cache* block_cache,
                DatabaseErrorReportingCallback on_get_error,
                DatabaseErrorReportingCallback on_write_error)
      : tracker_(tracker),
        name_(name),
        db_(db),
        on_get_error_(std::move(on_get_error)),
        on_write_error_(std::move(on_write_error)) {
    if (leveldb_chrome::GetSharedWebBlockCache() ==
        leveldb_chrome::GetSharedBrowserBlockCache()) {
      shared_read_cache_use_ = SharedReadCacheUse_Unified;
    } else if (block_cache == leveldb_chrome::GetSharedBrowserBlockCache()) {
      shared_read_cache_use_ = SharedReadCacheUse_Browser;
    } else if (block_cache == leveldb_chrome::GetSharedWebBlockCache()) {
      shared_read_cache_use_ = SharedReadCacheUse_Web;
    } else if (block_cache == leveldb_chrome::GetSharedInMemoryBlockCache()) {
      shared_read_cache_use_ = SharedReadCacheUse_InMemory;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    tracker_->DatabaseOpened(this);
  }

  TrackedDBImpl(const TrackedDBImpl&) = delete;
  TrackedDBImpl& operator=(const TrackedDBImpl&) = delete;

  ~TrackedDBImpl() override {
    tracker_->DatabaseDestroyed(this);
    db_.reset();
  }

  const std::string& name() const override { return name_; }

  SharedReadCacheUse block_cache_type() const override {
    return shared_read_cache_use_;
  }

  leveldb::Status Put(const leveldb::WriteOptions& options,
                      const leveldb::Slice& key,
                      const leveldb::Slice& value) override {
    return db_->Put(options, key, value);
  }

  leveldb::Status Delete(const leveldb::WriteOptions& options,
                         const leveldb::Slice& key) override {
    return db_->Delete(options, key);
  }

  leveldb::Status Write(const leveldb::WriteOptions& options,
                        leveldb::WriteBatch* updates) override {
    leveldb::Status status = db_->Write(options, updates);
    if (status.ok()) [[likely]] {
      return status;
    }
    if (on_write_error_)
      on_write_error_.Run(status);
    return status;
  }

  leveldb::Status Get(const leveldb::ReadOptions& options,
                      const leveldb::Slice& key,
                      std::string* value) override {
    leveldb::Status status = db_->Get(options, key, value);
    if (status.ok() || status.IsNotFound()) [[likely]] {
      return status;
    }
    if (on_get_error_)
      on_get_error_.Run(status);
    return status;
  }

  const leveldb::Snapshot* GetSnapshot() override { return db_->GetSnapshot(); }

  void ReleaseSnapshot(const leveldb::Snapshot* snapshot) override {
    return db_->ReleaseSnapshot(snapshot);
  }

  bool GetProperty(const leveldb::Slice& property,
                   std::string* value) override {
    return db_->GetProperty(property, value);
  }

  void GetApproximateSizes(const leveldb::Range* range,
                           int n,
                           uint64_t* sizes) override {
    return db_->GetApproximateSizes(range, n, sizes);
  }

  void CompactRange(const leveldb::Slice* begin,
                    const leveldb::Slice* end) override {
    return db_->CompactRange(begin, end);
  }

  leveldb::Iterator* NewIterator(const leveldb::ReadOptions& options) override {
    return db_->NewIterator(options);
  }

 private:
  DBTracker* tracker_;
  std::string name_;
  std::unique_ptr<leveldb::DB> db_;
  SharedReadCacheUse shared_read_cache_use_;
  const DatabaseErrorReportingCallback on_get_error_;
  const DatabaseErrorReportingCallback on_write_error_;
};

// Reports live databases and in-memory env's to memory-infra. For each live
// database the following information is reported:
// 1. Instance pointer (to disambiguate databases).
// 2. Memory taken by the database, with the shared cache being attributed
// equally to each database sharing 3. The name of the database (when not in
// BACKGROUND mode to avoid exposing
//    PIIs in slow reports).
//
// Example report (as seen after clicking "leveldatabase" in "Overview" pane
// in Chrome tracing UI):
//
// Component                  effective_size  size        name
// ---------------------------------------------------------------------------
// leveldatabase              390 KiB         490 KiB
//   db_0x7FE70F2040A0        100 KiB         100 KiB     Users/.../Sync
//     block_cache (browser)  40 KiB          40 KiB
//   db_0x7FE70F530D80        150 KiB         150 KiB     Users/.../Data Proxy
//     block_cache (web)      30 KiB          30 KiB
//   db_0x7FE70F530D80        140 KiB         140 KiB     Users/.../Extensions
//     block_cache (web)      30 KiB          30 KiB
//   block_cache              0 KiB           100 KiB
//     browser                0 KiB           40 KiB
//     web                    0 KiB           60 KiB
//   memenv_0x7FE80F2040A0    4 KiB           4 KiB
//   memenv_0x7FE80F3040A0    4 KiB           4 KiB
//
class DBTracker::MemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  void DumpAllDatabases(ProcessMemoryDump* pmd);

  bool OnMemoryDump(const MemoryDumpArgs& args,
                    ProcessMemoryDump* pmd) override {
    DumpAllDatabases(pmd);
    return true;
  }

  void DatabaseOpened(const TrackedDBImpl* database) {
    database_use_count_[database->block_cache_type()]++;
  }

  void DatabaseDestroyed(const TrackedDBImpl* database) {
    database_use_count_[database->block_cache_type()]--;
    DCHECK_GE(database_use_count_[database->block_cache_type()], 0);
  }

 private:
  void DumpVisitor(ProcessMemoryDump* pmd, TrackedDB* db);

  int database_use_count_[SharedReadCacheUse_NumCacheUses] = {};
};

void DBTracker::MemoryDumpProvider::DumpAllDatabases(ProcessMemoryDump* pmd) {
  if (pmd->GetAllocatorDump("leveldatabase"))
    return;
  pmd->CreateAllocatorDump("leveldatabase");

  const auto* browser_cache = leveldb_chrome::GetSharedBrowserBlockCache();
  const auto* web_cache = leveldb_chrome::GetSharedWebBlockCache();
  if (browser_cache == web_cache) {
    RecordCacheUsageInTracing(pmd, SharedReadCacheUse_Unified);
  } else {
    RecordCacheUsageInTracing(pmd, SharedReadCacheUse_Browser);
    RecordCacheUsageInTracing(pmd, SharedReadCacheUse_Web);
  }
  RecordCacheUsageInTracing(pmd, SharedReadCacheUse_InMemory);

  DBTracker::GetInstance()->VisitDatabases(
      base::BindRepeating(&DBTracker::MemoryDumpProvider::DumpVisitor,
                          base::Unretained(this), base::Unretained(pmd)));
  leveldb_chrome::DumpAllTrackedEnvs(pmd);
}

void DBTracker::MemoryDumpProvider::DumpVisitor(ProcessMemoryDump* pmd,
                                                TrackedDB* db) {
  std::string db_dump_name = GetDumpNameForDB(db);

  auto* db_cache_dump = pmd->CreateAllocatorDump(db_dump_name + "/block_cache");
  const std::string cache_dump_name =
      GetDumpNameForCache(db->block_cache_type());
  pmd->AddSuballocation(db_cache_dump->guid(), cache_dump_name);
  size_t cache_usage =
      pmd->GetAllocatorDump(cache_dump_name)->GetSizeInternal();
  // The |database_use_count_| can be accessed by the visitor because the
  // visitor is called holding the lock at DBTracker.
  size_t cache_usage_pss =
      cache_usage / database_use_count_[db->block_cache_type()];
  db_cache_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                           MemoryAllocatorDump::kUnitsBytes, cache_usage_pss);

  auto* db_dump = pmd->CreateAllocatorDump(db_dump_name);
  uint64_t total_usage = 0;
  std::string usage_string;
  bool success =
      db->GetProperty("leveldb.approximate-memory-usage", &usage_string) &&
      base::StringToUint64(usage_string, &total_usage);
  DCHECK(success);
  db_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                     MemoryAllocatorDump::kUnitsBytes,
                     total_usage - cache_usage + cache_usage_pss);

  if (pmd->dump_args().level_of_detail !=
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    db_dump->AddString("name", "", db->name());
  }
}

DBTracker::DBTracker() : mdp_(new MemoryDumpProvider()) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      mdp_.get(), "LevelDB", nullptr);
}

DBTracker::~DBTracker() {
  NOTREACHED_IN_MIGRATION();  // DBTracker is a singleton
}

// static
DBTracker* DBTracker::GetInstance() {
  static DBTracker* instance = new DBTracker();
  return instance;
}

// static
MemoryAllocatorDump* DBTracker::GetOrCreateAllocatorDump(
    ProcessMemoryDump* pmd,
    leveldb::DB* tracked_db) {
  DCHECK(GetInstance()->IsTrackedDB(tracked_db))
      << std::hex << tracked_db << " is not tracked";

  // Create dumps for all databases to make sure the shared cache is equally
  // attributed to each database sharing it.
  GetInstance()->mdp_->DumpAllDatabases(pmd);
  return pmd->GetAllocatorDump(GetDumpNameForDB(tracked_db));
}

// static
MemoryAllocatorDump* DBTracker::GetOrCreateAllocatorDump(
    ProcessMemoryDump* pmd,
    leveldb::Env* tracked_memenv) {
  GetInstance()->mdp_->DumpAllDatabases(pmd);
  return leveldb_chrome::GetEnvAllocatorDump(pmd, tracked_memenv);
}

bool DBTracker::IsTrackedDB(const leveldb::DB* db) const {
  base::AutoLock lock(databases_lock_);
  for (auto* i = databases_.head(); i != databases_.end(); i = i->next()) {
    if (i->value() == db)
      return true;
  }
  return false;
}

leveldb::Status DBTracker::OpenDatabase(const leveldb_env::Options& options,
                                        const std::string& name,
                                        TrackedDB** dbptr) {
  leveldb::DB* db = nullptr;
  auto status = leveldb::DB::Open(options, name, &db);
  // Enforce expectations: either we succeed, and get a valid object in |db|,
  // or we fail, and |db| is still NULL.
  CHECK((status.ok() && db) || (!status.ok() && !db));
  if (status.ok()) {
    // TrackedDBImpl ctor adds the instance to the tracker.
    *dbptr = new TrackedDBImpl(GetInstance(), name, db, options.block_cache,
                               std::move(options.on_get_error),
                               std::move(options.on_write_error));
  }
  return status;
}

void DBTracker::VisitDatabases(const DatabaseVisitor& visitor) {
  base::AutoLock lock(databases_lock_);
  for (auto* i = databases_.head(); i != databases_.end(); i = i->next())
    visitor.Run(i->value());
}

void DBTracker::DatabaseOpened(TrackedDBImpl* database) {
  base::AutoLock lock(databases_lock_);
  databases_.Append(database);
  mdp_->DatabaseOpened(database);
}

void DBTracker::DatabaseDestroyed(TrackedDBImpl* database) {
  base::AutoLock lock(databases_lock_);
  mdp_->DatabaseDestroyed(database);
  database->RemoveFromList();
}

leveldb::Status OpenDB(const leveldb_env::Options& options,
                       const std::string& name,
                       std::unique_ptr<leveldb::DB>* dbptr) {
  if (!GetDBFactoryOverride().is_null()) {
    return GetDBFactoryOverride().Run(options, name, dbptr);
  }

  // For UMA logging purposes we need the block cache to be created outside of
  // leveldb so that the size can be logged and it can be pruned.
  DCHECK(options.block_cache != nullptr);
  DBTracker::TrackedDB* tracked_db = nullptr;
  leveldb::Status s;
  if (options.env && leveldb_chrome::IsMemEnv(options.env)) {
    Options mem_options = options;
    mem_options.block_cache = leveldb_chrome::GetSharedInMemoryBlockCache();
    // Minimum size to save memory and because writing is cheap.
    mem_options.write_buffer_size = 0;
    // All data is stored in memory so there's no cost to holding a "file" open.
    mem_options.max_open_files = std::numeric_limits<int>::max();
    mem_options.create_if_missing = true;
    s = DBTracker::GetInstance()->OpenDatabase(mem_options, name, &tracked_db);
  } else {
    std::string tmp_name = DatabaseNameForRewriteDB(name);
    // If Chrome crashes during rewrite, there might be a temporary db but
    // no actual db.
    if (options.env->FileExists(tmp_name) &&
        !options.env->FileExists(name + "/CURRENT")) {
      s = leveldb::DestroyDB(name, options);
      if (!s.ok())
        return s;
      s = options.env->RenameFile(tmp_name, name);
      if (!s.ok())
        return s;
    }
    s = DBTracker::GetInstance()->OpenDatabase(options, name, &tracked_db);
    // It is possible that the database was partially deleted during a
    // rewrite and can't be opened anymore.
    if (!s.ok() && options.env->FileExists(tmp_name)) {
      s = leveldb::DestroyDB(name, options);
      if (!s.ok())
        return s;
      s = options.env->RenameFile(tmp_name, name);
      if (!s.ok())
        return s;
      s = DBTracker::GetInstance()->OpenDatabase(options, name, &tracked_db);
    }
    // There might be a temporary database that needs to be cleaned up.
    if (options.env->FileExists(tmp_name)) {
      leveldb::DestroyDB(tmp_name, options);
    }
  }
  if (s.ok())
    dbptr->reset(tracked_db);
  return s;
}

void SetDBFactoryForTesting(DBFactoryMethod factory) {
  GetDBFactoryOverride() = factory;
}

leveldb::Status RewriteDB(const leveldb_env::Options& options,
                          const std::string& name,
                          std::unique_ptr<leveldb::DB>* dbptr) {
  if (leveldb_chrome::IsMemEnv(options.env))
    return Status::OK();
  DCHECK(options.create_if_missing);
  TRACE_EVENT1("leveldb", "ChromiumEnv::RewriteDB", "name", name);
  leveldb::Status s;
  std::string tmp_name = DatabaseNameForRewriteDB(name);
  if (options.env->FileExists(tmp_name)) {
    s = leveldb::DestroyDB(tmp_name, options);
    if (!s.ok())
      return s;
  }
  // Copy all data from *dbptr to a temporary db.
  std::unique_ptr<leveldb::DB> tmp_db;
  s = leveldb_env::OpenDB(options, tmp_name, &tmp_db);
  if (!s.ok())
    return s;
  std::unique_ptr<leveldb::Iterator> it(
      (*dbptr)->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    s = tmp_db->Put(leveldb::WriteOptions(), it->key(), it->value());
    if (!s.ok())
      break;
  }
  it.reset();
  tmp_db.reset();
  if (!s.ok()) {
    leveldb::DestroyDB(tmp_name, options);
    return s;
  }
  // Replace the old database with tmp_db.
  (*dbptr).reset();
  s = leveldb::DestroyDB(name, options);
  if (!s.ok())
    return s;
  s = options.env->RenameFile(tmp_name, name);
  if (!s.ok())
    return s;
  return leveldb_env::OpenDB(options, name, dbptr);
}

std::string_view MakeStringView(const leveldb::Slice& s) {
  return std::string_view(s.data(), s.size());
}

leveldb::Slice MakeSlice(std::string_view s) {
  return leveldb::Slice(s.data(), s.size());
}

leveldb::Slice MakeSlice(base::span<const uint8_t> s) {
  return MakeSlice(
      std::string_view(reinterpret_cast<const char*>(s.data()), s.size()));
}

}  // namespace leveldb_env

namespace leveldb {

Env* Env::Default() {
  static base::NoDestructor<leveldb_env::ChromiumEnv> default_env;
  return default_env.get();
}

}  // namespace leveldb

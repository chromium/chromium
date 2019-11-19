// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "third_party/leveldatabase/env_chromium.h"

#include <atomic>
#include <limits>
#include <utility>

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <dirent.h>
#include <sys/types.h>
#endif

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/leveldatabase/chromium_logger.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/leveldb_features.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/re2/src/re2/re2.h"

#if defined(OS_WIN)
#undef DeleteFile
#define base_DeleteFile base::DeleteFileW
#else  // defined(OS_WIN)
#define base_DeleteFile base::DeleteFile
#endif  // defined(OS_WIN)

using base::FilePath;
using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::ProcessMemoryDump;
using leveldb::FileLock;
using leveldb::Slice;
using leveldb::Status;

const base::Feature kLevelDBFileHandleEviction{
    "LevelDBFileHandleEviction", base::FEATURE_ENABLED_BY_DEFAULT};

namespace leveldb_env {
namespace {

// After this limit we don't bother doing file eviction for leveldb for speed,
// memory usage, and simplicity.
const constexpr size_t kFileLimitToDisableEviction = 10'000;

const FilePath::CharType table_extension[] = FILE_PATH_LITERAL(".ldb");

static const FilePath::CharType kLevelDBTestDirectoryPrefix[] =
    FILE_PATH_LITERAL("leveldb-test-");

// This name should not be changed or users involved in a crash might not be
// able to recover data.
static const char kDatabaseNameSuffixForRebuildDB[] = "__tmp_for_rebuild";

// Making direct platform in lieu of using base::FileEnumerator because the
// latter can fail quietly without return an error result.
static base::File::Error GetDirectoryEntries(const FilePath& dir_param,
                                             std::vector<FilePath>* result) {
  TRACE_EVENT0("leveldb", "ChromiumEnv::GetDirectoryEntries");
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  result->clear();
#if defined(OS_WIN)
  FilePath dir_filepath = dir_param.Append(FILE_PATH_LITERAL("*"));
  WIN32_FIND_DATA find_data;
  HANDLE find_handle = FindFirstFile(dir_filepath.value().c_str(), &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    DWORD last_error = GetLastError();
    if (last_error == ERROR_FILE_NOT_FOUND)
      return base::File::FILE_OK;
    return base::File::OSErrorToFileError(last_error);
  }
  do {
    FilePath filepath(find_data.cFileName);
    FilePath::StringType basename = filepath.BaseName().value();
    if (basename == FILE_PATH_LITERAL(".") ||
        basename == FILE_PATH_LITERAL(".."))
      continue;
    result->push_back(filepath.BaseName());
  } while (FindNextFile(find_handle, &find_data));
  DWORD last_error = GetLastError();
  base::File::Error return_value = base::File::FILE_OK;
  if (last_error != ERROR_NO_MORE_FILES)
    return_value = base::File::OSErrorToFileError(last_error);
  FindClose(find_handle);
  return return_value;
#else
  const std::string dir_string = dir_param.AsUTF8Unsafe();
  DIR* dir = opendir(dir_string.c_str());
  int saved_errno;
  if (!dir) {
    saved_errno = errno;
    VLOG(1) << "Error " << saved_errno << " opening directory \"" << dir_string
            << '"';
    return base::File::OSErrorToFileError(saved_errno);
  }
  struct dirent* dent;
  while ((errno = 0, dent = readdir(dir)) != nullptr) {
    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;
    result->push_back(FilePath::FromUTF8Unsafe(dent->d_name));
  }
  saved_errno = errno;
  closedir(dir);
  if (saved_errno != 0) {
    VLOG(1) << "Error " << saved_errno << " listing entries in \"" << dir_string
            << '"';
    return base::File::OSErrorToFileError(saved_errno);
  }
  return base::File::FILE_OK;
#endif
}

// To avoid a dependency on storage_histograms.h and the storageLib,
// we re-implement the BytesCountHistogram functions here.
void RecordStorageBytesWritten(const char* label, int amount) {
  const std::string name = "Storage.BytesWritten.";
  base::UmaHistogramCounts10M(name + label, amount);
}

void RecordStorageBytesRead(const char* label, int amount) {
  const std::string name = "Storage.BytesRead.";
  base::UmaHistogramCounts10M(name + label, amount);
}

class ChromiumFileLock : public FileLock {
 public:
  ChromiumFileLock(base::File file, const std::string& name)
      : file_(std::move(file)), name_(name) {}

  base::File file_;
  std::string name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumFileLock);
};

class Retrier {
 public:
  Retrier(MethodID method, RetrierProvider* provider)
      : start_(base::TimeTicks::Now()),
        limit_(start_ + base::TimeDelta::FromMilliseconds(
                            provider->MaxRetryTimeMillis())),
        last_(start_),
        time_to_sleep_(base::TimeDelta::FromMilliseconds(10)),
        success_(true),
        method_(method),
        last_error_(base::File::FILE_OK),
        provider_(provider) {}
  ~Retrier() {
    if (success_) {
      provider_->GetRetryTimeHistogram(method_)->AddTime(last_ - start_);
      if (last_error_ != base::File::FILE_OK) {
        DCHECK_LT(last_error_, 0);
        provider_->GetRecoveredFromErrorHistogram(method_)->Add(-last_error_);
      }
    }
  }
  bool ShouldKeepTrying(base::File::Error last_error) {
    DCHECK_NE(last_error, base::File::FILE_OK);
    last_error_ = last_error;
    if (last_ < limit_) {
      base::PlatformThread::Sleep(time_to_sleep_);
      last_ = base::TimeTicks::Now();
      return true;
    }
    success_ = false;
    return false;
  }

 private:
  base::TimeTicks start_;
  base::TimeTicks limit_;
  base::TimeTicks last_;
  base::TimeDelta time_to_sleep_;
  bool success_;
  MethodID method_;
  base::File::Error last_error_;
  RetrierProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(Retrier);
};

class ChromiumSequentialFile : public leveldb::SequentialFile {
 public:
  ChromiumSequentialFile(const std::string& fname,
                         base::File f,
                         const UMALogger* uma_logger)
      : filename_(fname), file_(std::move(f)), uma_logger_(uma_logger) {}
  virtual ~ChromiumSequentialFile() {}

  // Note: This method is relatively hot during leveldb database
  // compaction. Please avoid making them slower.
  Status Read(size_t n, Slice* result, char* scratch) override {
    int bytes_read = file_.ReadAtCurrentPosNoBestEffort(scratch, n);
    if (bytes_read == -1) {
      base::File::Error error = base::File::GetLastFileError();
      uma_logger_->RecordErrorAt(kSequentialFileRead);
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         kSequentialFileRead, error);
    }
    if (bytes_read > 0)
      uma_logger_->RecordBytesRead(bytes_read);
    *result = Slice(scratch, bytes_read);
    return Status::OK();
  }

  Status Skip(uint64_t n) override {
    if (file_.Seek(base::File::FROM_CURRENT, n) == -1) {
      base::File::Error error = base::File::GetLastFileError();
      uma_logger_->RecordErrorAt(kSequentialFileSkip);
      return MakeIOError(filename_, base::File::ErrorToString(error),
                         kSequentialFileSkip, error);
    } else {
      return Status::OK();
    }
  }

 private:
  std::string filename_;
  base::File file_;
  const UMALogger* const uma_logger_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumSequentialFile);
};

void DeleteFile(const Slice& key, void* value) {
  delete static_cast<base::File*>(value);
};

Status ReadFromFileToScratch(uint64_t offset,
                             size_t n,
                             Slice* result,
                             char* scratch,
                             base::File* file,
                             const base::FilePath& file_path,
                             const UMALogger* uma_logger) {
  int bytes_read = file->Read(offset, scratch, n);
  if (bytes_read < 0) {
    uma_logger->RecordErrorAt(kRandomAccessFileRead);
    return MakeIOError(file_path.AsUTF8Unsafe(), "Could not perform read",
                       kRandomAccessFileRead);
  }
  *result = Slice(scratch, (bytes_read < 0) ? 0 : bytes_read);
  if (bytes_read > 0)
    uma_logger->RecordBytesRead(bytes_read);

  return Status::OK();
}

// The cache mechanism uses leveldb's LRU cache, which is a threadsafe sharded
// LRU cache. The keys use the pointer value of |this|, and the values are file
// objects.
// Each object will only use its own cache entry, so the |Erase| call in the
// object destructor should synchronously delete the file from the cache. This
// ensures that pointer location re-use won't re-use an entry in the cache as
// the entry at |this| will always have been deleted.
// Files are always cleaned up with |DeleteFile|, which will be called when the
// ChromiumEvictableRandomAccessFile is deleted, the cache is deleted, or the
// file is evicted.
class ChromiumEvictableRandomAccessFile : public leveldb::RandomAccessFile {
 public:
  ChromiumEvictableRandomAccessFile(base::FilePath file_path,
                                    base::File file,
                                    leveldb::Cache* file_cache,
                                    const UMALogger* uma_logger)
      : filepath_(std::move(file_path)),
        uma_logger_(uma_logger),
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
                                             1 /* charge */, &DeleteFile));
  }

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
      auto new_file = std::make_unique<base::File>(filepath_, flags);
      if (!new_file->IsValid()) {
        return MakeIOError(filepath_.AsUTF8Unsafe(), "Could not perform read",
                           kRandomAccessFileRead);
      }
      handle = file_cache_->Insert(cache_key_, new_file.release(),
                                   sizeof(base::File), &DeleteFile);
    }
    base::File* file = static_cast<base::File*>(file_cache_->Value(handle));
    Status status = ReadFromFileToScratch(offset, n, result, scratch, file,
                                          filepath_, uma_logger_);
    file_cache_->Release(handle);
    return status;
  }

 private:
  const base::FilePath filepath_;
  const UMALogger* const uma_logger_;
  mutable leveldb::Cache* file_cache_;
  const ChromiumEvictableRandomAccessFile* cache_key_data_;
  leveldb::Slice cache_key_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumEvictableRandomAccessFile);
};

class ChromiumRandomAccessFile : public leveldb::RandomAccessFile {
 public:
  ChromiumRandomAccessFile(base::FilePath file_path,
                           base::File file,
                           const UMALogger* uma_logger)
      : filepath_(std::move(file_path)),
        file_(std::move(file)),
        uma_logger_(uma_logger) {}

  virtual ~ChromiumRandomAccessFile() {}

  // Note: This method is relatively hot during leveldb database
  // compaction. Please avoid making them slower.
  Status Read(uint64_t offset,
              size_t n,
              Slice* result,
              char* scratch) const override {
    return ReadFromFileToScratch(offset, n, result, scratch, &file_, filepath_,
                                 uma_logger_);
  }

 private:
  const base::FilePath filepath_;
  mutable base::File file_;
  const UMALogger* uma_logger_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumRandomAccessFile);
};

class ChromiumWritableFile : public leveldb::WritableFile {
 public:
  ChromiumWritableFile(const std::string& fname,
                       base::File f,
                       const UMALogger* uma_logger);
  virtual ~ChromiumWritableFile() {}
  leveldb::Status Append(const leveldb::Slice& data) override;
  leveldb::Status Close() override;
  leveldb::Status Flush() override;
  leveldb::Status Sync() override;

 private:
  enum Type { kManifest, kTable, kOther };
  leveldb::Status SyncParent();

  std::string filename_;
  base::File file_;
  const UMALogger* uma_logger_;
  Type file_type_;
  std::string parent_dir_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumWritableFile);
};

ChromiumWritableFile::ChromiumWritableFile(const std::string& fname,
                                           base::File f,
                                           const UMALogger* uma_logger)
    : filename_(fname),
      file_(std::move(f)),
      uma_logger_(uma_logger),
      file_type_(kOther) {
  DCHECK(uma_logger);
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  if (path.BaseName().AsUTF8Unsafe().find("MANIFEST") == 0)
    file_type_ = kManifest;
  else if (path.MatchesExtension(table_extension))
    file_type_ = kTable;
  parent_dir_ = FilePath::FromUTF8Unsafe(fname).DirName().AsUTF8Unsafe();
}

Status ChromiumWritableFile::SyncParent() {
  TRACE_EVENT0("leveldb", "SyncParent");
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  FilePath path = FilePath::FromUTF8Unsafe(parent_dir_);
  base::File f(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!f.IsValid()) {
    uma_logger_->RecordOSError(kSyncParent, f.error_details());
    return MakeIOError(parent_dir_, "Unable to open directory", kSyncParent,
                       f.error_details());
  }
  if (!f.Flush()) {
    base::File::Error error = base::File::GetLastFileError();
    uma_logger_->RecordOSError(kSyncParent, error);
    return MakeIOError(parent_dir_, base::File::ErrorToString(error),
                       kSyncParent, error);
  }
#endif
  return Status::OK();
}

Status ChromiumWritableFile::Append(const Slice& data) {
  DCHECK(file_.IsValid());
  DCHECK(uma_logger_);
  int bytes_written = file_.WriteAtCurrentPos(data.data(), data.size());
  if (static_cast<size_t>(bytes_written) != data.size()) {
    base::File::Error error = base::File::GetLastFileError();
    uma_logger_->RecordOSError(kWritableFileAppend, error);
    return MakeIOError(filename_, base::File::ErrorToString(error),
                       kWritableFileAppend, error);
  }
  if (bytes_written > 0)
    uma_logger_->RecordBytesWritten(bytes_written);
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
    Status status = SyncParent();
    if (!status.ok())
      return status;
  }

  if (!file_.Flush()) {
    base::File::Error error = base::File::GetLastFileError();
    uma_logger_->RecordErrorAt(kWritableFileSync);
    return MakeIOError(filename_, base::File::ErrorToString(error),
                       kWritableFileSync, error);
  }

  return Status::OK();
}

base::LazyInstance<ChromiumEnv>::Leaky default_env = LAZY_INSTANCE_INITIALIZER;

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
      NOTREACHED();
  }
  NOTREACHED();
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
      NOTREACHED();
  }
  if (!cache_ptr)
    return;
  CreateDumpMalloced(pmd, name, cache_ptr->TotalCharge());
}

}  // unnamed namespace

Options::Options() {
// Note: Ensure that these default values correspond to those in
// components/services/leveldb/public/mojom/leveldb.mojom.
// TODO(cmumford) Create struct-trait for leveldb.mojom.OpenOptions to force
// users to pass in a leveldb_env::Options instance (and it's defaults).
//
// Currently log reuse is an experimental feature in leveldb. More info at:
// https://github.com/google/leveldb/commit/251ebf5dc70129ad3
#if defined(OS_CHROMEOS)
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
  reuse_logs = !base::SysInfo::IsLowEndDevice();
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
    case kDeleteFile:
      return "DeleteFile";
    case kCreateDir:
      return "CreateDir";
    case kDeleteDir:
      return "DeleteDir";
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
    case kNumEntries:
      NOTREACHED();
      return "kNumEntries";
  }
  NOTREACHED();
  return "Unknown";
}

Status MakeIOError(Slice filename,
                   const std::string& message,
                   MethodID method,
                   base::File::Error error) {
  DCHECK_LT(error, 0);
  char buf[512];
  base::snprintf(buf, sizeof(buf), "%s (ChromeMethodBFE: %d::%s::%d)",
           message.c_str(), method, MethodIDToString(method), -error);
  // TOOD(crbug.com/760362): Map base::File::FILE_ERROR_NOT_FOUND to
  //                         Status::NotFound, after fixing LevelDB to handle
  //                         the NotFound correctly.
  return Status::IOError(filename, buf);
}

Status MakeIOError(Slice filename,
                   const std::string& message,
                   MethodID method) {
  char buf[512];
  base::snprintf(buf, sizeof(buf), "%s (ChromeMethodOnly: %d::%s)",
                 message.c_str(), method, MethodIDToString(method));
  return Status::IOError(filename, buf);
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
  const size_t kNumPatterns = base::size(patterns);
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
  return base::size(patterns) + 1;
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

ChromiumEnv::ChromiumEnv() : ChromiumEnv("LevelDBEnv") {}

ChromiumEnv::ChromiumEnv(const std::string& name)
    : kMaxRetryTimeMillis(1000),
      name_(name),
      bgsignal_(&mu_),
      started_bgthread_(false) {
  size_t max_open_files = base::GetMaxFds();
  if (base::FeatureList::IsEnabled(kLevelDBFileHandleEviction) &&
      max_open_files < kFileLimitToDisableEviction) {
    file_cache_.reset(
        leveldb::NewLRUCache(GetLevelDBFileLimit(max_open_files)));
  }
  uma_ioerror_base_name_ = name_ + ".IOError.BFE";
}

ChromiumEnv::~ChromiumEnv() {
  // In chromium, ChromiumEnv is leaked. It'd be nice to add NOTREACHED here to
  // ensure that behavior isn't accidentally changed, but there's an instance in
  // a unit test that is deleted.
}

bool ChromiumEnv::FileExists(const std::string& fname) {
  return base::PathExists(FilePath::FromUTF8Unsafe(fname));
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
      NOTREACHED();
  }
  NOTIMPLEMENTED();
  return "Unknown error.";
}

// Delete unused table backup files - a feature no longer supported.
// TODO(cmumford): Delete this function once found backup files drop below some
//                 very small (TBD) number.
void ChromiumEnv::DeleteBackupFiles(const FilePath& dir) {
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      "LevelDBEnv.DeleteTableBackupFile",
      base::Histogram::kUmaTargetedHistogramFlag);

  base::FileEnumerator dir_reader(dir, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.bak"));
  for (base::FilePath fname = dir_reader.Next(); !fname.empty();
       fname = dir_reader.Next()) {
    histogram->AddBoolean(base_DeleteFile(fname, false));
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
  DeleteBackupFiles(dir_path);

  std::vector<FilePath> entries;
  base::File::Error error = GetDirectoryEntries(dir_path, &entries);
  if (error != base::File::FILE_OK) {
    RecordOSError(kGetChildren, error);
    return MakeIOError(dir, "Could not open/read directory", kGetChildren,
                       error);
  }

  result->clear();
  for (const auto& entry : entries)
    result->push_back(entry.BaseName().AsUTF8Unsafe());

  return Status::OK();
}

Status ChromiumEnv::DeleteFile(const std::string& fname) {
  Status result;
  FilePath fname_filepath = FilePath::FromUTF8Unsafe(fname);
  // TODO(jorlow): Should we assert this is a file?
  if (!base_DeleteFile(fname_filepath, false)) {
    result = MakeIOError(fname, "Could not delete file.", kDeleteFile);
    RecordErrorAt(kDeleteFile);
  }
  return result;
}

Status ChromiumEnv::CreateDir(const std::string& name) {
  Status result;
  base::File::Error error = base::File::FILE_OK;
  Retrier retrier(kCreateDir, this);
  do {
    if (base::CreateDirectoryAndGetError(FilePath::FromUTF8Unsafe(name),
                                         &error))
      return result;
  } while (retrier.ShouldKeepTrying(error));
  result = MakeIOError(name, "Could not create directory.", kCreateDir, error);
  RecordOSError(kCreateDir, error);
  return result;
}

Status ChromiumEnv::DeleteDir(const std::string& name) {
  Status result;
  // TODO(jorlow): Should we assert this is a directory?
  if (!base_DeleteFile(FilePath::FromUTF8Unsafe(name), false)) {
    result = MakeIOError(name, "Could not delete directory.", kDeleteDir);
    RecordErrorAt(kDeleteDir);
  }
  return result;
}

Status ChromiumEnv::GetFileSize(const std::string& fname, uint64_t* size) {
  Status s;
  int64_t signed_size;
  if (!base::GetFileSize(FilePath::FromUTF8Unsafe(fname), &signed_size)) {
    *size = 0;
    s = MakeIOError(fname, "Could not determine file size.", kGetFileSize);
    RecordErrorAt(kGetFileSize);
  } else {
    *size = static_cast<uint64_t>(signed_size);
  }
  return s;
}

Status ChromiumEnv::RenameFile(const std::string& src, const std::string& dst) {
  Status result;
  FilePath src_file_path = FilePath::FromUTF8Unsafe(src);
  if (!base::PathExists(src_file_path))
    return result;
  FilePath destination = FilePath::FromUTF8Unsafe(dst);

  Retrier retrier(kRenameFile, this);
  base::File::Error error = base::File::FILE_OK;
  do {
    if (base::ReplaceFile(src_file_path, destination, &error))
      return result;
  } while (retrier.ShouldKeepTrying(error));

  DCHECK(error != base::File::FILE_OK);
  RecordOSError(kRenameFile, error);
  char buf[100];
  base::snprintf(buf,
           sizeof(buf),
           "Could not rename file: %s",
           FileErrorString(error));
  return MakeIOError(src, buf, kRenameFile, error);
}

Status ChromiumEnv::LockFile(const std::string& fname, FileLock** lock) {
  *lock = NULL;
  Status result;
  int flags = base::File::FLAG_OPEN_ALWAYS |
              base::File::FLAG_READ |
              base::File::FLAG_WRITE;
  base::File::Error error_code;
  base::File file;
  Retrier retrier(kLockFile, this);
  do {
    file.Initialize(FilePath::FromUTF8Unsafe(fname), flags);
    if (!file.IsValid())
      error_code = file.error_details();
  } while (!file.IsValid() && retrier.ShouldKeepTrying(error_code));

  if (!file.IsValid()) {
    result =
        MakeIOError(fname, FileErrorString(error_code), kLockFile, error_code);
    RecordOSError(kLockFile, error_code);
    return result;
  }

  if (!locks_.Insert(fname)) {
    result = MakeIOError(fname, "Lock file already locked.", kLockFile);
    return result;
  }

#if !defined(OS_FUCHSIA)
  Retrier lock_retrier(kLockFile, this);
  do {
    error_code = file.Lock();
  } while (error_code != base::File::FILE_OK &&
           retrier.ShouldKeepTrying(error_code));

  if (error_code != base::File::FILE_OK) {
    locks_.Remove(fname);
    result = MakeIOError(fname, FileErrorString(error_code), kLockFile,
                         error_code);
    RecordOSError(kLockFile, error_code);
    return result;
  }
#endif  // !defined(OS_FUCHSIA)

  *lock = new ChromiumFileLock(std::move(file), fname);
  return result;
}

Status ChromiumEnv::UnlockFile(FileLock* lock) {
  std::unique_ptr<ChromiumFileLock> my_lock(
      reinterpret_cast<ChromiumFileLock*>(lock));
  Status result = Status::OK();

#if !defined(OS_FUCHSIA)
  base::File::Error error_code = my_lock->file_.Unlock();
  if (error_code != base::File::FILE_OK) {
    result =
        MakeIOError(my_lock->name_, "Could not unlock lock file.", kUnlockFile);
    RecordOSError(kUnlockFile, error_code);
  }
#endif  // !defined(OS_FUCHSIA)

  bool removed = locks_.Remove(my_lock->name_);
  DCHECK(removed);
  return result;
}

Status ChromiumEnv::GetTestDirectory(std::string* path) {
  mu_.Acquire();
  if (test_directory_.empty()) {
    if (!base::CreateNewTempDirectory(kLevelDBTestDirectoryPrefix,
                                      &test_directory_)) {
      mu_.Release();
      RecordErrorAt(kGetTestDirectory);
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
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  base::File f(path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!f.IsValid()) {
    *result = NULL;
    RecordOSError(kNewLogger, f.error_details());
    return MakeIOError(fname, "Unable to create log file", kNewLogger,
                       f.error_details());
  } else {
    *result = new leveldb::ChromiumLogger(std::move(f));
    return Status::OK();
  }
}

Status ChromiumEnv::NewSequentialFile(const std::string& fname,
                                      leveldb::SequentialFile** result) {
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  base::File f(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!f.IsValid()) {
    *result = NULL;
    RecordOSError(kNewSequentialFile, f.error_details());
    return MakeIOError(fname, "Unable to create sequential file",
                       kNewSequentialFile, f.error_details());
  } else {
    *result = new ChromiumSequentialFile(fname, std::move(f), this);
    return Status::OK();
  }
}

Status ChromiumEnv::NewRandomAccessFile(const std::string& fname,
                                        leveldb::RandomAccessFile** result) {
  int flags = base::File::FLAG_READ | base::File::FLAG_OPEN;
  base::FilePath file_path = FilePath::FromUTF8Unsafe(fname);
  base::File file(file_path, flags);
  if (file.IsValid()) {
    if (file_cache_) {
      *result = new ChromiumEvictableRandomAccessFile(
          std::move(file_path), std::move(file), file_cache_.get(), this);
    } else {
      *result = new ChromiumRandomAccessFile(std::move(file_path),
                                             std::move(file), this);
    }
    return Status::OK();
  }
  base::File::Error error_code = file.error_details();
  *result = NULL;
  RecordOSError(kNewRandomAccessFile, error_code);
  return MakeIOError(fname, FileErrorString(error_code), kNewRandomAccessFile,
                     error_code);
}

Status ChromiumEnv::NewWritableFile(const std::string& fname,
                                    leveldb::WritableFile** result) {
  *result = NULL;
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  base::File f(path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!f.IsValid()) {
    RecordErrorAt(kNewWritableFile);
    return MakeIOError(fname, "Unable to create writable file",
                       kNewWritableFile, f.error_details());
  } else {
    *result = new ChromiumWritableFile(fname, std::move(f), this);
    return Status::OK();
  }
}

Status ChromiumEnv::NewAppendableFile(const std::string& fname,
                                      leveldb::WritableFile** result) {
  *result = NULL;
  FilePath path = FilePath::FromUTF8Unsafe(fname);
  base::File f(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!f.IsValid()) {
    RecordErrorAt(kNewAppendableFile);
    return MakeIOError(fname, "Unable to create appendable file",
                       kNewAppendableFile, f.error_details());
  }
  *result = new ChromiumWritableFile(fname, std::move(f), this);
  return Status::OK();
}

uint64_t ChromiumEnv::NowMicros() {
  return base::TimeTicks::Now().ToInternalValue();
}

void ChromiumEnv::SleepForMicroseconds(int micros) {
  // Round up to the next millisecond.
  base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(micros));
}

void ChromiumEnv::RecordErrorAt(MethodID method) const {
  GetMethodIOErrorHistogram()->Add(method);
}

void ChromiumEnv::RecordOSError(MethodID method,
                                base::File::Error error) const {
  DCHECK_LT(error, 0);
  RecordErrorAt(method);
  GetOSErrorHistogram(method, -base::File::FILE_ERROR_MAX)->Add(-error);
}

void ChromiumEnv::RecordBytesRead(int amount) const {
  RecordStorageBytesRead(name_.c_str(), amount);
}

void ChromiumEnv::RecordBytesWritten(int amount) const {
  RecordStorageBytesWritten(name_.c_str(), amount);
}

base::HistogramBase* ChromiumEnv::GetOSErrorHistogram(MethodID method,
                                                      int limit) const {
  std::string uma_name;
  base::StringAppendF(&uma_name, "%s.%s", uma_ioerror_base_name_.c_str(),
                      MethodIDToString(method));
  return base::LinearHistogram::FactoryGet(uma_name, 1, limit, limit + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetMethodIOErrorHistogram() const {
  std::string uma_name(name_);
  uma_name.append(".IOError");
  return base::LinearHistogram::FactoryGet(uma_name, 1, kNumEntries,
      kNumEntries + 1, base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetRetryTimeHistogram(MethodID method) const {
  std::string uma_name(name_);
  // TODO(dgrogan): This is probably not the best way to concatenate strings.
  uma_name.append(".TimeUntilSuccessFor").append(MethodIDToString(method));

  const int kBucketSizeMillis = 25;
  // Add 2, 1 for each of the buckets <1 and >max.
  const int kNumBuckets = kMaxRetryTimeMillis / kBucketSizeMillis + 2;
  return base::Histogram::FactoryTimeGet(
      uma_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(kMaxRetryTimeMillis + 1),
      kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetRecoveredFromErrorHistogram(
    MethodID method) const {
  std::string uma_name(name_);
  uma_name.append(".RetryRecoveredFromErrorIn")
      .append(MethodIDToString(method));
  return base::LinearHistogram::FactoryGet(uma_name, 1, kNumEntries,
      kNumEntries + 1, base::Histogram::kUmaTargetedHistogramFlag);
}

class Thread : public base::PlatformThread::Delegate {
 public:
  Thread(void (*function)(void* arg), void* arg)
      : function_(function), arg_(arg) {
    base::PlatformThreadHandle handle;
    bool success = base::PlatformThread::Create(0, this, &handle);
    DCHECK(success);
  }
  virtual ~Thread() {}
  void ThreadMain() override {
    (*function_)(arg_);
    delete this;
  }

 private:
  void (*function_)(void* arg);
  void* arg_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

void ChromiumEnv::Schedule(ScheduleFunc* function, void* arg) {
  mu_.Acquire();

  // Start background thread if necessary
  if (!started_bgthread_) {
    started_bgthread_ = true;
    StartThread(&ChromiumEnv::BGThreadWrapper, this);
  }

  // If the queue is currently empty, the background thread may currently be
  // waiting.
  if (queue_.empty()) {
    bgsignal_.Signal();
  }

  // Add to priority queue
  queue_.push_back(BGItem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  mu_.Release();
}

void ChromiumEnv::BGThread() {
  base::PlatformThread::SetName(name_.c_str());

  while (true) {
    // Wait until there is an item that is ready to run
    mu_.Acquire();
    while (queue_.empty()) {
      bgsignal_.Wait();
    }

    void (*function)(void*) = queue_.front().function;
    void* arg = queue_.front().arg;
    queue_.pop_front();

    mu_.Release();
    TRACE_EVENT0("leveldb", "ChromiumEnv::BGThread-Task");
    (*function)(arg);
  }
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
                const std::string name,
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
      NOTREACHED();
    }
    tracker_->DatabaseOpened(this, shared_read_cache_use_);
  }

  ~TrackedDBImpl() override {
    tracker_->DatabaseDestroyed(this, shared_read_cache_use_);
    base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
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
    if (LIKELY(status.ok()))
      return status;
    if (on_write_error_)
      on_write_error_.Run(status);
    return status;
  }

  leveldb::Status Get(const leveldb::ReadOptions& options,
                      const leveldb::Slice& key,
                      std::string* value) override {
    leveldb::Status status = db_->Get(options, key, value);
    if (LIKELY(status.ok() || status.IsNotFound()))
      return status;
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

  DISALLOW_COPY_AND_ASSIGN(TrackedDBImpl);
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

  int database_use_count(SharedReadCacheUse cache) {
    return database_use_count_[cache];
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
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    db_dump->AddString("name", "", db->name());
  }
}

DBTracker::DBTracker() : mdp_(new MemoryDumpProvider()) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      mdp_.get(), "LevelDB", nullptr);
}

DBTracker::~DBTracker() {
  NOTREACHED();  // DBTracker is a singleton
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

void DBTracker::UpdateHistograms() {
  base::AutoLock lock(databases_lock_);
  if (leveldb_chrome::GetSharedWebBlockCache() ==
      leveldb_chrome::GetSharedBrowserBlockCache()) {
    UMA_HISTOGRAM_COUNTS_100(
        "LevelDB.SharedCache.DBCount.Unified",
        mdp_->database_use_count(SharedReadCacheUse_Unified));
  } else {
    UMA_HISTOGRAM_COUNTS_100("LevelDB.SharedCache.DBCount.Web",
                             mdp_->database_use_count(SharedReadCacheUse_Web));
    UMA_HISTOGRAM_COUNTS_100(
        "LevelDB.SharedCache.DBCount.Browser",
        mdp_->database_use_count(SharedReadCacheUse_Browser));
  }
  UMA_HISTOGRAM_COUNTS_100(
      "LevelDB.SharedCache.DBCount.InMemory",
      mdp_->database_use_count(SharedReadCacheUse_InMemory));
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

void DBTracker::DatabaseOpened(TrackedDBImpl* database,
                               SharedReadCacheUse cache_use) {
  base::AutoLock lock(databases_lock_);
  databases_.Append(database);
  mdp_->DatabaseOpened(database);
}

void DBTracker::DatabaseDestroyed(TrackedDBImpl* database,
                                  SharedReadCacheUse cache_use) {
  base::AutoLock lock(databases_lock_);
  mdp_->DatabaseDestroyed(database);
  database->RemoveFromList();
}

leveldb::Status OpenDB(const leveldb_env::Options& options,
                       const std::string& name,
                       std::unique_ptr<leveldb::DB>* dbptr) {
  // For UMA logging purposes we need the block cache to be created outside of
  // leveldb so that the size can be logged and it can be pruned.
  DCHECK(options.block_cache != nullptr);
  DBTracker::TrackedDB* tracked_db = nullptr;
  leveldb::Status s;
  if (options.env && leveldb_chrome::IsMemEnv(options.env)) {
    Options mem_options = options;
    mem_options.block_cache = leveldb_chrome::GetSharedInMemoryBlockCache();
    mem_options.write_buffer_size = 0;  // minimum size.
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

leveldb::Status RewriteDB(const leveldb_env::Options& options,
                          const std::string& name,
                          std::unique_ptr<leveldb::DB>* dbptr) {
  DCHECK(options.create_if_missing);
  if (!base::FeatureList::IsEnabled(leveldb::kLevelDBRewriteFeature))
    return Status::OK();
  if (leveldb_chrome::IsMemEnv(options.env))
    return Status::OK();
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

base::StringPiece MakeStringPiece(const leveldb::Slice& s) {
  return base::StringPiece(s.data(), s.size());
}

leveldb::Slice MakeSlice(const base::StringPiece& s) {
  return leveldb::Slice(s.begin(), s.size());
}

leveldb::Slice MakeSlice(base::span<const uint8_t> s) {
  return MakeSlice(
      base::StringPiece(reinterpret_cast<const char*>(s.data()), s.size()));
}

}  // namespace leveldb_env

namespace leveldb {

Env* Env::Default() {
  return leveldb_env::default_env.Pointer();
}

}  // namespace leveldb

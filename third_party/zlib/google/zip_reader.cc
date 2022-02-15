// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zlib/google/zip_reader.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "third_party/zlib/google/zip_internal.h"

#if defined(USE_SYSTEM_MINIZIP)
#include <minizip/unzip.h>
#else
#include "third_party/zlib/contrib/minizip/unzip.h"
#if defined(OS_WIN)
#include "third_party/zlib/contrib/minizip/iowin32.h"
#endif  // defined(OS_WIN)
#endif  // defined(USE_SYSTEM_MINIZIP)

#if defined(OS_POSIX)
#include <sys/stat.h>
#endif

namespace zip {
namespace {

enum UnzipError : int;

std::ostream& operator<<(std::ostream& out, UnzipError error) {
#define SWITCH_ERR(X) \
  case X:             \
    return out << #X;
  switch (error) {
    SWITCH_ERR(UNZ_OK);
    SWITCH_ERR(UNZ_END_OF_LIST_OF_FILE);
    SWITCH_ERR(UNZ_ERRNO);
    SWITCH_ERR(UNZ_PARAMERROR);
    SWITCH_ERR(UNZ_BADZIPFILE);
    SWITCH_ERR(UNZ_INTERNALERROR);
    SWITCH_ERR(UNZ_CRCERROR);
    default:
      return out << "UNZ" << static_cast<int>(error);
  }
#undef SWITCH_ERR
}

struct Redact {
  explicit Redact(const base::FilePath& path) : path(path) {}
  const base::FilePath& path;
};

std::ostream& operator<<(std::ostream& out, Redact r) {
  return LOG_IS_ON(INFO) ? out << "'" << r.path << "'" : out << "(redacted)";
}

// StringWriterDelegate --------------------------------------------------------

// A writer delegate that writes no more than |max_read_bytes| to a given
// std::string.
class StringWriterDelegate : public WriterDelegate {
 public:
  StringWriterDelegate(size_t max_read_bytes, std::string* output);

  StringWriterDelegate(const StringWriterDelegate&) = delete;
  StringWriterDelegate& operator=(const StringWriterDelegate&) = delete;

  ~StringWriterDelegate() override;

  // WriterDelegate methods:

  // Returns true.
  bool PrepareOutput() override;

  // Appends |num_bytes| bytes from |data| to the output string. Returns false
  // if |num_bytes| will cause the string to exceed |max_read_bytes|.
  bool WriteBytes(const char* data, int num_bytes) override;

  void SetTimeModified(const base::Time& time) override;

  void SetPosixFilePermissions(int mode) override;

 private:
  size_t max_read_bytes_;
  std::string* output_;
};

StringWriterDelegate::StringWriterDelegate(size_t max_read_bytes,
                                           std::string* output)
    : max_read_bytes_(max_read_bytes), output_(output) {}

StringWriterDelegate::~StringWriterDelegate() {}

bool StringWriterDelegate::PrepareOutput() {
  return true;
}

bool StringWriterDelegate::WriteBytes(const char* data, int num_bytes) {
  if (output_->size() + num_bytes > max_read_bytes_)
    return false;
  output_->append(data, num_bytes);
  return true;
}

void StringWriterDelegate::SetTimeModified(const base::Time& time) {
  // Do nothing.
}

void StringWriterDelegate::SetPosixFilePermissions(int mode) {
  // Do nothing.
}

#if defined(OS_POSIX)
void SetPosixFilePermissions(int fd, int mode) {
  base::stat_wrapper_t sb;
  if (base::File::Fstat(fd, &sb)) {
    return;
  }
  mode_t new_mode = sb.st_mode;
  // Transfer the executable bit only if the file is readable.
  if ((sb.st_mode & S_IRUSR) == S_IRUSR && (mode & S_IXUSR) == S_IXUSR) {
    new_mode |= S_IXUSR;
  }
  if ((sb.st_mode & S_IRGRP) == S_IRGRP && (mode & S_IXGRP) == S_IXGRP) {
    new_mode |= S_IXGRP;
  }
  if ((sb.st_mode & S_IROTH) == S_IROTH && (mode & S_IXOTH) == S_IXOTH) {
    new_mode |= S_IXOTH;
  }
  if (new_mode != sb.st_mode) {
    fchmod(fd, new_mode);
  }
}
#endif

}  // namespace

ZipReader::ZipReader() {
  Reset();
}

ZipReader::~ZipReader() {
  Close();
}

bool ZipReader::Open(const base::FilePath& zip_path) {
  DCHECK(!zip_file_);

  // Use of "Unsafe" function does not look good, but there is no way to do
  // this safely on Linux. See file_util.h for details.
  zip_file_ = internal::OpenForUnzipping(zip_path.AsUTF8Unsafe());
  if (!zip_file_) {
    LOG(ERROR) << "Cannot open ZIP archive " << Redact(zip_path);
    return false;
  }

  return OpenInternal();
}

bool ZipReader::OpenFromPlatformFile(base::PlatformFile zip_fd) {
  DCHECK(!zip_file_);

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  zip_file_ = internal::OpenFdForUnzipping(zip_fd);
#elif defined(OS_WIN)
  zip_file_ = internal::OpenHandleForUnzipping(zip_fd);
#endif
  if (!zip_file_) {
    LOG(ERROR) << "Cannot open ZIP from file handle " << zip_fd;
    return false;
  }

  return OpenInternal();
}

bool ZipReader::OpenFromString(const std::string& data) {
  zip_file_ = internal::PrepareMemoryForUnzipping(data);
  if (!zip_file_)
    return false;
  return OpenInternal();
}

void ZipReader::Close() {
  if (zip_file_) {
    unzClose(zip_file_);
  }
  Reset();
}

bool ZipReader::HasMore() {
  return !reached_end_;
}

bool ZipReader::AdvanceToNextEntry() {
  DCHECK(zip_file_);

  // Should not go further if we already reached the end.
  if (reached_end_)
    return false;

  if (const int err = unzGoToNextFile(zip_file_); err != UNZ_OK) {
    reached_end_ = true;
    if (err != UNZ_END_OF_LIST_OF_FILE) {
      LOG(ERROR) << "Cannot go to next entry in ZIP: " << UnzipError(err);
      ok_ = false;
      return false;
    }
  }

  entry_ = {};
  current_entry_ = nullptr;
  return true;
}

const ZipReader::Entry* ZipReader::Next() {
  DCHECK(zip_file_);

  if (reached_end_)
    return nullptr;

  if (next_index_ > 0 && (!AdvanceToNextEntry() || reached_end_))
    return nullptr;

  next_index_++;

  if (!OpenCurrentEntryInZip()) {
    reached_end_ = true;
    ok_ = false;
    return nullptr;
  }

  return &entry_;
}

bool ZipReader::OpenCurrentEntryInZip() {
  DCHECK(zip_file_);

  current_entry_ = nullptr;

  // Get entry info.
  unz_file_info info = {};
  char path_in_zip[internal::kZipMaxPath] = {};
  if (const int err = unzGetCurrentFileInfo(zip_file_, &info, path_in_zip,
                                            sizeof(path_in_zip) - 1, nullptr, 0,
                                            nullptr, 0);
      err != UNZ_OK) {
    LOG(ERROR) << "Cannot get entry from ZIP: " << UnzipError(err);
    return false;
  }

  Entry& entry = entry_;
  entry.path_in_original_encoding = path_in_zip;

  // Convert path from original encoding to Unicode.
  std::u16string path_in_utf16;
  const char* const encoding = encoding_.empty() ? "UTF-8" : encoding_.c_str();
  if (!base::CodepageToUTF16(entry.path_in_original_encoding, encoding,
                             base::OnStringConversionError::SUBSTITUTE,
                             &path_in_utf16)) {
    LOG(ERROR) << "Cannot convert path from encoding " << encoding;
    return false;
  }

  entry.path = base::FilePath::FromUTF16Unsafe(path_in_utf16);
  entry.original_size = info.uncompressed_size;

  // Directory entries in ZIP have a path ending with "/".
  entry.is_directory = base::EndsWith(path_in_utf16, u"/");

  // Check the entry path for directory traversal issues. We consider entry
  // paths unsafe if they are absolute or if they contain "..". On Windows,
  // IsAbsolute() returns false for paths starting with "/".
  entry.is_unsafe = entry.path.ReferencesParent() || entry.path.IsAbsolute() ||
                    base::StartsWith(path_in_utf16, u"/");

  // The file content of this entry is encrypted if flag bit 0 is set.
  entry.is_encrypted = info.flag & 1;

  // Construct the last modified time. The timezone info is not present in ZIP
  // archives, so we construct the time as UTC.
  base::Time::Exploded exploded_time = {};
  exploded_time.year = info.tmu_date.tm_year;
  exploded_time.month = info.tmu_date.tm_mon + 1;  // 0-based vs 1-based
  exploded_time.day_of_month = info.tmu_date.tm_mday;
  exploded_time.hour = info.tmu_date.tm_hour;
  exploded_time.minute = info.tmu_date.tm_min;
  exploded_time.second = info.tmu_date.tm_sec;
  exploded_time.millisecond = 0;

  if (!base::Time::FromUTCExploded(exploded_time, &entry.last_modified))
    entry.last_modified = base::Time::UnixEpoch();

#if defined(OS_POSIX)
  entry.posix_mode = (info.external_fa >> 16L) & (S_IRWXU | S_IRWXG | S_IRWXO);
#else
  entry.posix_mode = 0;
#endif

  current_entry_ = &entry_;
  return true;
}

bool ZipReader::ExtractCurrentEntry(WriterDelegate* delegate,
                                    uint64_t num_bytes_to_extract) const {
  DCHECK(zip_file_);

  // Use password only for encrypted files. For non-encrypted files, no password
  // is needed, and must be nullptr.
  const char* const password =
      entry_.is_encrypted() ? password_.c_str() : nullptr;
  if (const int err = unzOpenCurrentFilePassword(zip_file_, password);
      err != UNZ_OK) {
    LOG(ERROR) << "Cannot open file " << Redact(entry_.path)
               << " from ZIP: " << UnzipError(err);
    return false;
  }

  DCHECK(delegate);
  if (!delegate->PrepareOutput())
    return false;

  uint64_t remaining_capacity = num_bytes_to_extract;
  bool entire_file_extracted = false;

  while (remaining_capacity > 0) {
    char buf[internal::kZipBufSize];
    const int num_bytes_read =
        unzReadCurrentFile(zip_file_, buf, internal::kZipBufSize);

    if (num_bytes_read == 0) {
      entire_file_extracted = true;
      break;
    }

    if (num_bytes_read < 0) {
      LOG(ERROR) << "Cannot read file " << Redact(entry_.path)
                 << " from ZIP: " << UnzipError(num_bytes_read);
      break;
    }

    DCHECK_LT(0, num_bytes_read);
    CHECK_LE(num_bytes_read, internal::kZipBufSize);

    uint64_t num_bytes_to_write = std::min<uint64_t>(
        remaining_capacity, base::checked_cast<uint64_t>(num_bytes_read));
    if (!delegate->WriteBytes(buf, num_bytes_to_write))
      break;

    if (remaining_capacity == base::checked_cast<uint64_t>(num_bytes_read)) {
      // Ensures function returns true if the entire file has been read.
      const int n = unzReadCurrentFile(zip_file_, buf, 1);
      entire_file_extracted = (n == 0);
      LOG_IF(ERROR, n < 0) << "Cannot read file " << Redact(entry_.path)
                           << " from ZIP: " << UnzipError(n);
    }

    CHECK_GE(remaining_capacity, num_bytes_to_write);
    remaining_capacity -= num_bytes_to_write;
  }

  if (entire_file_extracted) {
    delegate->SetPosixFilePermissions(current_entry_info()->posix_mode());
    if (current_entry_info()->last_modified() != base::Time::UnixEpoch()) {
      delegate->SetTimeModified(current_entry_info()->last_modified());
    }
  }

  if (const int err = unzCloseCurrentFile(zip_file_); err != UNZ_OK) {
    LOG(ERROR) << "Cannot extract file " << Redact(entry_.path)
               << " from ZIP: " << UnzipError(err);
    return false;
  }

  return entire_file_extracted;
}

void ZipReader::ExtractCurrentEntryToFilePathAsync(
    const base::FilePath& output_file_path,
    SuccessCallback success_callback,
    FailureCallback failure_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(zip_file_);
  DCHECK(current_entry_);

  // If this is a directory, just create it and return.
  if (current_entry_info()->is_directory()) {
    if (base::CreateDirectory(output_file_path)) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(success_callback));
    } else {
      LOG(ERROR) << "Cannot create directory " << Redact(output_file_path);
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(failure_callback));
    }
    return;
  }

  // Use password only for encrypted files. For non-encrypted files, no password
  // is needed, and must be nullptr.
  const char* const password =
      entry_.is_encrypted() ? password_.c_str() : nullptr;
  if (const int err = unzOpenCurrentFilePassword(zip_file_, password);
      err != UNZ_OK) {
    LOG(ERROR) << "Cannot open file " << Redact(entry_.path)
               << " from ZIP: " << UnzipError(err);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(failure_callback));
    return;
  }

  base::FilePath output_dir_path = output_file_path.DirName();
  if (!base::CreateDirectory(output_dir_path)) {
    LOG(ERROR) << "Cannot create directory " << Redact(output_dir_path);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(failure_callback));
    return;
  }

  const int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
  base::File output_file(output_file_path, flags);

  if (!output_file.IsValid()) {
    LOG(ERROR) << "Cannot create file " << Redact(output_file_path);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(failure_callback));
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ZipReader::ExtractChunk, weak_ptr_factory_.GetWeakPtr(),
                     std::move(output_file), std::move(success_callback),
                     std::move(failure_callback), progress_callback,
                     0 /* initial offset */));
}

bool ZipReader::ExtractCurrentEntryToString(uint64_t max_read_bytes,
                                            std::string* output) const {
  DCHECK(output);
  DCHECK(zip_file_);

  if (max_read_bytes == 0) {
    output->clear();
    return true;
  }

  if (current_entry_info()->is_directory()) {
    output->clear();
    return true;
  }

  // The original_size() is the best hint for the real size, so it saves
  // doing reallocations for the common case when the uncompressed size is
  // correct. However, we need to assume that the uncompressed size could be
  // incorrect therefore this function needs to read as much data as possible.
  std::string contents;
  contents.reserve(
      static_cast<size_t>(std::min(base::checked_cast<int64_t>(max_read_bytes),
                                   current_entry_info()->original_size())));

  StringWriterDelegate writer(max_read_bytes, &contents);
  if (!ExtractCurrentEntry(&writer, max_read_bytes)) {
    if (contents.length() < max_read_bytes) {
      // There was an error in extracting entry. If ExtractCurrentEntry()
      // returns false, the entire file was not read - in which case
      // contents.length() should equal |max_read_bytes| unless an error
      // occurred which caused extraction to be aborted.
      output->clear();
    } else {
      // |num_bytes| is less than the length of current entry.
      output->swap(contents);
    }
    return false;
  }
  output->swap(contents);
  return true;
}

bool ZipReader::OpenInternal() {
  DCHECK(zip_file_);

  unz_global_info zip_info = {};  // Zero-clear.
  if (const int err = unzGetGlobalInfo(zip_file_, &zip_info); err != UNZ_OK) {
    LOG(ERROR) << "Cannot get ZIP info: " << UnzipError(err);
    return false;
  }

  num_entries_ = zip_info.number_entry;
  reached_end_ = (num_entries_ <= 0);
  ok_ = true;
  return true;
}

void ZipReader::Reset() {
  zip_file_ = nullptr;
  num_entries_ = 0;
  next_index_ = 0;
  reached_end_ = false;
  ok_ = false;
  entry_ = {};
  current_entry_ = nullptr;
}

void ZipReader::ExtractChunk(base::File output_file,
                             SuccessCallback success_callback,
                             FailureCallback failure_callback,
                             const ProgressCallback& progress_callback,
                             int64_t offset) {
  char buffer[internal::kZipBufSize];

  const int num_bytes_read =
      unzReadCurrentFile(zip_file_, buffer, internal::kZipBufSize);

  if (num_bytes_read == 0) {
    if (const int err = unzCloseCurrentFile(zip_file_); err != UNZ_OK) {
      LOG(ERROR) << "Cannot extract file " << Redact(entry_.path)
                 << " from ZIP: " << UnzipError(err);
      std::move(failure_callback).Run();
      return;
    }

    std::move(success_callback).Run();
    return;
  }

  if (num_bytes_read < 0) {
    LOG(ERROR) << "Cannot read file " << Redact(entry_.path)
               << " from ZIP: " << UnzipError(num_bytes_read);
    std::move(failure_callback).Run();
    return;
  }

  if (num_bytes_read != output_file.Write(offset, buffer, num_bytes_read)) {
    LOG(ERROR) << "Cannot write " << num_bytes_read
               << " bytes to file at offset " << offset;
    std::move(failure_callback).Run();
    return;
  }

  offset += num_bytes_read;
  progress_callback.Run(offset);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ZipReader::ExtractChunk, weak_ptr_factory_.GetWeakPtr(),
                     std::move(output_file), std::move(success_callback),
                     std::move(failure_callback), progress_callback, offset));
}

// FileWriterDelegate ----------------------------------------------------------

FileWriterDelegate::FileWriterDelegate(base::File* file) : file_(file) {}

FileWriterDelegate::FileWriterDelegate(std::unique_ptr<base::File> file)
    : file_(file.get()), owned_file_(std::move(file)) {}

FileWriterDelegate::~FileWriterDelegate() {
  if (!file_->SetLength(file_length_)) {
    DVPLOG(1) << "Failed updating length of written file";
  }
}

bool FileWriterDelegate::PrepareOutput() {
  return file_->Seek(base::File::FROM_BEGIN, 0) >= 0;
}

bool FileWriterDelegate::WriteBytes(const char* data, int num_bytes) {
  int bytes_written = file_->WriteAtCurrentPos(data, num_bytes);
  if (bytes_written > 0)
    file_length_ += bytes_written;
  return bytes_written == num_bytes;
}

void FileWriterDelegate::SetTimeModified(const base::Time& time) {
  file_->SetTimes(base::Time::Now(), time);
}

void FileWriterDelegate::SetPosixFilePermissions(int mode) {
#if defined(OS_POSIX)
  zip::SetPosixFilePermissions(file_->GetPlatformFile(), mode);
#endif
}

// FilePathWriterDelegate ------------------------------------------------------

FilePathWriterDelegate::FilePathWriterDelegate(
    const base::FilePath& output_file_path)
    : output_file_path_(output_file_path) {}

FilePathWriterDelegate::~FilePathWriterDelegate() {}

bool FilePathWriterDelegate::PrepareOutput() {
  // We can't rely on parent directory entries being specified in the
  // zip, so we make sure they are created.
  if (!base::CreateDirectory(output_file_path_.DirName()))
    return false;

  file_.Initialize(output_file_path_,
                   base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  return file_.IsValid();
}

bool FilePathWriterDelegate::WriteBytes(const char* data, int num_bytes) {
  return num_bytes == file_.WriteAtCurrentPos(data, num_bytes);
}

void FilePathWriterDelegate::SetTimeModified(const base::Time& time) {
  file_.Close();
  base::TouchFile(output_file_path_, base::Time::Now(), time);
}

void FilePathWriterDelegate::SetPosixFilePermissions(int mode) {
#if defined(OS_POSIX)
  zip::SetPosixFilePermissions(file_.GetPlatformFile(), mode);
#endif
}

}  // namespace zip

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zlib/google/zip_writer.h"

#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/zlib/google/zip_internal.h"

namespace zip {
namespace internal {

bool ZipWriter::AddFileContent(const base::FilePath& path, base::File file) {
  char buf[zip::internal::kZipBufSize];
  while (true) {
    const int num_bytes =
        file.ReadAtCurrentPos(buf, zip::internal::kZipBufSize);

    if (num_bytes < 0) {
      DPLOG(ERROR) << "Cannot read file '" << path << "'";
      return false;
    }

    if (num_bytes == 0)
      return true;

    if (zipWriteInFileInZip(zip_file_, buf, num_bytes) != ZIP_OK) {
      DLOG(ERROR) << "Cannot write data from file '" << path << "' to ZIP";
      return false;
    }
  }
}

bool ZipWriter::OpenNewFileEntry(const base::FilePath& path,
                                 bool is_directory,
                                 base::Time last_modified) {
  std::string str_path = path.AsUTF8Unsafe();
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&str_path, 0u, "\\", "/");
#endif
  if (is_directory)
    str_path += "/";

  return zip::internal::ZipOpenNewFileInZip(zip_file_, str_path, last_modified);
}

bool ZipWriter::CloseNewFileEntry() {
  return zipCloseFileInZip(zip_file_) == ZIP_OK;
}

bool ZipWriter::AddFileEntry(const base::FilePath& path, base::File file) {
  base::File::Info file_info;
  if (!file.GetInfo(&file_info))
    return false;

  if (!OpenNewFileEntry(path, /*is_directory=*/false, file_info.last_modified))
    return false;

  bool success = AddFileContent(path, std::move(file));
  if (!CloseNewFileEntry())
    return false;

  return success;
}

bool ZipWriter::AddDirectoryEntry(const base::FilePath& path,
                                  base::Time last_modified) {
  return OpenNewFileEntry(path, /*is_directory=*/true, last_modified) &&
         CloseNewFileEntry();
}

#if defined(OS_POSIX)
// static
std::unique_ptr<ZipWriter> ZipWriter::CreateWithFd(
    int zip_file_fd,
    const base::FilePath& root_dir,
    FileAccessor* file_accessor) {
  DCHECK(zip_file_fd != base::kInvalidPlatformFile);
  zipFile zip_file =
      internal::OpenFdForZipping(zip_file_fd, APPEND_STATUS_CREATE);
  if (!zip_file) {
    DLOG(ERROR) << "Couldn't create ZIP file for FD " << zip_file_fd;
    return nullptr;
  }
  return std::unique_ptr<ZipWriter>(
      new ZipWriter(zip_file, root_dir, file_accessor));
}
#endif

// static
std::unique_ptr<ZipWriter> ZipWriter::Create(
    const base::FilePath& zip_file_path,
    const base::FilePath& root_dir,
    FileAccessor* file_accessor) {
  DCHECK(!zip_file_path.empty());
  zipFile zip_file = internal::OpenForZipping(zip_file_path.AsUTF8Unsafe(),
                                              APPEND_STATUS_CREATE);
  if (!zip_file) {
    DLOG(ERROR) << "Couldn't create ZIP file at path " << zip_file_path;
    return nullptr;
  }
  return std::unique_ptr<ZipWriter>(
      new ZipWriter(zip_file, root_dir, file_accessor));
}

ZipWriter::ZipWriter(zipFile zip_file,
                     const base::FilePath& root_dir,
                     FileAccessor* file_accessor)
    : zip_file_(zip_file), root_dir_(root_dir), file_accessor_(file_accessor) {}

ZipWriter::~ZipWriter() {
  DCHECK(pending_entries_.empty());
}

bool ZipWriter::WriteEntries(const std::vector<base::FilePath>& paths) {
  return AddEntries(paths) && Close();
}

bool ZipWriter::AddEntries(const std::vector<base::FilePath>& paths) {
  DCHECK(zip_file_);
  pending_entries_.insert(pending_entries_.end(), paths.begin(), paths.end());
  return FlushEntriesIfNeeded(/*force=*/false);
}

bool ZipWriter::Close() {
  bool success = FlushEntriesIfNeeded(/*force=*/true) &&
                 zipClose(zip_file_, nullptr) == ZIP_OK;
  zip_file_ = nullptr;
  return success;
}

bool ZipWriter::FlushEntriesIfNeeded(bool force) {
  // Numbers of pending entries that triggers writing them to the ZIP file.
  const size_t kMaxPendingEntriesCount = 50;

  if (pending_entries_.size() < kMaxPendingEntriesCount && !force)
    return true;

  while (pending_entries_.size() >= kMaxPendingEntriesCount ||
         (force && !pending_entries_.empty())) {
    size_t entry_count =
        std::min(pending_entries_.size(), kMaxPendingEntriesCount);
    std::vector<base::FilePath> relative_paths;
    std::vector<base::FilePath> absolute_paths;
    relative_paths.insert(relative_paths.begin(), pending_entries_.begin(),
                          pending_entries_.begin() + entry_count);
    for (auto iter = pending_entries_.begin();
         iter != pending_entries_.begin() + entry_count; ++iter) {
      // The FileAccessor requires absolute paths.
      absolute_paths.push_back(root_dir_.Append(*iter));
    }
    pending_entries_.erase(pending_entries_.begin(),
                           pending_entries_.begin() + entry_count);

    // We don't know which paths are files and which ones are directories, and
    // we want to avoid making a call to file_accessor_ for each entry. Open the
    // files instead, invalid files are returned for directories.
    std::vector<base::File> files =
        file_accessor_->OpenFilesForReading(absolute_paths);
    DCHECK_EQ(files.size(), relative_paths.size());
    for (size_t i = 0; i < files.size(); i++) {
      const base::FilePath& relative_path = relative_paths[i];
      const base::FilePath& absolute_path = absolute_paths[i];
      base::File file = std::move(files[i]);
      if (file.IsValid()) {
        if (!AddFileEntry(relative_path, std::move(file))) {
          LOG(ERROR) << "Failed to write file " << relative_path.value()
                     << " to ZIP file.";
          return false;
        }
      } else {
        // Missing file or directory case.
        base::Time last_modified =
            file_accessor_->GetLastModifiedTime(absolute_path);
        if (last_modified.is_null()) {
          LOG(ERROR) << "Failed to write entry " << relative_path.value()
                     << " to ZIP file.";
          return false;
        }
        DCHECK(file_accessor_->DirectoryExists(absolute_path));
        if (!AddDirectoryEntry(relative_path, last_modified)) {
          LOG(ERROR) << "Failed to write directory " << relative_path.value()
                     << " to ZIP file.";
          return false;
        }
      }
    }
  }
  return true;
}

}  // namespace internal
}  // namespace zip

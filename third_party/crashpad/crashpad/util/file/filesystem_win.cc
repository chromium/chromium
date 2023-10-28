// Copyright 2017 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/file/filesystem.h"

#include <sys/time.h>
#include <windows.h>

#include "base/logging.h"
#include "util/file/directory_reader.h"
#include "util/misc/time.h"

namespace crashpad {

namespace {

bool IsSymbolicLink(const base::FilePath& path) {
  WIN32_FIND_DATA find_data;
  ScopedSearchHANDLE handle(FindFirstFileEx(path.value().c_str(),
                                            FindExInfoBasic,
                                            &find_data,
                                            FindExSearchNameMatch,
                                            nullptr,
                                            0));
  if (!handle.is_valid()) {
    PLOG(ERROR) << "FindFirstFileEx " << path;
    return false;
  }

  return (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
         find_data.dwReserved0 == IO_REPARSE_TAG_SYMLINK;
}

bool LoggingRemoveDirectoryImpl(const base::FilePath& path) {
  if (!RemoveDirectory(path.value().c_str())) {
    PLOG(ERROR) << "RemoveDirectory " << path;
    return false;
  }
  return true;
}

}  // namespace

bool FileModificationTime(const base::FilePath& path, timespec* mtime) {
  DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT;
  if (IsDirectory(path, true)) {
    // required for directory handles
    flags |= FILE_FLAG_BACKUP_SEMANTICS;
  }

  ScopedFileHandle handle(
      ::CreateFile(path.value().c_str(),
                   GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   nullptr,
                   OPEN_EXISTING,
                   flags,
                   nullptr));
  if (!handle.is_valid()) {
    PLOG(ERROR) << "CreateFile " << path;
    return false;
  }

  FILETIME file_mtime;
  if (!GetFileTime(handle.get(), nullptr, nullptr, &file_mtime)) {
    PLOG(ERROR) << "GetFileTime " << path;
    return false;
  }
  *mtime = FiletimeToTimespecEpoch(file_mtime);
  return true;
}

bool LoggingCreateDirectory(const base::FilePath& path,
                            FilePermissions permissions,
                            bool may_reuse) {
  if (CreateDirectory(path.value().c_str(), nullptr)) {
    return true;
  }
  if (may_reuse && GetLastError() == ERROR_ALREADY_EXISTS) {
    if (!IsDirectory(path, true)) {
      LOG(ERROR) << path << " not a directory";
      return false;
    }
    return true;
  }
  PLOG(ERROR) << "CreateDirectory " << path;
  return false;
}

bool MoveFileOrDirectory(const base::FilePath& source,
                         const base::FilePath& dest) {
  if (!MoveFileEx(source.value().c_str(),
                  dest.value().c_str(),
                  IsDirectory(source, false) ? 0 : MOVEFILE_REPLACE_EXISTING)) {
    PLOG(ERROR) << "MoveFileEx" << source << ", " << dest;
    return false;
  }
  return true;
}

bool IsRegularFile(const base::FilePath& path) {
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr == INVALID_FILE_ATTRIBUTES) {
    PLOG(ERROR) << "GetFileAttributes " << path;
    return false;
  }
  if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
      (fileattr & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    return false;
  }
  return true;
}

bool IsDirectory(const base::FilePath& path, bool allow_symlinks) {
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr == INVALID_FILE_ATTRIBUTES) {
    PLOG(ERROR) << "GetFileAttributes " << path;
    return false;
  }
  if (!allow_symlinks && (fileattr & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    return false;
  }
  return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool LoggingRemoveFile(const base::FilePath& path) {
  // RemoveDirectory is used if the file is a symbolic link to a directory.
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES &&
      (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
      (fileattr & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    return LoggingRemoveDirectoryImpl(path);
  }

  if (!DeleteFile(path.value().c_str())) {
    PLOG(ERROR) << "DeleteFile " << path;
    return false;
  }
  return true;
}

bool LoggingRemoveDirectory(const base::FilePath& path) {
  if (IsSymbolicLink(path)) {
    LOG(ERROR) << "Not a directory " << path;
    return false;
  }
  return LoggingRemoveDirectoryImpl(path);
}

uint64_t GetFileSize(const base::FilePath& filepath) {
  struct _stati64 statbuf;
  if (!IsRegularFile(filepath)) {
    return 0;
  }
  int ret_value = _wstat64(filepath.value().c_str(), &statbuf);
  if (ret_value == 0) {
    return statbuf.st_size;
  }
  PLOG(ERROR) << "stat " << filepath;
  return 0;
}

uint64_t GetDirectorySize(const base::FilePath& dirpath) {
  if (!IsDirectory(dirpath, /*allow_symlinks=*/false)) {
    return 0;
  }
  DirectoryReader reader;
  if (!reader.Open(dirpath)) {
    return 0;
  }
  base::FilePath filename;
  DirectoryReader::Result result;
  uint64_t size = 0;
  while ((result = reader.NextFile(&filename)) ==
         DirectoryReader::Result::kSuccess) {
    const base::FilePath filepath(dirpath.Append(filename));
    if (IsDirectory(filepath, /*allow_symlinks=*/false)) {
      size += GetDirectorySize(filepath);
    } else {
      size += GetFileSize(filepath);
    }
  }
  return size;
}

}  // namespace crashpad

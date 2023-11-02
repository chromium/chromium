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

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "util/file/directory_reader.h"

namespace crashpad {

bool FileModificationTime(const base::FilePath& path, timespec* mtime) {
  struct stat st;
  if (lstat(path.value().c_str(), &st) != 0) {
    PLOG(ERROR) << "lstat " << path.value();
    return false;
  }

#if BUILDFLAG(IS_APPLE)
  *mtime = st.st_mtimespec;
#elif BUILDFLAG(IS_ANDROID)
  // This is needed to compile with traditional NDK headers.
  mtime->tv_sec = st.st_mtime;
  mtime->tv_nsec = st.st_mtime_nsec;
#else
  *mtime = st.st_mtim;
#endif
  return true;
}

bool LoggingCreateDirectory(const base::FilePath& path,
                            FilePermissions permissions,
                            bool may_reuse) {
  if (mkdir(path.value().c_str(),
            permissions == FilePermissions::kWorldReadable ? 0755 : 0700) ==
      0) {
    return true;
  }
  if (may_reuse && errno == EEXIST) {
    if (!IsDirectory(path, true)) {
      LOG(ERROR) << path.value() << " not a directory";
      return false;
    }
    return true;
  }
  PLOG(ERROR) << "mkdir " << path.value();
  return false;
}

bool MoveFileOrDirectory(const base::FilePath& source,
                         const base::FilePath& dest) {
  if (rename(source.value().c_str(), dest.value().c_str()) != 0) {
    PLOG(ERROR) << "rename " << source.value().c_str() << ", "
                << dest.value().c_str();
    return false;
  }
  return true;
}

bool IsRegularFile(const base::FilePath& path) {
  struct stat st;
  if (lstat(path.value().c_str(), &st) != 0) {
    PLOG_IF(ERROR, errno != ENOENT) << "stat " << path.value();
    return false;
  }
  return S_ISREG(st.st_mode);
}

bool IsDirectory(const base::FilePath& path, bool allow_symlinks) {
  struct stat st;
  if (allow_symlinks) {
    if (stat(path.value().c_str(), &st) != 0) {
      PLOG_IF(ERROR, errno != ENOENT) << "stat " << path.value();
      return false;
    }
  } else if (lstat(path.value().c_str(), &st) != 0) {
    PLOG_IF(ERROR, errno != ENOENT) << "lstat " << path.value();
    return false;
  }
  return S_ISDIR(st.st_mode);
}

bool LoggingRemoveFile(const base::FilePath& path) {
  if (unlink(path.value().c_str()) != 0) {
    PLOG(ERROR) << "unlink " << path.value();
    return false;
  }
  return true;
}

bool LoggingRemoveDirectory(const base::FilePath& path) {
  if (rmdir(path.value().c_str()) != 0) {
    PLOG(ERROR) << "rmdir " << path.value();
    return false;
  }
  return true;
}

uint64_t GetFileSize(const base::FilePath& filepath) {
  if (!IsRegularFile(filepath)) {
    return 0;
  }
  struct stat statbuf;
  if (stat(filepath.value().c_str(), &statbuf) == 0) {
    return statbuf.st_size;
  }
  PLOG(ERROR) << "stat " << filepath.value().c_str();
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

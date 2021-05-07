// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/file/file_io.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <limits>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "util/misc/random_string.h"

namespace crashpad {

namespace {

struct ReadTraits {
  using BufferType = void*;
  static FileOperationResult Operate(int fd, BufferType buffer, size_t size) {
    return read(fd, buffer, size);
  }
};

struct WriteTraits {
  using BufferType = const void*;
  static FileOperationResult Operate(int fd, BufferType buffer, size_t size) {
    return write(fd, buffer, size);
  }
};

template <typename Traits>
FileOperationResult ReadOrWrite(int fd,
                                typename Traits::BufferType buffer,
                                size_t size) {
  constexpr size_t kMaxReadWriteSize =
      static_cast<size_t>(std::numeric_limits<ssize_t>::max());
  const size_t requested_bytes = std::min(size, kMaxReadWriteSize);

  FileOperationResult transacted_bytes =
      HANDLE_EINTR(Traits::Operate(fd, buffer, requested_bytes));
  if (transacted_bytes < 0) {
    return -1;
  }

  DCHECK_LE(static_cast<size_t>(transacted_bytes), requested_bytes);
  return transacted_bytes;
}

FileHandle OpenFileForOutput(int rdwr_or_wronly,
                             const base::FilePath& path,
                             FileWriteMode mode,
                             FilePermissions permissions) {
#if defined(OS_FUCHSIA)
  // O_NOCTTY is invalid on Fuchsia, and O_CLOEXEC isn't necessary.
  int flags = 0;
#else
  int flags = O_NOCTTY | O_CLOEXEC;
#endif

  DCHECK(rdwr_or_wronly & (O_RDWR | O_WRONLY));
  DCHECK_EQ(rdwr_or_wronly & ~(O_RDWR | O_WRONLY), 0);
  flags |= rdwr_or_wronly;

  switch (mode) {
    case FileWriteMode::kReuseOrFail:
      break;
    case FileWriteMode::kReuseOrCreate:
      flags |= O_CREAT;
      break;
    case FileWriteMode::kTruncateOrCreate:
      flags |= O_CREAT | O_TRUNC;
      break;
    case FileWriteMode::kCreateOrFail:
      flags |= O_CREAT | O_EXCL;
      break;
  }

  return HANDLE_EINTR(
      open(path.value().c_str(),
           flags,
           permissions == FilePermissions::kWorldReadable ? 0644 : 0600));
}
}  // namespace

namespace internal {

FileOperationResult NativeWriteFile(FileHandle file,
                                    const void* buffer,
                                    size_t size) {
  return ReadOrWrite<WriteTraits>(file, buffer, size);
}

}  // namespace internal

FileOperationResult ReadFile(FileHandle file, void* buffer, size_t size) {
  return ReadOrWrite<ReadTraits>(file, buffer, size);
}

FileHandle OpenFileForRead(const base::FilePath& path) {
  int flags = O_RDONLY;
#if !defined(OS_FUCHSIA)
  // O_NOCTTY is invalid on Fuchsia, and O_CLOEXEC isn't necessary.
  flags |= O_NOCTTY | O_CLOEXEC;
#endif
  return HANDLE_EINTR(open(path.value().c_str(), flags));
}

FileHandle OpenFileForWrite(const base::FilePath& path,
                            FileWriteMode mode,
                            FilePermissions permissions) {
  return OpenFileForOutput(O_WRONLY, path, mode, permissions);
}

FileHandle OpenFileForReadAndWrite(const base::FilePath& path,
                                   FileWriteMode mode,
                                   FilePermissions permissions) {
  return OpenFileForOutput(O_RDWR, path, mode, permissions);
}

FileHandle LoggingOpenFileForRead(const base::FilePath& path) {
  FileHandle fd = OpenFileForRead(path);
  PLOG_IF(ERROR, fd < 0) << "open " << path.value();
  return fd;
}

FileHandle LoggingOpenFileForWrite(const base::FilePath& path,
                                   FileWriteMode mode,
                                   FilePermissions permissions) {
  FileHandle fd = OpenFileForWrite(path, mode, permissions);
  PLOG_IF(ERROR, fd < 0) << "open " << path.value();
  return fd;
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
FileHandle LoggingOpenMemoryFileForReadAndWrite(const base::FilePath& name) {
  DCHECK(name.value().find('/') == std::string::npos);

  int result = HANDLE_EINTR(memfd_create(name.value().c_str(), 0));
  if (result >= 0 || errno != ENOSYS) {
    PLOG_IF(ERROR, result < 0) << "memfd_create";
    return result;
  }

  const char* tmp = getenv("TMPDIR");
  tmp = tmp ? tmp : "/tmp";

  result = HANDLE_EINTR(open(tmp, O_RDWR | O_EXCL | O_TMPFILE, 0600));
  if (result >= 0 ||
      // These are the expected possible error codes indicating that O_TMPFILE
      // doesn't have kernel or filesystem support. O_TMPFILE was added in Linux
      // 3.11. Experimentation confirms that at least Linux 2.6.29 and Linux
      // 3.10 set errno to EISDIR. EOPNOTSUPP is returned when the filesystem
      // doesn't support O_TMPFILE. The man pages also mention ENOENT as an
      // error code to check, but the language implies it would only occur when
      // |tmp| is also an invalid directory. EINVAL is mentioned as a possible
      // error code for any invalid values in flags, but O_TMPFILE isn't
      // mentioned explicitly in this context and hasn't been observed in
      // practice.
      (errno != EISDIR && errno != EOPNOTSUPP)) {
    PLOG_IF(ERROR, result < 0) << "open";
    return result;
  }

  std::string path = base::StringPrintf("%s/%s.%d.%s",
                                        tmp,
                                        name.value().c_str(),
                                        getpid(),
                                        RandomString().c_str());
  result = HANDLE_EINTR(open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600));
  if (result < 0) {
    PLOG(ERROR) << "open";
    return result;
  }
  if (unlink(path.c_str()) != 0) {
    PLOG(WARNING) << "unlink";
  }
  return result;
}
#endif

FileHandle LoggingOpenFileForReadAndWrite(const base::FilePath& path,
                                          FileWriteMode mode,
                                          FilePermissions permissions) {
  FileHandle fd = OpenFileForReadAndWrite(path, mode, permissions);
  PLOG_IF(ERROR, fd < 0) << "open " << path.value();
  return fd;
}

#if !defined(OS_FUCHSIA)

FileLockingResult LoggingLockFile(FileHandle file,
                                  FileLocking locking,
                                  FileLockingBlocking blocking) {
  int operation = (locking == FileLocking::kShared) ? LOCK_SH : LOCK_EX;
  if (blocking == FileLockingBlocking::kNonBlocking)
    operation |= LOCK_NB;

  int rv = HANDLE_EINTR(flock(file, operation));
  if (rv != 0) {
    if (errno == EWOULDBLOCK) {
      return FileLockingResult::kWouldBlock;
    }
    PLOG(ERROR) << "flock";
    return FileLockingResult::kFailure;
  }
  return FileLockingResult::kSuccess;
}

bool LoggingUnlockFile(FileHandle file) {
  int rv = flock(file, LOCK_UN);
  PLOG_IF(ERROR, rv != 0) << "flock";
  return rv == 0;
}

#endif  // !OS_FUCHSIA

FileOffset LoggingSeekFile(FileHandle file, FileOffset offset, int whence) {
  off_t rv = lseek(file, offset, whence);
  PLOG_IF(ERROR, rv < 0) << "lseek";
  return rv;
}

bool LoggingTruncateFile(FileHandle file) {
  if (HANDLE_EINTR(ftruncate(file, 0)) != 0) {
    PLOG(ERROR) << "ftruncate";
    return false;
  }
  return true;
}

bool LoggingCloseFile(FileHandle file) {
  int rv = IGNORE_EINTR(close(file));
  PLOG_IF(ERROR, rv != 0) << "close";
  return rv == 0;
}

FileOffset LoggingFileSizeByHandle(FileHandle file) {
  struct stat st;
  if (fstat(file, &st) != 0) {
    PLOG(ERROR) << "fstat";
    return -1;
  }
  return st.st_size;
}

FileHandle StdioFileHandle(StdioStream stdio_stream) {
  switch (stdio_stream) {
    case StdioStream::kStandardInput:
      return STDIN_FILENO;
    case StdioStream::kStandardOutput:
      return STDOUT_FILENO;
    case StdioStream::kStandardError:
      return STDERR_FILENO;
  }

  NOTREACHED();
  return kInvalidFileHandle;
}

}  // namespace crashpad

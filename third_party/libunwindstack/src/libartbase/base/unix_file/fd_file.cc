/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fd_file.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__BIONIC__) && defined(__ANDROID_API__) && __ANDROID_API__ >= 29
  #define FDSAN_AVAILABLE 1
#endif

#if defined(FDSAN_AVAILABLE)
#include <android/fdsan.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include <limits>

#include <android-base/file.h>
#include <android-base/logging.h>

// Includes needed for FdFile::Copy().
#ifdef __linux__
#include <sys/sendfile.h>
#else
#include <algorithm>
#include "base/globals.h"
#include "base/stl_util.h"
#endif

namespace unix_file {

#if defined(_WIN32)
// RAII wrapper for an event object to allow asynchronous I/O to correctly signal completion.
class ScopedEvent {
 public:
  ScopedEvent() {
    handle_ = CreateEventA(/*lpEventAttributes*/ nullptr,
                           /*bManualReset*/ true,
                           /*bInitialState*/ false,
                           /*lpName*/ nullptr);
  }

  ~ScopedEvent() { CloseHandle(handle_); }

  HANDLE handle() { return handle_; }

 private:
  HANDLE handle_;
  DISALLOW_COPY_AND_ASSIGN(ScopedEvent);
};

// Windows implementation of pread/pwrite. Note that these DO move the file descriptor's read/write
// position, but do so atomically.
static ssize_t pread(int fd, void* data, size_t byte_count, off64_t offset) {
  ScopedEvent event;
  if (event.handle() == INVALID_HANDLE_VALUE) {
    PLOG(ERROR) << "Could not create event handle.";
    errno = EIO;
    return static_cast<ssize_t>(-1);
  }

  auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  DWORD bytes_read = 0;
  OVERLAPPED overlapped = {};
  overlapped.Offset = static_cast<DWORD>(offset);
  overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
  overlapped.hEvent = event.handle();
  if (!ReadFile(handle, data, static_cast<DWORD>(byte_count), &bytes_read, &overlapped)) {
    // If the read failed with other than ERROR_IO_PENDING, return an error.
    // ERROR_IO_PENDING signals the write was begun asynchronously.
    // Block until the asynchronous operation has finished or fails, and return
    // result accordingly.
    if (::GetLastError() != ERROR_IO_PENDING ||
        !::GetOverlappedResult(handle, &overlapped, &bytes_read, TRUE)) {
      // In case someone tries to read errno (since this is masquerading as a POSIX call).
      errno = EIO;
      return static_cast<ssize_t>(-1);
    }
  }
  return static_cast<ssize_t>(bytes_read);
}

static ssize_t pwrite(int fd, const void* buf, size_t count, off64_t offset) {
  ScopedEvent event;
  if (event.handle() == INVALID_HANDLE_VALUE) {
    PLOG(ERROR) << "Could not create event handle.";
    errno = EIO;
    return static_cast<ssize_t>(-1);
  }

  auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  DWORD bytes_written = 0;
  OVERLAPPED overlapped = {};
  overlapped.Offset = static_cast<DWORD>(offset);
  overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
  overlapped.hEvent = event.handle();
  if (!::WriteFile(handle, buf, count, &bytes_written, &overlapped)) {
    // If the write failed with other than ERROR_IO_PENDING, return an error.
    // ERROR_IO_PENDING signals the write was begun asynchronously.
    // Block until the asynchronous operation has finished or fails, and return
    // result accordingly.
    if (::GetLastError() != ERROR_IO_PENDING ||
        !::GetOverlappedResult(handle, &overlapped, &bytes_written, TRUE)) {
      // In case someone tries to read errno (since this is masquerading as a POSIX call).
      errno = EIO;
      return static_cast<ssize_t>(-1);
    }
  }
  return static_cast<ssize_t>(bytes_written);
}

static int fsync(int fd) {
  auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (handle != INVALID_HANDLE_VALUE && ::FlushFileBuffers(handle)) {
    return 0;
  }
  errno = EINVAL;
  return -1;
}
#endif

#if defined(FDSAN_AVAILABLE)
static uint64_t GetFdFileOwnerTag(FdFile* fd_file) {
  return android_fdsan_create_owner_tag(ANDROID_FDSAN_OWNER_TYPE_ART_FDFILE,
                                        reinterpret_cast<uint64_t>(fd_file));
}
#endif

FdFile::FdFile(int fd, bool check_usage)
    : FdFile(fd, std::string(), check_usage) {}

FdFile::FdFile(int fd, const std::string& path, bool check_usage)
    : FdFile(fd, path, check_usage, false) {}

FdFile::FdFile(int fd, const std::string& path, bool check_usage,
               bool read_only_mode)
    : guard_state_(check_usage ? GuardState::kBase : GuardState::kNoCheck),
      fd_(fd),
      file_path_(path),
      read_only_mode_(read_only_mode) {
#if defined(FDSAN_AVAILABLE)
  if (fd >= 0) {
    android_fdsan_exchange_owner_tag(fd, 0, GetFdFileOwnerTag(this));
  }
#endif
}

FdFile::FdFile(const std::string& path, int flags, mode_t mode,
               bool check_usage) {
  Open(path, flags, mode);
  if (!check_usage || !IsOpened()) {
    guard_state_ = GuardState::kNoCheck;
  }
}

void FdFile::Destroy() {
  if (kCheckSafeUsage && (guard_state_ < GuardState::kNoCheck)) {
    if (guard_state_ < GuardState::kFlushed) {
      LOG(ERROR) << "File " << file_path_ << " wasn't explicitly flushed before destruction.";
    }
    if (guard_state_ < GuardState::kClosed) {
      LOG(ERROR) << "File " << file_path_ << " wasn't explicitly closed before destruction.";
    }
    DCHECK_GE(guard_state_, GuardState::kClosed);
  }
  if (fd_ != kInvalidFd) {
    if (Close() != 0) {
      PLOG(WARNING) << "Failed to close file with fd=" << fd_ << " path=" << file_path_;
    }
  }
}

FdFile::FdFile(FdFile&& other) noexcept
    : guard_state_(other.guard_state_),
      fd_(other.fd_),
      file_path_(std::move(other.file_path_)),
      read_only_mode_(other.read_only_mode_) {
#if defined(FDSAN_AVAILABLE)
  if (fd_ >= 0) {
    android_fdsan_exchange_owner_tag(fd_, GetFdFileOwnerTag(&other), GetFdFileOwnerTag(this));
  }
#endif
  other.guard_state_ = GuardState::kClosed;
  other.fd_ = kInvalidFd;
}

FdFile& FdFile::operator=(FdFile&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (this->fd_ != other.fd_) {
    Destroy();  // Free old state.
  }

  guard_state_ = other.guard_state_;
  fd_ = other.fd_;
  file_path_ = std::move(other.file_path_);
  read_only_mode_ = other.read_only_mode_;

#if defined(FDSAN_AVAILABLE)
  if (fd_ >= 0) {
    android_fdsan_exchange_owner_tag(fd_, GetFdFileOwnerTag(&other), GetFdFileOwnerTag(this));
  }
#endif
  other.guard_state_ = GuardState::kClosed;
  other.fd_ = kInvalidFd;
  return *this;
}

FdFile::~FdFile() {
  Destroy();
}

int FdFile::Release() {
  int tmp_fd = fd_;
  fd_ = kInvalidFd;
  guard_state_ = GuardState::kNoCheck;
#if defined(FDSAN_AVAILABLE)
  if (tmp_fd >= 0) {
    android_fdsan_exchange_owner_tag(tmp_fd, GetFdFileOwnerTag(this), 0);
  }
#endif
  return tmp_fd;
}

void FdFile::Reset(int fd, bool check_usage) {
  CHECK_NE(fd, fd_);

  if (fd_ != kInvalidFd) {
    Destroy();
  }
  fd_ = fd;

#if defined(FDSAN_AVAILABLE)
  if (fd_ >= 0) {
    android_fdsan_exchange_owner_tag(fd_, 0, GetFdFileOwnerTag(this));
  }
#endif

  if (check_usage) {
    guard_state_ = fd == kInvalidFd ? GuardState::kNoCheck : GuardState::kBase;
  } else {
    guard_state_ = GuardState::kNoCheck;
  }
}

void FdFile::moveTo(GuardState target, GuardState warn_threshold, const char* warning) {
  if (kCheckSafeUsage) {
    if (guard_state_ < GuardState::kNoCheck) {
      if (warn_threshold < GuardState::kNoCheck && guard_state_ >= warn_threshold) {
        LOG(ERROR) << warning;
      }
      guard_state_ = target;
    }
  }
}

void FdFile::moveUp(GuardState target, const char* warning) {
  if (kCheckSafeUsage) {
    if (guard_state_ < GuardState::kNoCheck) {
      if (guard_state_ < target) {
        guard_state_ = target;
      } else if (target < guard_state_) {
        LOG(ERROR) << warning;
      }
    }
  }
}

bool FdFile::Open(const std::string& path, int flags) {
  return Open(path, flags, 0640);
}

bool FdFile::Open(const std::string& path, int flags, mode_t mode) {
  static_assert(O_RDONLY == 0, "Readonly flag has unexpected value.");
  DCHECK_EQ(fd_, kInvalidFd) << path;
  read_only_mode_ = ((flags & O_ACCMODE) == O_RDONLY);
  fd_ = TEMP_FAILURE_RETRY(open(path.c_str(), flags, mode));
  if (fd_ == kInvalidFd) {
    return false;
  }

#if defined(FDSAN_AVAILABLE)
  android_fdsan_exchange_owner_tag(fd_, 0, GetFdFileOwnerTag(this));
#endif

  file_path_ = path;
  if (kCheckSafeUsage && (flags & (O_RDWR | O_CREAT | O_WRONLY)) != 0) {
    // Start in the base state (not flushed, not closed).
    guard_state_ = GuardState::kBase;
  } else {
    // We are not concerned with read-only files. In that case, proper flushing and closing is
    // not important.
    guard_state_ = GuardState::kNoCheck;
  }
  return true;
}

int FdFile::Close() {
#if defined(FDSAN_AVAILABLE)
  int result = android_fdsan_close_with_tag(fd_, GetFdFileOwnerTag(this));
#else
  int result = close(fd_);
#endif

  // Test here, so the file is closed and not leaked.
  if (kCheckSafeUsage) {
    DCHECK_GE(guard_state_, GuardState::kFlushed) << "File " << file_path_
        << " has not been flushed before closing.";
    moveUp(GuardState::kClosed, nullptr);
  }

#if defined(__linux__)
  // close always succeeds on linux, even if failure is reported.
  UNUSED(result);
#else
  if (result == -1) {
    return -errno;
  }
#endif

  fd_ = kInvalidFd;
  file_path_ = "";
  return 0;
}

int FdFile::Flush() {
  DCHECK(!read_only_mode_);

#ifdef __linux__
  int rc = TEMP_FAILURE_RETRY(fdatasync(fd_));
#else
  int rc = TEMP_FAILURE_RETRY(fsync(fd_));
#endif

  moveUp(GuardState::kFlushed, "Flushing closed file.");
  if (rc == 0) {
    return 0;
  }

  // Don't report failure if we just tried to flush a pipe or socket.
  return errno == EINVAL ? 0 : -errno;
}

int64_t FdFile::Read(char* buf, int64_t byte_count, int64_t offset) const {
#ifdef __linux__
  int rc = TEMP_FAILURE_RETRY(pread64(fd_, buf, byte_count, offset));
#else
  int rc = TEMP_FAILURE_RETRY(pread(fd_, buf, byte_count, offset));
#endif
  return (rc == -1) ? -errno : rc;
}

int FdFile::SetLength(int64_t new_length) {
  DCHECK(!read_only_mode_);
#ifdef __linux__
  int rc = TEMP_FAILURE_RETRY(ftruncate64(fd_, new_length));
#else
  int rc = TEMP_FAILURE_RETRY(ftruncate(fd_, new_length));
#endif
  moveTo(GuardState::kBase, GuardState::kClosed, "Truncating closed file.");
  return (rc == -1) ? -errno : rc;
}

int64_t FdFile::GetLength() const {
  struct stat s;
  int rc = TEMP_FAILURE_RETRY(fstat(fd_, &s));
  return (rc == -1) ? -errno : s.st_size;
}

int64_t FdFile::Write(const char* buf, int64_t byte_count, int64_t offset) {
  DCHECK(!read_only_mode_);
#ifdef __linux__
  int rc = TEMP_FAILURE_RETRY(pwrite64(fd_, buf, byte_count, offset));
#else
  int rc = TEMP_FAILURE_RETRY(pwrite(fd_, buf, byte_count, offset));
#endif
  moveTo(GuardState::kBase, GuardState::kClosed, "Writing into closed file.");
  return (rc == -1) ? -errno : rc;
}

int FdFile::Fd() const {
  return fd_;
}

bool FdFile::ReadOnlyMode() const {
  return read_only_mode_;
}

bool FdFile::CheckUsage() const {
  return guard_state_ != GuardState::kNoCheck;
}

bool FdFile::IsOpened() const {
  return FdFile::IsOpenFd(fd_);
}

static ssize_t ReadIgnoreOffset(int fd, void *buf, size_t count, off_t offset) {
  DCHECK_EQ(offset, 0);
  return read(fd, buf, count);
}

template <ssize_t (*read_func)(int, void*, size_t, off_t)>
static bool ReadFullyGeneric(int fd, void* buffer, size_t byte_count, size_t offset) {
  char* ptr = static_cast<char*>(buffer);
  while (byte_count > 0) {
    ssize_t bytes_read = TEMP_FAILURE_RETRY(read_func(fd, ptr, byte_count, offset));
    if (bytes_read <= 0) {
      // 0: end of file
      // -1: error
      return false;
    }
    byte_count -= bytes_read;  // Reduce the number of remaining bytes.
    ptr += bytes_read;  // Move the buffer forward.
    offset += static_cast<size_t>(bytes_read);  // Move the offset forward.
  }
  return true;
}

bool FdFile::ReadFully(void* buffer, size_t byte_count) {
  return ReadFullyGeneric<ReadIgnoreOffset>(fd_, buffer, byte_count, 0);
}

bool FdFile::PreadFully(void* buffer, size_t byte_count, size_t offset) {
  return ReadFullyGeneric<pread>(fd_, buffer, byte_count, offset);
}

template <bool kUseOffset>
bool FdFile::WriteFullyGeneric(const void* buffer, size_t byte_count, size_t offset) {
  DCHECK(!read_only_mode_);
  moveTo(GuardState::kBase, GuardState::kClosed, "Writing into closed file.");
  DCHECK(kUseOffset || offset == 0u);
  const char* ptr = static_cast<const char*>(buffer);
  while (byte_count > 0) {
    ssize_t bytes_written = kUseOffset
        ? TEMP_FAILURE_RETRY(pwrite(fd_, ptr, byte_count, offset))
        : TEMP_FAILURE_RETRY(write(fd_, ptr, byte_count));
    if (bytes_written == -1) {
      return false;
    }
    byte_count -= bytes_written;  // Reduce the number of remaining bytes.
    ptr += bytes_written;  // Move the buffer forward.
    offset += static_cast<size_t>(bytes_written);
  }
  return true;
}

bool FdFile::PwriteFully(const void* buffer, size_t byte_count, size_t offset) {
  return WriteFullyGeneric<true>(buffer, byte_count, offset);
}

bool FdFile::WriteFully(const void* buffer, size_t byte_count) {
  return WriteFullyGeneric<false>(buffer, byte_count, 0u);
}

bool FdFile::Copy(FdFile* input_file, int64_t offset, int64_t size) {
  DCHECK(!read_only_mode_);
  off_t off = static_cast<off_t>(offset);
  off_t sz = static_cast<off_t>(size);
  if (offset < 0 || static_cast<int64_t>(off) != offset ||
      size < 0 || static_cast<int64_t>(sz) != size ||
      sz > std::numeric_limits<off_t>::max() - off) {
    errno = EINVAL;
    return false;
  }
  if (size == 0) {
    return true;
  }

  off_t out_length = lseek(Fd(), 0, SEEK_END);
  if (out_length != 0) {
    // Output file is not empty.
    //
    // Currently there is no use-case for copying into non-empty files. To support the use-case,
    // when needed, the implementation should handle the case in which the input file has an empty
    // block whereas the corresponding location in the output file already contains some data. The
    // current implementation would preserve that data instead of overwriting it with zeroes.
    errno = EINVAL;
    return false;
  }
#ifdef __linux__
  // Use sendfile(), available for files since linux kernel 2.6.33.
  // Use lseek with SEEK_HOLE/SEEK_DATA to skip empty blocks, available since kernel 3.1.
  int64_t offset_diff = -off;
  off_t end = off + sz;
  off_t out_offset = 0;
  while (off != end) {
    off = lseek(input_file->Fd(), off, SEEK_DATA);
    if (off == -1) {
      if (errno != ENXIO) {
        return false;
      }

      // This is only expected to happen when there is no non-empty block between the current
      // position and the end of the input file. In this case, no more copying is to be done, and
      // the output file length going to be adjusted by `SetLength` below.
      off = end;
    }
    DCHECK_GE(off, 0);

    if (off >= end) {
      // Next non-empty block is beyond the location pointed by `end` - no more data to be copied.
      // Update the offsets for both files and set the output file length as expected - as in, to
      // the values which would have been set if the block at the end of output file would have
      // been non-empty.
      off = lseek(input_file->Fd(), end, SEEK_SET);
      if (off == -1) {
        return false;
      }
      DCHECK_EQ(off, end);

      out_offset = lseek(Fd(), end + offset_diff, SEEK_SET);
      if (out_offset == -1) {
        return false;
      }
      DCHECK_EQ(out_offset, end + offset_diff);

      if (SetLength(out_offset) != 0) {
        return false;
      }
    } else {
      // Find the position of the next empty block.
      // It could be a gap within the input file or the implicit empty block at the end of the file.
      off_t hole_offset = lseek(input_file->Fd(), off, SEEK_HOLE);
      if (hole_offset == -1) {
        return false;
      }
      DCHECK_GT(hole_offset, off);

      // Update output position to match the position of the data to be written from the input file.
      out_offset = lseek(Fd(), off + offset_diff, SEEK_SET);
      if (out_offset == -1) {
        return false;
      }
      DCHECK_EQ(out_offset, off + offset_diff);

      // Write the data. If end points in the middle of the current data block, only write the data
      // until that position.
      int result = TEMP_FAILURE_RETRY(
          sendfile(Fd(), input_file->Fd(), &off, std::min(end, hole_offset) - off));
      if (result == -1) {
        return false;
      }
      // Ignore the number of bytes in `result`, sendfile() already updated `off`.
    }
  }
#else
  if (lseek(input_file->Fd(), off, SEEK_SET) != off) {
    return false;
  }
  constexpr size_t kMaxBufferSize = 16 * ::art::KB;
  const size_t buffer_size = std::min<uint64_t>(size, kMaxBufferSize);
  art::UniqueCPtr<void> buffer(malloc(buffer_size));
  if (buffer == nullptr) {
    errno = ENOMEM;
    return false;
  }
  while (size != 0) {
    size_t chunk_size = std::min<uint64_t>(buffer_size, size);
    if (!input_file->ReadFully(buffer.get(), chunk_size) ||
        !WriteFully(buffer.get(), chunk_size)) {
      return false;
    }
    size -= chunk_size;
  }
#endif
  return true;
}

bool FdFile::Unlink() {
  if (file_path_.empty()) {
    return false;
  }

  // Try to figure out whether this file is still referring to the one on disk.
  bool is_current = false;
  {
    struct stat this_stat, current_stat;
    int cur_fd = TEMP_FAILURE_RETRY(open(file_path_.c_str(), O_RDONLY | O_CLOEXEC));
    if (cur_fd > 0) {
      // File still exists.
      if (fstat(fd_, &this_stat) == 0 && fstat(cur_fd, &current_stat) == 0) {
        is_current = (this_stat.st_dev == current_stat.st_dev) &&
                     (this_stat.st_ino == current_stat.st_ino);
      }
      close(cur_fd);
    }
  }

  if (is_current) {
    unlink(file_path_.c_str());
  }

  return is_current;
}

bool FdFile::Erase(bool unlink) {
  DCHECK(!read_only_mode_);

  bool ret_result = true;
  if (unlink) {
    ret_result = Unlink();
  }

  int result;
  result = SetLength(0);
  result = Flush();
  result = Close();
  // Ignore the errors.
  (void) result;

  return ret_result;
}

int FdFile::FlushCloseOrErase() {
  DCHECK(!read_only_mode_);
  int flush_result = Flush();
  if (flush_result != 0) {
    LOG(ERROR) << "CloseOrErase failed while flushing a file.";
    Erase();
    return flush_result;
  }
  int close_result = Close();
  if (close_result != 0) {
    LOG(ERROR) << "CloseOrErase failed while closing a file.";
    Erase();
    return close_result;
  }
  return 0;
}

int FdFile::FlushClose() {
  DCHECK(!read_only_mode_);
  int flush_result = Flush();
  if (flush_result != 0) {
    LOG(ERROR) << "FlushClose failed while flushing a file.";
  }
  int close_result = Close();
  if (close_result != 0) {
    LOG(ERROR) << "FlushClose failed while closing a file.";
  }
  return (flush_result != 0) ? flush_result : close_result;
}

void FdFile::MarkUnchecked() {
  guard_state_ = GuardState::kNoCheck;
}

bool FdFile::ClearContent() {
  DCHECK(!read_only_mode_);
  if (SetLength(0) < 0) {
    PLOG(ERROR) << "Failed to reset the length";
    return false;
  }
  return ResetOffset();
}

bool FdFile::ResetOffset() {
  DCHECK(!read_only_mode_);
  off_t rc =  TEMP_FAILURE_RETRY(lseek(fd_, 0, SEEK_SET));
  if (rc == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to reset the offset";
    return false;
  }
  return true;
}

int FdFile::Compare(FdFile* other) {
  int64_t length = GetLength();
  int64_t length2 = other->GetLength();
  if (length != length2) {
    return length < length2 ? -1 : 1;
  }
  static const size_t kBufferSize = 4096;
  std::unique_ptr<uint8_t[]> buffer1(new uint8_t[kBufferSize]);
  std::unique_ptr<uint8_t[]> buffer2(new uint8_t[kBufferSize]);
  size_t offset = 0;
  while (length > 0) {
    size_t len = std::min(kBufferSize, static_cast<size_t>(length));
    if (!PreadFully(&buffer1[0], len, offset)) {
      return -1;
    }
    if (!other->PreadFully(&buffer2[0], len, offset)) {
      return 1;
    }
    int result = memcmp(&buffer1[0], &buffer2[0], len);
    if (result != 0) {
      return result;
    }
    length -= len;
    offset += len;
  }
  return 0;
}

bool FdFile::IsOpenFd(int fd) {
  if (fd == kInvalidFd) {
    return false;
  }
  #ifdef _WIN32  // Windows toolchain does not support F_GETFD.
    return true;
  #else
    int saved_errno = errno;
    bool is_open = (fcntl(fd, F_GETFD) != -1);
    errno = saved_errno;
    return is_open;
  #endif
}

}  // namespace unix_file

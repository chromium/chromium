// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "protostore/file-storage.h"

#include <cstdint>
#include <sys/stat.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace protostore {
namespace {
absl::Status IOError(absl::string_view context) {
  std::string message = absl::StrCat(context, ": ", strerror(errno));
  switch (errno) {
    case 0:
      return absl::OkStatus();
      break;
    case EINVAL:        // Invalid argument
    case ENAMETOOLONG:  // Filename too long
    case E2BIG:         // Argument list too long
    case EDESTADDRREQ:  // Destination address required
    case EDOM:          // Mathematics argument out of domain of function
    case EFAULT:        // Bad address
    case EILSEQ:        // Illegal byte sequence
    case ENOPROTOOPT:   // Protocol not available
    case ENOSTR:        // Not a STREAM
    case ENOTSOCK:      // Not a socket
    case ENOTTY:        // Inappropriate I/O control operation
    case EPROTOTYPE:    // Protocol wrong type for socket
    case ESPIPE:        // Invalid seek
      return absl::InvalidArgumentError(message);
      break;
    case ETIMEDOUT:  // Connection timed out
    case ETIME:      // Timer expired
      return absl::DeadlineExceededError(message);
      break;
    case ENODEV:  // No such device
    case ENOENT:  // No such file or directory
    case ENXIO:   // No such device or address
    case ESRCH:   // No such process
      return absl::NotFoundError(message);
      break;
    case EEXIST:         // File exists
    case EADDRNOTAVAIL:  // Address not available
    case EALREADY:       // Connection already in progress
      return absl::AlreadyExistsError(message);
      break;
    case EPERM:   // Operation not permitted
    case EACCES:  // Permission denied
    case EROFS:   // Read only file system
      return absl::PermissionDeniedError(message);
      break;
    case ENOTEMPTY:   // Directory not empty
    case EISDIR:      // Is a directory
    case ENOTDIR:     // Not a directory
    case EADDRINUSE:  // Address already in use
    case EBADF:       // Invalid file descriptor
    case EBUSY:       // Device or resource busy
    case ECHILD:      // No child processes
    case EISCONN:     // Socket is connected
#if !defined(_WIN32)
    case ENOTBLK:     // Block device required
#endif
    case ENOTCONN:    // The socket is not connected
    case EPIPE:       // Broken pipe
#if !defined(_WIN32)
    case ESHUTDOWN:   // Cannot send after transport endpoint shutdown
#endif
    case ETXTBSY:     // Text file busy
      return absl::FailedPreconditionError(message);
      break;
    case ENOSPC:   // No space left on device
#if !defined(_WIN32)
    case EDQUOT:   // Disk quota exceeded
#endif
    case EMFILE:   // Too many open files
    case EMLINK:   // Too many links
    case ENFILE:   // Too many open files in system
    case ENOBUFS:  // No buffer space available
    case ENODATA:  // No message is available on the STREAM read queue
    case ENOMEM:   // Not enough space
    case ENOSR:    // No STREAM resources
#if !defined(_WIN32)
    case EUSERS:   // Too many users
#endif
      return absl::ResourceExhaustedError(message);
      break;
    case EFBIG:      // File too large
    case EOVERFLOW:  // Value too large to be stored in data type
    case ERANGE:     // Result too large
      return absl::OutOfRangeError(message);
      break;
    case ENOSYS:           // Function not implemented
    case ENOTSUP:          // Operation not supported
    case EAFNOSUPPORT:     // Address family not supported
#if !defined(_WIN32)
    case EPFNOSUPPORT:     // Protocol family not supported
#endif
    case EPROTONOSUPPORT:  // Protocol not supported
#if !defined(_WIN32)
    case ESOCKTNOSUPPORT:  // Socket type not supported
#endif
    case EXDEV:            // Improper link
      return absl::UnimplementedError(message);
      break;
    case EAGAIN:        // Resource temporarily unavailable
    case ECONNREFUSED:  // Connection refused
    case ECONNABORTED:  // Connection aborted
    case ECONNRESET:    // Connection reset
    case EINTR:         // Interrupted function call
#if !defined(_WIN32)
    case EHOSTDOWN:     // Host is down
#endif
    case EHOSTUNREACH:  // Host is unreachable
    case ENETDOWN:      // Network is down
    case ENETRESET:     // Connection aborted by network
    case ENETUNREACH:   // Network unreachable
    case ENOLCK:        // No locks available
    case ENOLINK:       // Link has been severed
#if !(defined(__APPLE__) || defined(__FreeBSD__) || defined(_WIN32))
    case ENONET:  // Machine is not on the network
#endif
      return absl::UnavailableError(message);
      break;
    case EDEADLK:  // Resource deadlock avoided
#if !defined(_WIN32)
    case ESTALE:   // Stale file handle
#endif
      return absl::AbortedError(message);
      break;
    case ECANCELED:  // Operation cancelled
      return absl::CancelledError(message);
      break;
    // NOTE: If you get any of the following (especially in a
    // reproducible way) and can propose a better mapping,
    // please email the owners about updating this mapping.
    // case EBADMSG:      // Bad message
    // case EIDRM:        // Identifier removed
    // case EINPROGRESS:  // Operation in progress
    // case EIO:          // I/O error
    // case ELOOP:        // Too many levels of symbolic links
    // case ENOEXEC:      // Exec format error
    // case ENOMSG:       // No message of the desired type
    // case EPROTO:       // Protocol error
    // #if !defined(_WIN32)
    // case EREMOTE:      // Object is remote
    // #endif
    default:
      return absl::UnknownError(message);
      break;
  }
}

absl::Status IncrementReadBuffer(absl::string_view filename,
    ssize_t bytes_read, char** buffer, size_t* bytes_to_read) {
  if (bytes_read > 0) {
    (*buffer) += bytes_read;
    (*bytes_to_read) -= bytes_read;
  } else if (bytes_read == 0) {
    return absl::OutOfRangeError(filename);
  } else if (errno == EINTR || errno == EAGAIN) {
    // Retry
  } else {
    return IOError(filename);
  }
  return absl::OkStatus();
}
}  // namespace

InputStream::InputStream(absl::string_view filename, FILE* file)
  : filename_(filename), file_(file) {}

InputStream::~InputStream() {
  if (file_ != nullptr) {
    fclose(file_);
    file_ = nullptr;
  }
}

absl::Status InputStream::Read(size_t n, absl::string_view* result,
    char* scratch) {
  absl::Status s;
  char* dst = scratch;
  while (n > 0 && s.ok()) {
    ssize_t bytes_read = fread(dst, 1, n, file_);
    s = IncrementReadBuffer(filename_, bytes_read, &dst, &n);
  }
  *result = absl::string_view(scratch, dst - scratch);
  return s;
}

OutputStream::OutputStream(absl::string_view filename, FILE* file)
  : filename_(filename), file_(file) {}

OutputStream::~OutputStream() {
  if (file_ != nullptr) {
    Close().IgnoreError();
  }
}

absl::Status OutputStream::Append(absl::string_view data) {
  size_t r = fwrite(data.data(), 1, data.size(), file_);
  if (r != data.size()) {
    return IOError(filename_);
  }
  return absl::OkStatus();
}

absl::Status OutputStream::Close() {
  absl::Status result;
  if (fflush(file_) != 0) {
    return IOError(filename_);
  }
  if (fclose(file_) != 0) {
    result = IOError(filename_);
  }
  file_ = NULL;
  return result;
}

absl::StatusOr<uint64_t> FileStorage::GetFileSize(
    const std::string& filename) const {
  struct stat sbuf;
  if (stat(filename.c_str(), &sbuf) != 0) {
    return IOError(filename);
  }
  return sbuf.st_size;
}

absl::StatusOr<std::unique_ptr<InputStream>> FileStorage::OpenForRead(
  const std::string& filename) const {
  FILE* file = fopen(filename.c_str(), "rb");
  if (file == nullptr) {
    return IOError(filename);
  }
  return absl::make_unique<InputStream>(filename, file);
}

absl::StatusOr<std::unique_ptr<OutputStream>> FileStorage::OpenForWrite(
  const std::string& filename) const {
  FILE* file = fopen(filename.c_str(), "wb");
  if (file == nullptr) {
    return IOError(filename);
  }
  return absl::make_unique<OutputStream>(filename, file);
}

}  // namespace protostore

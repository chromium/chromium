/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

#ifdef ABSL_HAVE_MMAP
#include <sys/mman.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cstdint>
#include <memory>
#include <string>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"

namespace tflite {
namespace task {
namespace core {
namespace {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;

#ifdef _WIN32
StatusOr<std::wstring> Utf8ToWideChar(const std::string& utf8str) {
  const auto generate_error_status = [] {
    return CreateStatusWithPayload(
        StatusCode::kUnknown,
        absl::StrFormat("Unable to convert UTF8 to wide string, error=%d",
                        ::GetLastError()),
        TfLiteSupportStatus::kFileMmapError);
  };

  const int size_required =
    MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(),
                        static_cast<int>(utf8str.size()), NULL, 0);
  if (size_required == 0) {
    return generate_error_status();
  }

  std::wstring ws_translated_str(size_required, L'\0');
  int characters_written =
    MultiByteToWideChar(
        CP_UTF8, 0, utf8str.c_str(),
        static_cast<int>(utf8str.size()), &ws_translated_str[0],
        size_required);
  if (characters_written == 0) {
    return generate_error_status();
  }

  return ws_translated_str;
}
#endif

// Gets the offset aligned to page size for mapping given files into memory by
// file descriptor correctly, as according to mmap(2), the offset used in mmap
// must be a multiple of sysconf(_SC_PAGE_SIZE).
int64_t GetPageSizeAlignedOffset(int64_t offset) {
#ifdef _WIN32
  _SYSTEM_INFO info = {};
  ::GetNativeSystemInfo(&info);
  int64_t allocation_granularity = info.dwAllocationGranularity;
#else
  int64_t allocation_granularity = sysconf(_SC_PAGE_SIZE);
#endif
  int64_t aligned_offset = offset;
  if (offset % allocation_granularity != 0) {
    aligned_offset = offset / allocation_granularity * allocation_granularity;
  }
  return aligned_offset;
}

}  // namespace

/* static */
StatusOr<std::unique_ptr<ExternalFileHandler>>
ExternalFileHandler::CreateFromExternalFile(const ExternalFile* external_file) {
  // Use absl::WrapUnique() to call private constructor:
  // https://abseil.io/tips/126.
  std::unique_ptr<ExternalFileHandler> handler =
      absl::WrapUnique(new ExternalFileHandler(external_file));

  TFLITE_RETURN_IF_ERROR(handler->MapExternalFile());

  return handler;
}

absl::Status ExternalFileHandler::MapExternalFile() {
  if (!external_file_.file_content().empty()) {
    return absl::OkStatus();
  }

  if (external_file_.file_name().empty() &&
      !external_file_.has_file_descriptor_meta()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "ExternalFile must specify at least one of 'file_content', 'file_name' "
        "or 'file_descriptor_meta'.",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
#ifdef _WIN32
  HANDLE file_handle = nullptr;

  if (!external_file_.file_name().empty()) {
    StatusOr<std::wstring> wide_file_name =
        Utf8ToWideChar(external_file_.file_name());
    if (!wide_file_name.ok()) {
      return wide_file_name.status();
    }

    owned_file_handle_ = ::CreateFileW(wide_file_name->c_str(), GENERIC_READ,
        /*dwShareMode=*/0, NULL, OPEN_EXISTING, /*dwFlagsAndAttributes=*/0,
        NULL);
    if (owned_file_handle_ == INVALID_HANDLE_VALUE) {
      owned_file_handle_ = nullptr;

      const std::string error_message = absl::StrFormat(
          "Unable to open file at %s", external_file_.file_name());
      switch (::GetLastError()) {
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
        return CreateStatusWithPayload(
            StatusCode::kNotFound, error_message,
            TfLiteSupportStatus::kFileNotFoundError);

      case ERROR_ACCESS_DENIED:
      case ERROR_LOCK_VIOLATION:
        return CreateStatusWithPayload(
            StatusCode::kPermissionDenied, error_message,
            TfLiteSupportStatus::kFilePermissionDeniedError);

      case ERROR_OUTOFMEMORY:
      case ERROR_NOT_ENOUGH_MEMORY:
        return CreateStatusWithPayload(
            StatusCode::kResourceExhausted, error_message,
            TfLiteSupportStatus::kFileReadError);

      case ERROR_NOT_READY:
        // The device is not ready.
      case ERROR_SECTOR_NOT_FOUND:
        // The drive cannot find the sector requested.
      case ERROR_GEN_FAILURE:
        // A device ... is not functioning.
      case ERROR_DEV_NOT_EXIST:
        // Net resource or device is no longer available.
      case ERROR_IO_DEVICE:
      case ERROR_DISK_OPERATION_FAILED:
      case ERROR_FILE_CORRUPT:
        // File or directory is corrupted and unreadable.
      case ERROR_DISK_CORRUPT:
        // The disk structure is corrupted and unreadable.
        return CreateStatusWithPayload(
            StatusCode::kDataLoss, error_message,
            TfLiteSupportStatus::kFileReadError);
      }
    }
    file_handle = owned_file_handle_;
  } else {
    file_handle =
      reinterpret_cast<HANDLE>(external_file_.file_descriptor_meta().handle());
    if (!file_handle) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Provided file handle is invalid: %p", file_handle),
          TfLiteSupportStatus::kInvalidArgumentError);
    }
    buffer_offset_ = external_file_.file_descriptor_meta().offset();
    buffer_size_ = external_file_.file_descriptor_meta().length();
  }

  LARGE_INTEGER size;
  if (!::GetFileSizeEx(file_handle, &size) || size.QuadPart < 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown,
        absl::StrFormat("Unable to get file size, error=%d", ::GetLastError()),
        TfLiteSupportStatus::kFileReadError);
  }
  int64_t file_size = size.QuadPart;

  // Deduce buffer size if not explicitly provided through file descriptor.
  if (buffer_size_ <= 0) {
    buffer_size_ = file_size - buffer_offset_;
  }
  // Check for out of range issues.
  if (file_size <= buffer_offset_) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Provided file offset (%d) exceeds or matches actual "
                        "file length (%d)",
                        buffer_offset_, file_size),
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (file_size < buffer_size_ + buffer_offset_) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Provided file length + offset (%d) exceeds actual "
                        "file length (%d)",
                        buffer_size_ + buffer_offset_, file_size),
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  // If buffer_offset_ is not multiple of sysconf(_SC_PAGE_SIZE), align with
  // extra leading bytes and adjust buffer_size_ to account for the extra
  // leading bytes.
  buffer_aligned_offset_ = GetPageSizeAlignedOffset(buffer_offset_);
  buffer_aligned_size_ = buffer_size_ + buffer_offset_ - buffer_aligned_offset_;

  file_mapping_ = ::CreateFileMapping(file_handle, nullptr, PAGE_READONLY, 0, 0,
                                      nullptr);
  if (!file_mapping_) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown,
        absl::StrFormat("Unable to create file mapping, error=%d",
                        ::GetLastError()),
        TfLiteSupportStatus::kFileMmapError);
  }

  buffer_ = ::MapViewOfFile(file_mapping_, FILE_MAP_READ, 0, 0, 0);

  if (!buffer_) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown,
        absl::StrFormat("Unable to map view of file, error=%d",
                        ::GetLastError()),
        TfLiteSupportStatus::kFileMmapError);
  }

#else
  // Obtain file descriptor, offset and size.
  int fd = -1;
  if (!external_file_.file_name().empty()) {
    owned_fd_ = open(external_file_.file_name().c_str(), O_RDONLY);
    if (owned_fd_ < 0) {
      const std::string error_message = absl::StrFormat(
          "Unable to open file at %s", external_file_.file_name());
      switch (errno) {
        case ENOENT:
          return CreateStatusWithPayload(
              StatusCode::kNotFound, error_message,
              TfLiteSupportStatus::kFileNotFoundError);
        case EACCES:
        case EPERM:
          return CreateStatusWithPayload(
              StatusCode::kPermissionDenied, error_message,
              TfLiteSupportStatus::kFilePermissionDeniedError);
        case EINTR:
          return CreateStatusWithPayload(StatusCode::kUnavailable,
                                         error_message,
                                         TfLiteSupportStatus::kFileReadError);
        case EBADF:
          return CreateStatusWithPayload(StatusCode::kFailedPrecondition,
                                         error_message,
                                         TfLiteSupportStatus::kFileReadError);
        default:
          return CreateStatusWithPayload(
              StatusCode::kUnknown,
              absl::StrFormat("%s, errno=%d", error_message, errno),
              TfLiteSupportStatus::kFileReadError);
      }
    }
    fd = owned_fd_;
  } else {
    fd = external_file_.file_descriptor_meta().fd();
    if (fd < 0) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Provided file descriptor is invalid: %d < 0", fd),
          TfLiteSupportStatus::kInvalidArgumentError);
    }
    buffer_offset_ = external_file_.file_descriptor_meta().offset();
    buffer_size_ = external_file_.file_descriptor_meta().length();
  }
  // Get actual file size. Always use 0 as offset to lseek(2) to get the actual
  // file size, as SEEK_END returns the size of the file *plus* offset.
  size_t file_size = lseek(fd, /*offset=*/0, SEEK_END);
  if (file_size <= 0) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown,
        absl::StrFormat("Unable to get file size, errno=%d", errno),
        TfLiteSupportStatus::kFileReadError);
  }
  // Deduce buffer size if not explicitly provided through file descriptor.
  if (buffer_size_ <= 0) {
    buffer_size_ = file_size - buffer_offset_;
  }
  // Check for out of range issues.
  if (file_size <= buffer_offset_) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Provided file offset (%d) exceeds or matches actual "
                        "file length (%d)",
                        buffer_offset_, file_size),
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (file_size < buffer_size_ + buffer_offset_) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Provided file length + offset (%d) exceeds actual "
                        "file length (%d)",
                        buffer_size_ + buffer_offset_, file_size),
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  // If buffer_offset_ is not multiple of sysconf(_SC_PAGE_SIZE), align with
  // extra leading bytes and adjust buffer_size_ to account for the extra
  // leading bytes.
  buffer_aligned_offset_ = GetPageSizeAlignedOffset(buffer_offset_);
  buffer_aligned_size_ = buffer_size_ + buffer_offset_ - buffer_aligned_offset_;
  // Map into memory.
  buffer_ = mmap(/*addr=*/nullptr, buffer_aligned_size_, PROT_READ, MAP_SHARED,
                 fd, buffer_aligned_offset_);
  if (buffer_ == MAP_FAILED) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown,
        absl::StrFormat("Unable to map file to memory buffer, errno=%d", errno),
        TfLiteSupportStatus::kFileMmapError);
  }
#endif
  return absl::OkStatus();
}

absl::string_view ExternalFileHandler::GetFileContent() {
  if (!external_file_.file_content().empty()) {
    return external_file_.file_content();
  } else {
    return absl::string_view(static_cast<const char*>(buffer_) +
                                 buffer_offset_ - buffer_aligned_offset_,
                             buffer_size_);
  }
}

ExternalFileHandler::~ExternalFileHandler() {
#ifdef _WIN32
  if (buffer_) {
    ::UnmapViewOfFile(buffer_);
  }
  if (file_mapping_) {
    ::CloseHandle(file_mapping_);
  }
#else
  if (buffer_ != MAP_FAILED) {
    munmap(buffer_, buffer_aligned_size_);
  }
  if (owned_fd_ >= 0) {
    close(owned_fd_);
  }
#endif
}

}  // namespace core
}  // namespace task
}  // namespace tflite

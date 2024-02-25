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

#include <memory>
#include <string>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"

namespace tflite {
namespace task {
namespace core {
namespace {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;

// Gets the offset aligned to page size for mapping given files into memory by
// file descriptor correctly, as according to mmap(2), the offset used in mmap
// must be a multiple of sysconf(_SC_PAGE_SIZE).
int64 GetPageSizeAlignedOffset(int64 offset) {
#ifdef _WIN32
  // mmap is not used on Windows
  return -1;
#else
  int64 aligned_offset = offset;
  int64 page_size = sysconf(_SC_PAGE_SIZE);
  if (offset % page_size != 0) {
    aligned_offset = offset / page_size * page_size;
  }
  return aligned_offset;
#endif
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
// TODO(b/195588083): Add Windows support
#ifdef _WIN32
  return CreateStatusWithPayload(
      StatusCode::kFailedPrecondition,
      "File loading is not yet supported on Windows",
      TfLiteSupportStatus::kFileReadError);
#else
  if (external_file_.file_name().empty() &&
      !external_file_.has_file_descriptor_meta()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "ExternalFile must specify at least one of 'file_content', 'file_name' "
        "or 'file_descriptor_meta'.",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
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
  return absl::OkStatus();
#endif
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
#ifndef _WIN32
  if (buffer_ != MAP_FAILED) {
    munmap(buffer_, buffer_aligned_size_);
  }
#endif
  if (owned_fd_ >= 0) {
    close(owned_fd_);
  }
}

}  // namespace core
}  // namespace task
}  // namespace tflite

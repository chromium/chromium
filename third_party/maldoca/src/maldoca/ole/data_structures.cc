// Copyright 2021 Google LLC
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

#include "maldoca/ole/data_structures.h"

#include <memory>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status.h"
#include "maldoca/base/status_macros.h"
#include "maldoca/base/statusor.h"
#include "maldoca/ole/endian_reader.h"

namespace maldoca {
namespace ole {
namespace data_structure {
namespace {
typedef struct _CompObjHeader {
  uint32_t reserved1;
  uint32_t version;
  char reserved2[20];
} CompObjHeader;

// Attempt to parse LengthPrefixedAnsiString, updating context by the
// amount of bytes consumed.
// See section 2.1.4 of [MS-OLEDS]
StatusOr<std::string> ConsumeLengthPrefixedAnsiString(
    absl::string_view *content) {
  uint32_t length = 0;
  if (!LittleEndianReader::ConsumeUInt32(content, &length)) {
    return ::maldoca::ResourceExhaustedError(
        "Stream too small for LengthPrefixedAnsiString header",
        MaldocaErrorCode::PREFIXED_ANSI_STRING_HEADER_TOO_SHORT);
  }
  std::string str;
  if (!LittleEndianReader::ConsumeString(content, length, &str)) {
    return ::maldoca::ResourceExhaustedError(
        "Stream too small for LengthPrefixedAnsiString string",
        MaldocaErrorCode::PREFIXED_ANSI_STRING_CONTENT_TOO_SHORT);
  }
  if (!str.empty()) {
    // remmove the null-termination
    str.resize(str.size() - 1);
  }
  return str;
}

// Attempt to parse ClipboardFormatOrAnsiString, updating context by the
// amount of bytes consumed.
// See section 2.3.1 of [MS-OLEDS]
StatusOr<std::string> ConsumeClipboardFormatOrAnsiString(
    absl::string_view *content) {
  uint32_t marker_or_length = 0;
  if (!LittleEndianReader::ConsumeUInt32(content, &marker_or_length)) {
    return ::maldoca::ResourceExhaustedError(
        "Stream too small for ClipboardFormatOrAnsiString header",
        MaldocaErrorCode::CLIPBOARD_FORMAT_OR_ANSI_STRING_TOO_SHORT);
  }
  switch (marker_or_length) {
    case 0:
      // next field must not be present
      return std::string();
    case 0xfffffffe:
    case 0xffffffff: {
      // clipboard format follow (4 bytes)
      // TODO(somebody): support this.
      return std::string();
    }
    default: {
      // String
      std::string str;
      if (!LittleEndianReader::ConsumeString(content, marker_or_length, &str)) {
        return ::maldoca::ResourceExhaustedError(
            "Stream too small for LengthPrefixedAnsiString string",
            MaldocaErrorCode::PREFIXED_ANSI_STRING_CONTENT_TOO_SHORT);
      }
      if (!str.empty()) {
        // remmove the null-termination
        str.resize(str.size() - 1);
      }
      return str;
    }
  }
}
}  // namespace
absl::Status CompObj::Parse(absl::string_view content) {
  // Parse the header
  if (content.size() < sizeof(CompObjHeader)) {
    DLOG(WARNING) << "CompObj size " << content.size()
                  << " too small for header " << sizeof(CompObjHeader);
    return absl::ResourceExhaustedError("Stream too small for header");
  }
  auto header = reinterpret_cast<const CompObjHeader *>(content.data());
  version_ = header->version;
  content.remove_prefix(sizeof(CompObjHeader));

  // Parse AnsiUserType
  auto status_or = ConsumeLengthPrefixedAnsiString(&content);
  MALDOCA_RETURN_IF_ERROR(status_or.status(),
                          _ << "Failed to parse AnsiUserType");
  if (content.empty()) {
    return absl::OkStatus();
  }
  user_type_ = status_or.value();

  // Parse AnsiClipboardFormat
  status_or = ConsumeClipboardFormatOrAnsiString(&content);
  MALDOCA_RETURN_IF_ERROR(status_or.status(),
                          _ << "Failed to parse AnsiClipboardFormat");
  clipboard_format_ = status_or.value();

  // Parse Reserved
  status_or = ConsumeLengthPrefixedAnsiString(&content);
  MALDOCA_RETURN_IF_ERROR(status_or.status(), _ << "Failed to parse Reserved");
  if (content.empty()) {
    return absl::OkStatus();
  }
  reserved_ = status_or.value();

  return absl::OkStatus();
}

std::string OleNativeEmbedded::FileHash() {
  if (!file_hash_.empty()) {
    return file_hash_;
  }
  if (file_content_.empty()) {
    return {};
  }
  file_hash_ = Sha256HexString(file_content_);
  return file_hash_;
}

absl::Status OleNativeEmbedded::Parse(absl::string_view content,
                                      uint32_t stream_size) {
  // parse native size
  if (!LittleEndianReader::ConsumeUInt32(&content, &native_size_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to obtain native size from bytes[0:3] for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_SIZE_FAIL);
  }

  // sanity check - stream size should match embedded native size.
  if (stream_size != native_size_ + sizeof(native_size_)) {
    return ::maldoca::OutOfRangeError(
        "OleNativeEmbedded stream size does not match embedded native size",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_SIZE_MISMATCH);
  }

  // parse type
  if (!LittleEndianReader::ConsumeUInt16(&content, &type_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to parse type from bytes[4:5] for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_TYPE_FAIL);
  }

  // parse file name
  if (!LittleEndianReader::ConsumeNullTerminatedString(&content, &file_name_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to parse filename for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_FILENAME_FAIL);
  }

  // parse file path
  if (!LittleEndianReader::ConsumeNullTerminatedString(&content, &file_path_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to parse filepath for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_FILEPATH_FAIL);
  }

  // parse reserved
  if (!LittleEndianReader::ConsumeUInt64(&content, &reserved_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to parse reserved for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_RESERVED_FAIL);
  }

  // parse temp path
  if (!LittleEndianReader::ConsumeNullTerminatedString(&content, &temp_path_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to parse temppath for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_TEMPPATH_FAIL);
  }

  // parse file size
  if (!LittleEndianReader::ConsumeUInt32(&content, &file_size_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to parse filesize for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_FILESIZE_FAIL);
  }

  // sanity check - file size should be less than EOF
  if (file_size_ > content.size()) {
    return ::maldoca::OutOfRangeError(
        "OleNativeEmbedded parsed filesize is out of range",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_FILESIZE_MISMATCH);
  }

  // parse file content
  if (!LittleEndianReader::ConsumeString(&content, file_size_,
                                         &file_content_)) {
    return ::maldoca::ResourceExhaustedError(
        "Unable to parse content for OleNativeEmbedded",
        MaldocaErrorCode::OLE_NATIVE_EMBEDDED_PARSE_CONTENT_FAIL);
  }

  return absl::OkStatus();
}
}  // namespace data_structure
}  // namespace ole
}  // namespace maldoca

/*
 * Copyright 2021 Google LLC
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

// This module supports some of the contents of the following Microsoft spec:
// [MS-OLEDS]: Object Linking and Embedding (OLE) Data Structures
// https://msdn.microsoft.com/en-us/library/dd942265.aspx

#ifndef MALDOCA_OLE_DATA_STRUCTURES_H_
#define MALDOCA_OLE_DATA_STRUCTURES_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "maldoca/ole/endian_reader.h"

namespace maldoca {
namespace ole {
namespace data_structure {
class CompObj {
 public:
  CompObj() = default;

  absl::Status Parse(absl::string_view content);

  uint32_t Version() const { return version_; }
  std::string UserType() const { return user_type_; }
  std::string ClipboardFormat() const { return clipboard_format_; }
  std::string Reserved() const { return reserved_; }

 private:
  uint32_t version_ = 0;
  std::string user_type_;
  std::string clipboard_format_;
  std::string reserved_;
};

// OLENative Embedded Packager Object type
// See MSOLEDS 1.3.1 and 2.3.6
class OleNativeEmbedded {
 public:
  OleNativeEmbedded() = default;

  absl::Status Parse(absl::string_view content, uint32_t stream_size);

  uint32_t NativeSize() const { return native_size_; }
  uint16_t Type() const { return type_; }
  std::string FileName() const { return file_name_; }
  std::string FilePath() const { return file_path_; }
  uint64_t Reserved() const { return reserved_; }
  std::string TempPath() const { return temp_path_; }
  uint32_t FileSize() const { return file_size_; }
  std::string FileContent() const { return file_content_; }
  std::string FileHash();  // sha256 of file_content_

 private:
  uint32_t native_size_;
  uint16_t type_;
  std::string file_name_;
  std::string file_path_;
  uint64_t reserved_;
  std::string temp_path_;
  uint32_t file_size_;
  std::string file_content_;
  std::string file_hash_;
};
}  // namespace data_structure
}  // namespace ole
}  // namespace maldoca
#endif  // MALDOCA_OLE_DATA_STRUCTURES_H_

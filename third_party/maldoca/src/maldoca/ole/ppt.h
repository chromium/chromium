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

// Support for the PowerPoint (.ppt) binary format.
//
// Implements parts relevant to extraction of VBA code of the [MS-PPT] file
// format, as documented at:
// https://msdn.microsoft.com/en-us/library/cc313106(v=office.12).aspx
#ifndef MALDOCA_OLE_PPT_H_
#define MALDOCA_OLE_PPT_H_

#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/statusor.h"

namespace maldoca {

// Parser for the PowerPoint RecordHeader structure. This header is found at the
// beginning of each container record and each atom record of a PPT document and
// is expected to be 8 bytes long.
//
// Reference:
// https://msdn.microsoft.com/en-us/library/dd926377%28v=office.12%29.aspx
//
// Sample usage:
//
//   RecordHeader h;
//   CHECK_OK(h.Parse(ppt_stream));
class RecordHeader {
 public:
  // Values for the "Type" field of the RecordHeader structure.
  //
  // Reference:
  // https://msdn.microsoft.com/en-us/library/dd945336(v=office.12).aspx
  enum RecordHeaderType {
    kExternalOleObjectStg = 0x1011,  // VBAProjectStorage*Atom
  };

  static constexpr size_t SizeOf() { return 8; }

  // Parses a RecordHeader structure.
  absl::Status Parse(absl::string_view input);

  RecordHeader() = default;

  // Getters
  uint8_t Version() const { return version_; }
  uint16_t Instance() const { return instance_; }
  uint16_t Type() const { return type_; }
  uint32_t Length() const { return length_; }

 private:
  // RecordHeader fields, see the RecordHeader documentation for meaning
  uint8_t version_;
  uint16_t instance_;
  uint16_t type_;
  uint32_t length_;
};

// Parser for the VbaProjectStgUncompressedAtom and VbaProjectStgCompressedAtom
// structures. These structures start with a RecordHeader and their payload is a
// VBA Project stored as structured storage (OLE).
//
// References:
// https://msdn.microsoft.com/en-us/library/dd952169(v=office.12).aspx
// https://msdn.microsoft.com/en-us/library/dd943342(v=office.12).aspx
class VBAProjectStorage {
 public:
  // Byte pattern identifing VbaProjectStgUncompressedAtom, a RecordHeader with:
  // .recVer = 0x0
  // .revInstance = 0x000
  // .recType = 0x1011 (RT_ExternalOleObjectStg)
  static const absl::string_view UncompressedHeaderPattern() {
    static const absl::string_view pattern("\x00\x00\x11\x10", 4);
    return pattern;
  }

  // Byte pattern identifing VbaProjectStgCompressedAtom, a RecordHeader with:
  // .recVer = 0x0
  // .revInstance = 0x001
  // .recType = 0x1011 (RT_ExternalOleObjectStg)
  static const absl::string_view CompressedHeaderPattern() {
    static const absl::string_view pattern("\x10\x00\x11\x10", 4);
    return pattern;
  }

  // Returns a string containing the content of the VBA Project Storage,
  // decompressing it if compressed content is specified in the RecordHeader.
  //
  static StatusOr<std::string> ExtractContent(absl::string_view storage);

  // Checks that the VBA Project Storage content has the expected CDF header.
  static absl::Status IsContentValid(absl::string_view content);

 private:
  // Values for the "Instance" field of the VBAProjectStorage*Atom RecordHeader.
  // See: https://msdn.microsoft.com/en-us/library/dd908115(v=office.12).aspx
  enum RecordHeaderInstance {
    kUncompressed = 0x000,
    kCompressed = 0x001,
  };

  // Value of the "Version" field of the VBAProjectStorage*Atom RecordHeader.
  static constexpr uint8_t kRecordHeaderVersion = 0x00;

  // Header of the VBA Project Storage content, which is the start of a CDF
  // file.
  static const absl::string_view CDFHeader() {
    static const absl::string_view header{"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1"};
    return header;
  }

  // Parses a compressed VBA Project Storage.
  // Reference:
  // https://msdn.microsoft.com/en-us/library/dd943342(v=office.12).aspx
  static StatusOr<std::string> ExtractCompressedContent(
      absl::string_view prj_storage);

  // Returns the zlib decompressed content of the compressed VBA project
  // storage. The compression algorithm used is the DEFLATE standard. Uses the
  // decompressed size to allocate a big enough buffer. Does not check if the
  // actual decompressed content matches.
  static StatusOr<std::string> Decompress(absl::string_view compressed_storage,
                                          size_t decompressed_size);

  VBAProjectStorage() = delete;
  ~VBAProjectStorage() = delete;
  VBAProjectStorage(const VBAProjectStorage&) = delete;
  VBAProjectStorage& operator=(const VBAProjectStorage&) = delete;
};

// Extracts VBA projects from the PowerPoint Document stream. The VBA projects
// are stored as OLE files. Returns an empty vector if no VBA projects were
// found.
StatusOr<std::vector<std::string>> PPT97ExtractVBAStorage(
    absl::string_view ppt_stream);
}  // namespace maldoca

#endif  // MALDOCA_OLE_PPT_H_

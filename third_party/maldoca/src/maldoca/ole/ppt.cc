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

#include "maldoca/ole/ppt.h"

#include <iomanip>

#ifndef MALDOCA_IN_CHROMIUM
#include "absl/flags/flag.h"  // nogncheck
#endif
#include "absl/strings/match.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status_macros.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/endian_reader.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/header.h"
#include "maldoca/ole/oss_utils.h"
#ifdef MALDOCA_IN_CHROMIUM
#include "third_party/zlib/zlib.h"
#else
#include "zlib/include/zlib.h"
#endif
#include "zlibwrapper/zlibwrapper.h"

#ifdef MALDOCA_IN_CHROMIUM
const static int32_t ppt_deflated_max_vba_project_size = 2 << 20;
#else
ABSL_FLAG(
    int32_t, ppt_deflated_max_vba_project_size, 2 << 20,
    "The maximum deflated VBA Project Storage size (in bytes) we are "
    "willing to process. The PowerPoint 97 file format stores VBA code in "
    "structured storage (OLE) using an External Ole Object Storage "
    "which can be zlib compressed. The compressed object storage "
    "header defines a decompressed content size, which we set the maximum "
    "value of with this flag.");
#endif

namespace maldoca {

absl::Status RecordHeader::Parse(absl::string_view input) {
  // Parse recVer and recInstance (2 bytes)
  //
  // Bit   Content
  // ---------------------------
  // 0-3   recVer (4 bits)
  // 4-15  recInstance (12 bits)
  uint16_t version_instance;
  if (!LittleEndianReader::ConsumeUInt16(&input, &version_instance)) {
    return absl::ResourceExhaustedError(
        "Bytes [0:1]: Cannot read version and instance fields of the "
        "RecordHeader");
  }
  version_ = version_instance & 0x0f;
  instance_ = (version_instance & 0xfff0) >> 4;

  // Parse recType (2 bytes)
  if (!LittleEndianReader::ConsumeUInt16(&input, &type_)) {
    return absl::ResourceExhaustedError(
        "Bytes [2:3]: Cannot read type field of the RecordHeader");
  }

  // Parse recLen (4 bytes)
  if (!LittleEndianReader::ConsumeUInt32(&input, &length_)) {
    return absl::ResourceExhaustedError(
        "Bytes [4:7]: Cannot read length field of the RecordHeader");
  }

  return absl::OkStatus();
}

StatusOr<std::string> VBAProjectStorage::ExtractContent(
    absl::string_view prj_storage) {
  // Read the Record Header at the beginning of the VBA Project Storage Atom
  RecordHeader header;
  MALDOCA_RETURN_IF_ERROR(header.Parse(prj_storage),
                          _ << "Could not parse Record Header");

  // Sanity check the content of the Record Header
  if (header.Version() != kRecordHeaderVersion) {
    auto status = absl::FailedPreconditionError(absl::StrCat(
        "Invalid record header version field, expected ",
        absl::StrCat(kRecordHeaderVersion), " got ", header.Version()));
    LOG(WARNING) << status.message();
    return status;
  }

  if (header.Type() != RecordHeader::RecordHeaderType::kExternalOleObjectStg) {
    auto status = absl::FailedPreconditionError(
        absl::StrCat("Invalid record header type field, expected ",
                     RecordHeader::RecordHeaderType::kExternalOleObjectStg,
                     " got ", header.Type()));
    LOG(WARNING) << status.message();
    return status;
  }

  auto required_size = header.Length() + RecordHeader::SizeOf();
  if (required_size > prj_storage.size()) {
    auto status = absl::OutOfRangeError(absl::StrCat(
        "Invalid record header length field, is ", required_size,
        ", which is larger than rest of the input: ", prj_storage.size(),
        ". File truncated?"));
    LOG(WARNING) << status.message();
    return status;
  }

  // Remove header and cut string to the size defined by the header
  auto prj_storage_payload =
      prj_storage.substr(RecordHeader::SizeOf(), header.Length());

  // Payload storage method is defined by the recInstance field. It can be
  // either uncompressed (rarely seen) or compressed (most common case).
  switch (header.Instance()) {
    case kUncompressed:
      // VBA storage content is the payload itself
      return std::string{prj_storage_payload};
    case kCompressed:
      // VBA storage payload is compressed and requires more work
      return ExtractCompressedContent(prj_storage_payload);
    default: {
      auto status = absl::FailedPreconditionError(absl::StrCat(
          "Unknown record header instance value: ", header.Instance()));
      LOG(WARNING) << status.message();
      return status;
    }
  }
}

StatusOr<std::string> VBAProjectStorage::ExtractCompressedContent(
    absl::string_view prj_storage) {
  // The compressed payload starts with the uncompressed size
  uint32_t decompressed_size;
  if (!LittleEndianReader::ConsumeUInt32(&prj_storage, &decompressed_size)) {
    return absl::OutOfRangeError("Cannot read decompressed size");
  }

  // Resource check
  if (decompressed_size > ppt_deflated_max_vba_project_size) {
    auto status = absl::FailedPreconditionError(absl::StrCat(
        "VBA Project Compressed Storage header defines a decompressed "
        "size of ",
        decompressed_size,
        ", which is larger than the allowed decompressed size: ",
        ppt_deflated_max_vba_project_size));
    LOG(WARNING) << status.message();
    return status;
  }

  // Decompress
  MALDOCA_ASSIGN_OR_RETURN(std::string content,
                           Decompress(prj_storage, decompressed_size));

  // Sanity checks of decompressed content
  // TODO(b/113256605): Store findings for further analysis.
  if (content.size() != decompressed_size) {
    DLOG(WARNING) << "Decompressed size does not match header value: is "
                  << content.size() << " expected " << decompressed_size;
  }

  auto result = VBAProjectStorage::IsContentValid(content);
  if (!result.ok()) {
    // Log error message
    DLOG(WARNING) << result;
  }

  return content;
}

absl::Status VBAProjectStorage::IsContentValid(absl::string_view content) {
  if (!absl::StartsWith(content, CDFHeader())) {
    return absl::InternalError(
        "VBA project storage content does not start with the CDF header");
  }
  return absl::OkStatus();
}

StatusOr<std::string> VBAProjectStorage::Decompress(
    absl::string_view compressed_storage, size_t decompressed_size) {
  // Prepare output string
  std::string storage;
  storage.resize(decompressed_size);

  // Decompress zlib chunks
  ZLib z;
  // Have to use a temp var due to different datatypes (size_t vs uLongf)
  uLongf decompressed_size_tmp = decompressed_size;
  int result = z.UncompressChunk(
      reinterpret_cast<Bytef *>(&(storage[0])), &decompressed_size_tmp,
      reinterpret_cast<const Byte *>(compressed_storage.data()),
      compressed_storage.size());
  decompressed_size = decompressed_size_tmp;
  if (result != Z_OK) {
    auto status = absl::FailedPreconditionError(absl::StrCat(
        "Could not decompress VBAProjectStorage, zlib error: ", result));
    LOG(WARNING) << status.message();
    return status;
  }

  // zlib updated the announced decompressed size with actual decompressed size
  storage.resize(decompressed_size);
  return storage;
}

// Return offsets of pattern in input
static void FindPatternOffsets(absl::string_view input,
                               absl::string_view pattern,
                               std::vector<size_t> *offsets) {
  size_t idx = -1;
  while (true) {
    idx = input.find(pattern, idx + 1);
    if (idx == std::string::npos) {
      break;
    }
    offsets->push_back(idx);
  }
}

StatusOr<std::vector<std::string>> PPT97ExtractVBAStorage(
    absl::string_view ppt_stream) {
  std::vector<std::string> vba_projects;

  // Instead of parsing the PowerPoint document, we search for byte patterns
  // that identify the header of the
  // ExternalOleObjectStorage{Uncompressed,Compressed}Atom structures.

  // Find uncompressed VBA Projects
  std::vector<size_t> offsets;
  FindPatternOffsets(ppt_stream, VBAProjectStorage::UncompressedHeaderPattern(),
                     &offsets);
  // Find compressed VBA Projects
  FindPatternOffsets(ppt_stream, VBAProjectStorage::CompressedHeaderPattern(),
                     &offsets);

  // Extract VBA projects at the offsets where the byte patterns where found
  for (const auto &offset : offsets) {
    DLOG(INFO) << "Found VBA Project Storage Atom found at PPT stream offset "
               << offset;
    // Make the string start with the VBA Project Storage
    auto storage_data = ppt_stream.substr(offset);

    auto result = VBAProjectStorage::ExtractContent(storage_data);
    if (IsFailedPrecondition(result.status())) {
      // Failed precondidtion errors are not fatal. Because we are searching for
      // relatively small byte patterns, it's not impossible to get false
      // positives.
      DLOG(WARNING) << "Failed preconditions to read VBA Project at offset "
                    << offset << ", could be a false positive. Error was: "
                    << result.status();
      continue;
    } else if (!result.ok()) {
      // Other type of errors is treated as error
      DLOG(WARNING) << "Could not read VBA Project at offset " << offset << ": "
                    << result.status();
      continue;
    }
    vba_projects.push_back(result.value());
  }

  return vba_projects;
}
}  // namespace maldoca

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

// Extracts metadata from the OLE2 streams and put it into OleFile protobuf
//
// Use case:
//   OleFile ole_proto;
//   maldoca::OleToProto ole_to_proto;
//   ole_to_proto.ParseOleBuffer(input_string, &ole_proto);
//
// By default all supported metadata will be extracted. Certain parts of the
// metadata can be disabled through flags specified before call to the
// ParseOleBuffer:
//   ole_to_proto.IncludeSummaryInformation(bool);
//   ole_to_proto.IncludeVbaCode(bool);
//   ole_to_proto.IncludeDirectoryStructure(bool);
//   ole_to_proto.IncludeStreamHashes(bool);
//   ole_to_proto.IncludeOleNativeMetadata(bool);
// Content extraction not enabled by default but can be enabled using flags
// specified before call to ParseOleBuffer:
//   ole_to_proto.IncludeOleNativeContent(bool);

#ifndef MALDOCA_OLE_OLE_TO_PROTO_H_
#define MALDOCA_OLE_OLE_TO_PROTO_H_

#include "absl/container/node_hash_map.h"
#include "maldoca/base/statusor.h"
#include "maldoca/ole/data_structures.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/header.h"
#include "maldoca/ole/proto/office.pb.h"
#include "maldoca/ole/proto/ole.pb.h"
#include "maldoca/ole/proto/ole_to_proto_settings.pb.h"

namespace maldoca {
class OleToProto {
 public:
  OleToProto();
  explicit OleToProto(const maldoca::ole::OleToProtoSettings &settings);
  virtual ~OleToProto() {}

  absl::Status ParseOleBuffer(absl::string_view in_buf,
                              maldoca::ole::OleFile *ole_proto);

 private:
  maldoca::OLEHeader header_;
  std::vector<uint32_t> fat_;
  maldoca::OLEDirectoryEntry root_;
  maldoca::OLEDirectoryEntry *vba_root_;

  maldoca::ole::OleToProtoSettings settings_;

  absl::node_hash_map<std::string, std::string> code_modules_;
  void PrepareForVBA(absl::string_view in_buf);
  void DumpStreams(
      absl::string_view in_buf, const maldoca::OLEDirectoryEntry *entry,
      int level, int32_t parent_index, maldoca::ole::OleFile *ole_proto,
      google::protobuf::RepeatedPtrField<maldoca::ole::OleDirectoryEntry>
          *dirs);
  bool ParseSummaryInformationStream(const std::string &stream_content,
                                     const maldoca::OLEDirectoryEntry &entry,
                                     maldoca::ole::OleFile *ole_proto);
  bool ParseDocumentSummaryInformationStream(
      const std::string &stream_content,
      const maldoca::OLEDirectoryEntry &entry,
      maldoca::ole::OleFile *ole_proto);
  bool ParseComponentObjectStream(const std::string &stream_content,
                                  const maldoca::OLEDirectoryEntry &entry,
                                  maldoca::ole::OleFile *ole_proto);
  void ParsePidHlinks(maldoca::ole::OleFile *ole_proto);
  bool ParseOleNativeEmbedded(const std::string &stream_content,
                              const maldoca::OLEDirectoryEntry &entry,
                              maldoca::ole::OleFile *ole_proto);
};

StatusOr<maldoca::ole::OleFile> GetOleFileProto(
    absl::string_view in_buf, const maldoca::ole::OleToProtoSettings &settings);

StatusOr<maldoca::office::ParserOutput> GetParserOutputProto(
    absl::string_view in_buf, const maldoca::ole::OleToProtoSettings &settings);

}  // namespace maldoca

#endif  // MALDOCA_OLE_OLE_TO_PROTO_H_

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

#ifndef MALDOCA_OLE_OOXML_TO_PROTO_H_
#define MALDOCA_OLE_OOXML_TO_PROTO_H_

#include "absl/strings/string_view.h"
#include "maldoca/base/statusor.h"
#include "maldoca/ole/proto/office.pb.h"
#include "maldoca/ole/proto/ooxml.pb.h"
#include "maldoca/ole/proto/ooxml_to_proto_settings.pb.h"

namespace maldoca {
// Extracts content of OOXML archives, and returns it as a ooxml::OOXMLFile
// protobuf message. If the extraction fails, it returns the error
// message instead.
StatusOr<maldoca::ooxml::OOXMLFile> GetOOXMLFileProto(
    absl::string_view in_buf,
    const maldoca::ooxml::OoxmlToProtoSettings &settings);

StatusOr<office::ParserOutput> GetOoxmlParserOutputProto(
    absl::string_view in_buf,
    const maldoca::ooxml::OoxmlToProtoSettings &settings);
}  // namespace maldoca

#endif  // MALDOCA_OLE_OOXML_TO_PROTO_H_

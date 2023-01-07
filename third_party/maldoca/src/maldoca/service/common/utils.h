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

// Put all files related functions here.

#ifndef MALDOCA_SERVICE_COMMON_UTILS_H_
#define MALDOCA_SERVICE_COMMON_UTILS_H_
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/status.h"
#include "maldoca/ole/proto/extract_vba_settings.pb.h"
#include "maldoca/ole/proto/office.pb.h"
#include "maldoca/ole/proto/ole_to_proto_settings.pb.h"
#include "maldoca/ole/proto/ooxml_to_proto_settings.pb.h"
#ifndef MALDOCA_CHROME
#include "maldoca/service/common/file_type_identifier.h"
#endif  // MALDOCA_CHROME
#include "maldoca/service/proto/doc_type.pb.h"
#include "maldoca/service/proto/document_features.pb.h"
#include "maldoca/service/proto/maldoca_service.pb.h"
#include "maldoca/service/proto/parsed_document.pb.h"

namespace maldoca {
namespace utils {

using ::absl::StatusOr;

// Comvert specific doc type (e.g. DOCX) to general type
// (e.g.GENERAL_DOC_TYPE_DOC).
GeneralDocType DocTypeToGeneralType(DocType t);

#ifndef MALDOCA_CHROME
// Return an identifier by loading the magic database for identify document
// types (pdf, ooxml and cdf type).
StatusOr<std::unique_ptr<FileTypeIdentifier>> FileTypeIdentifierForDocType();

// Infer the doc type based on name and content. Caller needs to provide a
// FileTypeIdentifier* in identifier, e.g., using FileTypeIdentifierForDocType()
// or we ony infer is by file_name.
// The identifier is exptected to produce MIME string as output. We trust the
// content based inference over name based.
DocType InferDocType(absl::string_view file_name, absl::string_view doc,
                     ::maldoca::FileTypeIdentifier* identifier);
// Infer the doc type based on content. Caller needs to proved a
// FileTypeIdentifier* in identifier, e.g. from FileTypeIdentifierForDocType().
// The identifier is exptected to produce MIME string as output.
DocType InferDocTypeByContent(absl::string_view doc,
                              ::maldoca::FileTypeIdentifier* identifier);
#endif  // MALDOCA_CHROME

// Infer the doc type by file name.
DocType InferDocTypeByName(absl::string_view file_name);

// Simple topological sort of a dependency graph. Here each entry in out_edges
// represents a node with ID given by its vector index and has a vector of
// dependencies represetd as ID (index) into this vector nodes. E.g.,
// {{2, 3}, {0}, {}, {}, {}} means a dependencies graph like this where a number
// is a node and arrow is a dependency.
//          -> 2
//   1 -> 0             4
//          -> 3
// Returns error if cycle is detected, otherwise, returns a topological ordered
// list of <index, connected component>,
// e.g.,[<2, 0>, <3, 0>, <0, 0>, <1, 0>, <4, 1>] such
// that the nodes are in topological sorted order in each component. One can use
// this to sort a list of dependent jobs into parallel queues.
StatusOr<std::vector<std::pair<int32_t, int32_t>>> SortDependencies(
    const std::vector<std::vector<int32_t>>& out_edges);

// Get the default OletoProtoSettings value
const ::maldoca::ole::OleToProtoSettings& GetDefaultOleToProtoSettings();

// Get the default OoxmltoProtoSettings value
const maldoca::ooxml::OoxmlToProtoSettings& GetDefaultOoxmlToProtoSettings();

// Get the default ExtractVBASettings value
const maldoca::vba::ExtractVBASettings& GetDefaultExtractVBASettings();

::maldoca::StatusProto TranslateStatusToProto(const absl::Status& status);

absl::Status TranslateStatusFromProto(const ::maldoca::StatusProto& proto);

bool ParsedDocumentHasVbaScript(const ParsedDocument& parsed_document);
}  // namespace utils
}  // namespace maldoca

#endif  // MALDOCA_SERVICE_COMMON_UTILS_H_

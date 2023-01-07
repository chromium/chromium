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

#include "maldoca/service/common/utils.h"

#include <cstdint>
#include <queue>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_split.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/status_macros.h"
#include "maldoca/base/utf8/unicodetext.h"
#include "maldoca/ole/proto/ooxml_to_proto_settings.pb.h"
#include "maldoca/ole/proto/vba_extraction.pb.h"
#ifndef MALDOCA_CHROME
#include "maldoca/service/common/file_type_identifier.h"
#include "maldoca/service/magic_db_docs_embed.h"
#endif  // MALDOCA_CHROME
#include "maldoca/service/proto/doc_type.pb.h"
#include "re2/re2.h"

using ::RE2;
using ::maldoca::StatusProto;

namespace maldoca {
namespace utils {
namespace {

#ifndef MALDOCA_CHROME
static LazyRE2 kExtractSubtype = {"^application/([a-zA-Z0-9.-]+)"};
#endif  // MALDOCA_CHROME

// Using the list of nodes' out-edges and in-edges, perform a DFS to mark the
// connected components a node belongs to.  node is the current node and
// the component is the current component the node belongs to.
void MarkConnectedComponent(int node, int component,
                            const std::vector<std::vector<int32_t>>& out_edges,
                            const std::vector<std::vector<int32_t>>& in_edges,
                            std::vector<int32_t>* components) {
  if ((*components)[node] != -1) return;
  (*components)[node] = component;
  for (int32_t nbr : out_edges[node]) {
    MarkConnectedComponent(nbr, component, out_edges, in_edges, components);
  }
  for (int32_t nbr : in_edges[node]) {
    MarkConnectedComponent(nbr, component, out_edges, in_edges, components);
  }
}

}  // namespace

GeneralDocType DocTypeToGeneralType(DocType t) {
  switch (t) {
    case DocType::DOC:
    case DocType::DOCX:
    case DocType::DOCM:
      return GeneralDocType::GENERAL_DOC_TYPE_DOC;

    case DocType::XLA:
    case DocType::XLSB:
    case DocType::XLSM:
    case DocType::XLSX:
    case DocType::XLS:
      return GeneralDocType::GENERAL_DOC_TYPE_XLS;

    case DocType::PPT:
    case DocType::PPS:
    case DocType::PPSX:
    case DocType::PPTX:
      return GeneralDocType::GENERAL_DOC_TYPE_PPT;

    case DocType::PDF:
      return GeneralDocType::GENERAL_DOC_TYPE_PDF;
    case DocType::VB:
    case DocType::VBE:
    case DocType::VBS:
      return GeneralDocType::GENERAL_DOC_TYPE_VBA;

    default:
      return GeneralDocType::GENERAL_DOC_TYPE_UNKNOWN;
  }
}

#ifndef MALDOCA_CHROME
StatusOr<std::unique_ptr<FileTypeIdentifier>> FileTypeIdentifierForDocType() {
  std::vector<absl::string_view> db_buffers =
      FileTypeIdentifier::DefaultDbBuffers();

  for (auto* p = magic_db::magic_db_docs_embed_create(); p->name != nullptr;
       ++p) {
    db_buffers.push_back({p->data, p->size});
  }

  return FileTypeIdentifier::CreateFromDbBuffers(
      MAGIC_CONTINUE | MAGIC_ERROR | MAGIC_SYMLINK | MAGIC_MIME_TYPE,
      db_buffers);
}

DocType InferDocType(absl::string_view file_name, absl::string_view doc,
                     ::maldoca::FileTypeIdentifier* identifier) {
  DocType content_res = identifier == nullptr
                            ? DocType::UNKNOWN_TYPE
                            : InferDocTypeByContent(doc, identifier);
  auto file_res = InferDocTypeByName(file_name);

  // First, if no content_res then use file_res. Secondly, since MIME typs for
  // xla and xls is the same, so see if file_res is more specific
  if (content_res == DocType::UNKNOWN_TYPE ||
      (content_res == DocType::XLS && file_res == DocType::XLA)) {
    return file_res;
  }
  return content_res;
}

DocType InferDocTypeByContent(absl::string_view doc,
                              ::maldoca::FileTypeIdentifier* identifier) {
  CHECK(identifier != nullptr);
  static const auto* kMimeMap = new absl::flat_hash_map<std::string, DocType>{
      {"msword", DocType::DOC},
      {"vnd.openxmlformats-officedocument.wordprocessingml.document",
       DocType::DOCX},
      {"vnd.ms-word.document.macroEnabled.12", DocType::DOCM},
      {"vnd.ms-excel.sheet.binary.macroEnabled.12", DocType::XLSB},
      {"vnd.ms-excel.sheet.macroEnabled.12", DocType::XLSM},
      {"vnd.openxmlformats-officedocument.spreadsheetml.sheet", DocType::XLSX},
      {"vnd.ms-excel", DocType::XLS},
      {"vnd.ms-powerpoint", DocType::PPT},
      {"vnd.openxmlformats-officedocument.presentationml.slideshow",
       DocType::PPSX},
      {"vnd.openxmlformats-officedocument.presentationml.presentation",
       DocType::PPTX},
      {"pdf", DocType::PDF}};
  auto file_type = identifier->IdentifyBuffer(doc);
  if (file_type.ok()) {
    std::string subtype;
    if (RE2::Extract(*file_type, *kExtractSubtype, "\\1", &subtype)) {
      DLOG(INFO) << "type: " << *file_type << ", subtype: " << subtype;
      auto iter = kMimeMap->find(subtype);
      if (iter != kMimeMap->end()) {
        return iter->second;
      }
    }
  }
  return DocType::UNKNOWN_TYPE;
}
#endif  // MALDOCA_CHROME

DocType InferDocTypeByName(absl::string_view file_name) {
  static const auto* kExtMap = new absl::flat_hash_map<std::string, DocType>{
      {"doc", DocType::DOC},   {"docx", DocType::DOCX}, {"docm", DocType::DOCM},
      {"xla", DocType::XLA},   {"xlsb", DocType::XLSB}, {"xlsm", DocType::XLSM},
      {"xlsx", DocType::XLSX}, {"xls", DocType::XLS},   {"ppt", DocType::PPT},
      {"pps", DocType::PPS},   {"ppsx", DocType::PPSX}, {"pptx", DocType::PPTX},
      {"pdf", DocType::PDF}};
  auto ext = file::SplitFilename(file_name).second;
  auto iter = kExtMap->find(absl::AsciiStrToLower(ext));
  if (iter != kExtMap->end()) {
    return iter->second;
  } else {
    return DocType::UNKNOWN_TYPE;
  }
}

// Use the Kahn algo. to sort.
StatusOr<std::vector<std::pair<int32_t, int32_t>>> SortDependencies(
    const std::vector<std::vector<int32_t>>& out_edges) {
  std::vector<std::pair<int32_t, int32_t>> sorted;
  sorted.reserve(out_edges.size());
  // Store the connected component for each node
  std::vector<int32_t> components(out_edges.size(), -1);
  // For the in-edge graph.
  std::vector<std::vector<int32_t>> in_edges(out_edges.size());
  std::vector<int32_t> out_degrees(out_edges.size(), 0);
  std::queue<int32_t> out_queue;  // the nodes w/o out degree can be removed
  for (int i = 0; i < out_edges.size(); ++i) {
    out_degrees[i] = out_edges[i].size();
    if (out_degrees[i] == 0) {
      out_queue.push(i);
    } else {
      for (int32_t dest : out_edges[i]) {
        in_edges[dest].push_back(i);
      }
    }
  }
  // Use DFS to find the connected component
  int current_component = 0;
  for (int i = 0; i < components.size(); ++i) {
    if (components[i] == -1) {
      // not touched, mark the components
      MarkConnectedComponent(i, current_component++, out_edges, in_edges,
                             &components);
    }
  }
  int count = 0;  // num of nodes visted
  while (!out_queue.empty()) {
    int32_t node = out_queue.front();
    out_queue.pop();
    sorted.push_back({node, components[node]});
    for (int32_t nbr : in_edges[node]) {
      if (--out_degrees[nbr] == 0) {
        out_queue.push(nbr);  // this node has no more out degree so graduated
      }
    }
    ++count;
  }
  if (count != out_edges.size()) {
    // cycle
    return absl::InvalidArgumentError("The graph is cyclic.");
  }
  return sorted;
}

// Right now we enable everything as default
const ole::OleToProtoSettings& GetDefaultOleToProtoSettings() {
  static const ole::OleToProtoSettings* proto = []() {
    auto setting = new ole::OleToProtoSettings();
    setting->set_include_summary_information(true);
    setting->set_include_vba_code(true);
    setting->set_include_directory_structure(true);
    setting->set_include_stream_hashes(true);
    setting->set_include_olenative_metadata(true);
    setting->set_include_olenative_content(true);
    setting->set_include_unknown_strings(true);
    setting->set_include_excel4_macros(true);
    return setting;
  }();
  return *proto;
}

// Right now we enable everything as default
const ooxml::OoxmlToProtoSettings& GetDefaultOoxmlToProtoSettings() {
  static const ooxml::OoxmlToProtoSettings* proto = []() {
    auto setting = new ooxml::OoxmlToProtoSettings();
    setting->set_include_metadata(true);
    setting->set_include_structure_information(true);
    setting->set_include_embedded_objects(true);
    setting->set_include_vba_code(true);
    return setting;
  }();
  return *proto;
}

const maldoca::vba::ExtractVBASettings& GetDefaultExtractVBASettings() {
  static const vba::ExtractVBASettings* proto = []() {
    auto setting = new vba::ExtractVBASettings();
    setting->set_extraction_method(vba::EXTRACTION_TYPE_DEFAULT);
    return setting;
  }();
  return *proto;
}

StatusProto TranslateStatusToProto(const absl::Status& status) {
  StatusProto proto;
  proto.set_code(static_cast<int32_t>(status.code()));
  auto message = status.message();
  proto.set_error_message(message.data(), message.size());
  // copy payload
  const auto payload = status.GetPayload(kMaldocaStatusType);
  if (payload.has_value()) {
    auto status_payload = proto.add_payloads();
    status_payload->set_type_url(kMaldocaStatusType);
    status_payload->set_payload(std::string(payload.value()));
  }
  return proto;
}

absl::Status TranslateStatusFromProto(const StatusProto& proto) {
  absl::Status status(static_cast<absl::StatusCode>(proto.code()),
                      proto.error_message());
  for (const auto& payload : proto.payloads()) {
    status.SetPayload(payload.type_url(), absl::Cord(payload.payload()));
  }
  return status;
}

bool ParsedDocumentHasVbaScript(const ParsedDocument& parsed_document) {
  if (!parsed_document.has_office()) {
    return false;
  }
  if (!parsed_document.office().has_parser_output()) {
    return false;
  }
  if (!parsed_document.office().parser_output().has_script_features()) {
    return false;
  }
  for (auto& script :
       parsed_document.office().parser_output().script_features().scripts()) {
    if (script.has_vba_code()) {
      return true;
    }
  }
  return false;
}
}  // namespace utils
}  // namespace maldoca

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

#include "maldoca/ole/ooxml_to_proto.h"

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "libxml/xpath.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/status.h"
#include "maldoca/base/status_macros.h"
#include "maldoca/ole/archive_handler.h"
#include "maldoca/ole/ole_to_proto.h"
#include "maldoca/ole/ooxml_properties_extract.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/proto/ooxml.pb.h"
#include "maldoca/ole/vba_extract.h"
#include "maldoca/service/common/utils.h"

namespace maldoca {

using ::maldoca::ooxml::OoxmlToProtoSettings;
using utils::XmlCharDeleter;
using utils::XmlDocDeleter;

namespace {
// Set containing the OOXML entries with properties we want to extract.
const std::vector<std::string>& PropertiesFiles() {
  static const std::vector<std::string>* data = []() {
    std::vector<std::string>* vec = new std::vector<std::string>();
    *vec = {"docProps/app.xml", "docProps/core.xml"};
    return vec;
  }();
  return *data;
}

// Class able to parse OOXML streams into a OOXMLFile proto.
// It parses the file in a way similar to this script:
// https://github.com/magjogui/snortdlp/blob/master/src/python/read_open_xml.pl
class OOXMLToProto final {
 public:
  explicit OOXMLToProto(const OoxmlToProtoSettings& settings);
  ~OOXMLToProto() = default;

  // Extracts the content of the `in_buf` stream into a `OOXMLFile` message.
  // If the extraction fails, the function returns the error status.
  StatusOr<ooxml::OOXMLFile> ParseOOXMLBuffer(absl::string_view in_buf);

 private:
  // Extracts and inserts into the proto the content of the `_rels/.rels`
  // file, and for each properties file found calls the `ExtractProperties`
  // function.
  // Returns an error status if the archive doesn't contain a valid `.rels`
  // file.
  absl::Status ExtractRelationships();

  // Extracts document properties from core.xml and app.xml.
  absl::Status ExtractProperties();

  // Extracts document properties from the file filename`.
  absl::Status ExtractPropertiesFromFile(absl::string_view filename,
                                         int* property_id_counter);

  // Saves an entry of the OOXML archive to the proto.
  // `filename` is the full path of the entry, and `content` the content.
  // WARNING: This function may destroy the `content` object.
  // Note: The file may be an OLE2 entry (in which case it will be parsed with
  // the OLE parser).
  absl::Status ExtractFile(absl::string_view filename, std::string& content);

  // Stores the content of the archive, in { entry_name, content } form.
  absl::flat_hash_map<std::string, std::string> archive_content_;
  // Stores the OOXMLFile proto, which is returned by `ParseOOXMLBuffer`.
  ooxml::OOXMLFile ooxml_proto_;
  // Settings used for parsing OOXML files.
  const ooxml::OoxmlToProtoSettings& ooxml_to_proto_settings_;
  // Settings used for parsing OLE2 files.
  ole::OleToProtoSettings ole_to_proto_settings_;
};

OOXMLToProto::OOXMLToProto(const OoxmlToProtoSettings& settings)
    : ooxml_to_proto_settings_(settings) {
  // Only enable OLE features requested by the OOXML config.
  ole_to_proto_settings_.set_include_summary_information(
      settings.include_metadata());
  ole_to_proto_settings_.set_include_vba_code(settings.include_vba_code());
  ole_to_proto_settings_.set_include_directory_structure(
      settings.include_structure_information());
  ole_to_proto_settings_.set_include_stream_hashes(false);
  ole_to_proto_settings_.set_include_olenative_metadata(
      settings.include_embedded_objects());
  ole_to_proto_settings_.set_include_olenative_content(
      settings.include_embedded_objects());
  ole_to_proto_settings_.set_include_unknown_strings(false);
  ole_to_proto_settings_.set_include_excel4_macros(false);
}

StatusOr<ooxml::OOXMLFile> OOXMLToProto::ParseOOXMLBuffer(
    absl::string_view in_buf) {
  // Clear data from former parsing.
  archive_content_.clear();
  ooxml_proto_.Clear();

  // Initiate archive handler.
  auto archive_or = ::maldoca::utils::GetArchiveHandler(
      in_buf, "zip", "" /*dummy location since zip uses in-memory libarchive*/,
      false, false);

  // Verify the archive is not corrupted.
  if (!archive_or.ok() || !archive_or.value()->Initialized()) {
    return ::maldoca::InternalError("Unable to open archive!",
                                    MaldocaErrorCode::ARCHIVE_CORRUPTED);
  }

  auto archive = archive_or.value().get();
  std::string filename, content;
  int64_t size;

  // Read archive and store files in the `archive_content_` map.
  while (archive->GetNextGoodContent(&filename, &size, &content)) {
    // Size is wrong. Zip file may be corrupt but we still try to parse it.
    if (size != static_cast<int64_t>(content.size())) {
      DLOG(INFO) << "File " + filename + " has size of " << content.size()
                 << " but found as " << size << "!\n";
    }
    archive_content_[std::move(filename)] = std::move(content);
  }

  // Now the entire archive is extracted into `archive_content_`.
  // We have to extract relationships and properties, then save
  // all entries to the proto and return it.
  auto status = ExtractRelationships();
  if (!status.ok()) {
    DLOG(ERROR) << "Error while parsing relationships: " << status.message();
  }
  status = ExtractProperties();
  if (!status.ok()) {
    DLOG(ERROR) << "Error while parsing properties: " << status.message();
  }

  // Save file entries to the proto.
  for (auto& filename_content : archive_content_) {
    MALDOCA_RETURN_IF_ERROR(
        ExtractFile(filename_content.first, filename_content.second));
  }

  return std::move(ooxml_proto_);
}

absl::Status OOXMLToProto::ExtractRelationships() {
  if (!ooxml_to_proto_settings_.include_metadata()) {
    return absl::OkStatus();
  }

  for (const auto& filename_content : archive_content_) {
    const auto& filename = filename_content.first;
    const auto& content = filename_content.second;
    if (!absl::EndsWith(filename, ".rels")) {
      continue;
    }

    // XML container created with the libXML library.
    // Note that the `maldoca::utils::XmlParseMemory` function initializes the
    // parser the first time it's called. This allows `libXML` to be used in
    // multithreading.
    std::unique_ptr<xmlDoc, utils::XmlDocDeleter> doc(
        utils::XmlParseMemory(content.c_str(), content.size()));

    // Doc is nullptr if the file isn't a valid XML file.
    if (!doc) {
      DLOG(ERROR) << "Unable to open \'" << filename << "\'";
      continue;
    }

    xmlNodePtr document_root = xmlDocGetRootElement(doc.get());
    if (document_root == nullptr) {
      DLOG(ERROR) << "Unable to get root of document: " << filename;
      continue;
    }

    xmlNodePtr relationship = document_root->children;

    for (; relationship != nullptr; relationship = relationship->next) {
      auto& rel_proto = *(ooxml_proto_.add_relationships());
      xmlAttr* attribute = relationship->properties;

      // Process all the attributes.
      for (; attribute != nullptr; attribute = attribute->next) {
        absl::string_view name = utils::XmlCharPointerToString(attribute->name);
        std::unique_ptr<xmlChar, utils::XmlCharDeleter> prop(
            xmlGetProp(relationship, attribute->name));
        absl::string_view value = utils::XmlCharPointerToString(prop.get());

        if (name == "Id") {
          rel_proto.set_id(value.data(), value.size());
        } else if (name == "Type") {
          rel_proto.set_type(value.data(), value.size());
        } else if (name == "Target") {
          rel_proto.set_target(value.data(), value.size());
        } else {
          DLOG(ERROR) << "Unexpected attribute in \'_rels/.rels\': " << name
                      << " = " << value;
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::Status OOXMLToProto::ExtractProperties() {
  if (!ooxml_to_proto_settings_.include_metadata()) {
    return absl::OkStatus();
  }

  ooxml_proto_.mutable_summary_information()
      ->add_property_set()
      ->add_dictionary();
  ooxml_proto_.mutable_document_summary_information()
      ->add_property_set()
      ->add_dictionary();

  int property_id_counter = 0;
  for (auto& filename : PropertiesFiles()) {
    auto status = ExtractPropertiesFromFile(filename, &property_id_counter);
    if (!status.ok()) {
      DLOG(ERROR) << "Error when parsing " << filename
                  << " for properties: " << status.message();
    }
  }
  return absl::OkStatus();
}

// This function extracts properties from the file `filename`.
// `property_id_counter` is used for assigning unique ids to the parsed
// properties.
absl::Status OOXMLToProto::ExtractPropertiesFromFile(absl::string_view filename,
                                                     int* property_id_counter) {
  auto it = archive_content_.find(filename);
  if (it == archive_content_.end()) {
    return ::maldoca::InternalError(
        "Unable to find the file inside the archive!",
        MaldocaErrorCode::MISSING_FILE_IN_ARCHIVE);
  }

  // Initiate the XML document parser. `doc` is the root of the document tree
  // structure.
  absl::string_view content = it->second;
  std::unique_ptr<xmlDoc, utils::XmlDocDeleter> doc(
      utils::XmlParseMemory(content.data(), content.size()));

  // `doc` is nullptr iff the file isn't a valid XML file.
  if (!doc) {
    return ::maldoca::InternalError("The file is not a valid XML document!",
                                    MaldocaErrorCode::INVALID_XML_DOC);
  }

  xmlNodePtr document_root = xmlDocGetRootElement(doc.get());
  if (document_root == nullptr) {
    return ::maldoca::InternalError("Unable to get root of the document!",
                                    MaldocaErrorCode::INVALID_ROOT_DIR);
  }

  absl::string_view root_node_name =
      utils::XmlCharPointerToString(document_root->name);

  if (!absl::StrContains(root_node_name, "Properties")) {
    return ::maldoca::AbortedError("Unable to get root of the document!",
                                   MaldocaErrorCode::MISSING_PROPERTIES);
  }

  // Beginning of the linked list with all the properties.
  xmlNodePtr property = document_root->children;

  for (; property != nullptr; property = property->next) {
    auto status = ExtractOOXMLPropertyToProto(property, &ooxml_proto_,
                                              property_id_counter);
    if (!status.ok()) {
      DLOG(INFO) << "Error while extracting a property: " << status.message();
    }
  }

  return absl::OkStatus();
}

absl::Status OOXMLToProto::ExtractFile(absl::string_view filename,
                                       std::string& content) {
  if ((ooxml_to_proto_settings_.include_metadata() ||
       ooxml_to_proto_settings_.include_structure_information() ||
       ooxml_to_proto_settings_.include_vba_code()) &&
      IsOLE2Content(content)) {
    // It's an OLE2 entry, parse it with the ole_to_proto parser
    // and save it inside an ole_entry.
    auto status_or_proto =
        maldoca::GetOleFileProto(content, ole_to_proto_settings_);
    if (!status_or_proto.ok()) {
      return status_or_proto.status();
    }
    auto ole_entry = ooxml_proto_.add_ole_entries();
    ole_entry->set_filename(filename.data(), filename.size());
    ole_entry->set_filesize(content.size());
    ole_entry->set_hash(Sha256HexString(content));
    *(ole_entry->mutable_ole_content()) = status_or_proto.value();
    return absl::OkStatus();
  }
  if (ooxml_to_proto_settings_.include_structure_information()) {
    // Create new entry in ooxml.zipentry.
    auto entry = ooxml_proto_.add_entries();
    entry->set_file_name(filename.data(), filename.size());
    entry->set_hash(Sha256HexString(content));
    entry->set_content(std::move(content));
    entry->set_file_size(filename.size());
  }
  return absl::OkStatus();
}

}  // namespace

StatusOr<ooxml::OOXMLFile> GetOOXMLFileProto(
    absl::string_view in_buf, const OoxmlToProtoSettings& settings) {
  OOXMLToProto ooxml_to_proto(settings);

  auto status_or_proto = ooxml_to_proto.ParseOOXMLBuffer(in_buf);

  if (!status_or_proto.ok()) {
    return status_or_proto.status();
  }

  return status_or_proto.value();
}

StatusOr<office::ParserOutput> GetOoxmlParserOutputProto(
    absl::string_view in_buf, const OoxmlToProtoSettings& settings) {
  StatusOr<ooxml::OOXMLFile> status_or = GetOOXMLFileProto(in_buf, settings);
  if (!status_or.ok()) {
    return status_or.status();
  }
  ooxml::OOXMLFile ooxml_file = status_or.value();
  office::ParserOutput parser_output;

  // Metadata
  *parser_output.mutable_metadata_features()->mutable_summary_information() =
      std::move(*ooxml_file.mutable_summary_information());
  *parser_output.mutable_metadata_features()
       ->mutable_document_summary_information() =
      std::move(*ooxml_file.mutable_document_summary_information());
  *parser_output.mutable_metadata_features()
       ->mutable_ooxml_metadata_features()
       ->mutable_relationships() =
      std::move(*ooxml_file.mutable_relationships());

  // Structure
  auto* ooxml = parser_output.mutable_structure_features()->mutable_ooxml();
  for (const auto& entry : ooxml_file.entries()) {
    auto* output_entry = ooxml->add_entries();
    *output_entry->mutable_content() = entry.content();
    *output_entry->mutable_sha256() = entry.hash();
    *output_entry->mutable_file_name() = entry.file_name();
    output_entry->set_file_size(entry.file_size());
  }
  for (int i = 0; i < ooxml_file.ole_entries_size(); ++i) {
    auto* entry = ooxml_file.mutable_ole_entries(i);
    auto* output_entry = ooxml->add_entries();
    // Copy content so that the VBA code is still present and can be added to
    // the script features.
    *output_entry->mutable_ole_content() = entry->ole_content();
    *output_entry->mutable_sha256() = entry->hash();
    *output_entry->mutable_file_name() = entry->filename();
    output_entry->set_file_size(entry->filesize());

    // Embedded files
    auto* embedded = parser_output.mutable_embedded_file_features()
                         ->add_ole_native_embedded();
    *embedded =
        std::move(*entry->mutable_ole_content()->mutable_olenative_embedded());

    // Scripts
    auto* vba_code = entry->mutable_ole_content()->mutable_vba_code();
    // Add the new code chunks to the existing ones after having prepended the
    // new code chunk paths with the name of the container file.
    for (int j = 0; j < vba_code->chunk_size(); ++j) {
      vba_code::InsertPathPrefix(output_entry->file_name(),
                                 vba_code->mutable_chunk(j));
    }
    auto* script = parser_output.mutable_script_features()->add_scripts();
    script->mutable_vba_code()->MergeFrom(*vba_code);
    *script->mutable_filename() = entry->filename();
  }

  return parser_output;
}
}  // namespace maldoca

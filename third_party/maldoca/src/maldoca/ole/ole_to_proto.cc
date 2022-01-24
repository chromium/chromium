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

#include "maldoca/ole/ole_to_proto.h"

#include "absl/status/status.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/status.h"

#ifndef MALDOCA_CHROME
#include "maldoca/ole/biff_obj.h"
#endif  // MALDOCA_CHROME

#include "maldoca/ole/data_structures.h"
#include "maldoca/ole/endian_reader.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/ole_helper.h"
#include "maldoca/ole/property_set_stream.h"

#ifndef MALDOCA_CHROME
#include "maldoca/ole/proto/excel4_extraction.proto.h"
#endif  // MALDOCA_CHROME

#include "maldoca/ole/stream.h"
#include "maldoca/ole/strings_extract.h"
#include "maldoca/ole/vba.h"

// one space at the beginning accounts for the non printable character, which
// is converted to " " by the OLE parser
constexpr char kStreamSummaryInformation[] = " SummaryInformation";
constexpr char kStreamDocumentSummaryInformation[] =
    " DocumentSummaryInformation";
constexpr char kStreamComponentObject[] = " CompObj";
constexpr char kStreamHwpSummaryInformation[] = " HwpSummaryInformation";
constexpr char kStreamVba[] = "VBA";
constexpr char kStreamVbaDir[] = "dir";
constexpr char kStreamVbaProject[] = "project";
constexpr char kStreamOleNative[] = " Ole10Native";
constexpr char kStreamWorkbook[] = "Workbook";
constexpr char kStreamBook[] = "Book";

constexpr int32_t kMaxExtractedStrings = 1024;
constexpr int32_t kExtractedStringMinLen = 6;
constexpr int32_t kExtractedStringMaxLen = 1024;
constexpr int32_t kOleRootDirIndex = -1;

using ::maldoca::ole::OleDirectoryEntry;
using ::maldoca::ole::OleFile;
using ::maldoca::ole::OleToProtoSettings;
using ::google::protobuf::RepeatedPtrField;

namespace maldoca {
OleToProto::OleToProto() {}

OleToProto::OleToProto(const OleToProtoSettings &settings)
    : settings_(settings) {}

void OleToProto::PrepareForVBA(absl::string_view in_buf) {
  vba_root_ = root_.FindVBAContentRoot();
  if (vba_root_ == nullptr) {
    return;
  }
  OLEDirectoryEntry *project_dir = vba_root_->FindChildByName(
      kStreamVbaProject, DirectoryStorageType::Stream);
  if (project_dir == nullptr) {
    return;
  }
  std::string project_content;
  if (!OLEStream::ReadDirectoryContent(in_buf, header_, *project_dir, fat_,
                                       &project_content)) {
    return;
  }

  if (project_content.empty()) {
    DLOG(WARNING) << "Stream content for " << project_dir->Path() << " empty";
  } else {
    if (!vba_code::ParseCodeModules(project_content, &code_modules_)) {
      DLOG(WARNING) << "Can not parse code modules content from "
                    << project_dir->Path();
    }
  }
}

bool OleToProto::ParseSummaryInformationStream(
    const std::string &stream_content, const OLEDirectoryEntry &entry,
    OleFile *ole_proto) {
  if (!settings_.include_summary_information() ||
      ((entry.Name() != kStreamSummaryInformation) &&
       (entry.Name() != kStreamHwpSummaryInformation))) {
    return false;
  }
  DLOG(INFO) << kStreamSummaryInformation;
  maldoca::OLEPropertySetStream property_set_stream;
  if (!property_set_stream.Read(stream_content,
                                ole_proto->mutable_summary_information())) {
    ole_proto->clear_summary_information();
    return false;
  }
  return true;
}

bool OleToProto::ParseComponentObjectStream(
    const std::string &stream_content, const maldoca::OLEDirectoryEntry &entry,
    OleFile *ole_proto) {
  if (!settings_.include_summary_information() ||
      (entry.Name() != kStreamComponentObject)) {
    return false;
  }
  maldoca::ole::data_structure::CompObj comp_obj;
  auto status = comp_obj.Parse(stream_content);
  if (!status.ok()) {
    DLOG(WARNING) << "Can not parse CompObj content from stream: " << status;
    ole_proto->clear_comp_obj();
    return false;
  }
  auto co = ole_proto->mutable_comp_obj();
  co->set_version(comp_obj.Version());
  if (!comp_obj.UserType().empty()) {
    co->set_user_type(comp_obj.UserType());
  }
  if (!comp_obj.ClipboardFormat().empty()) {
    co->set_clipboard_format(comp_obj.ClipboardFormat());
  }
  if (!comp_obj.Reserved().empty()) {
    co->set_app_version(comp_obj.Reserved());
  }
  return true;
}

void OleToProto::ParsePidHlinks(OleFile *ole_proto) {
  DLOG(INFO) << "_Parse _PID_HLINKS";
  StatusOr<const ole::Property *> result =
      OLEPropertySetStream::GetPropertyFromDictionary(
          "_PID_HLINKS", *ole_proto->mutable_document_summary_information());
  if (!result.ok()) {
    return;
  }
  const ole::Property &property = *result.value();
  if (!property.has_type() || !property.has_value() ||
      (property.type() != ole::VariantType::kVtBlob) ||
      !property.value().has_blob()) {
    return;
  }

  absl::string_view blob = property.value().blob();
  maldoca::OLEPropertySetStream reader;
  reader.VecVtHyperlinkReader(
      &blob, ole_proto->mutable_reserved_properties()->mutable_pid_hlinks());
}

bool OleToProto::ParseDocumentSummaryInformationStream(
    const std::string &stream_content, const OLEDirectoryEntry &entry,
    OleFile *ole_proto) {
  if (!settings_.include_summary_information() ||
      (entry.Name() != kStreamDocumentSummaryInformation)) {
    return false;
  }
  DLOG(INFO) << kStreamDocumentSummaryInformation;
  maldoca::OLEPropertySetStream property_set_stream;
  if (!property_set_stream.Read(
          stream_content, ole_proto->mutable_document_summary_information())) {
    ole_proto->clear_document_summary_information();
    return false;
  }

  ParsePidHlinks(ole_proto);
  return true;
}

// Attempts to parse embedded packager objects from OleNative streams. It
// extracts metadata fields for the packager object. If content extraction is
// enabled it will also extract the embedded file.
bool OleToProto::ParseOleNativeEmbedded(const std::string &stream_content,
                                        const OLEDirectoryEntry &entry,
                                        OleFile *ole_proto) {
  if (!settings_.include_olenative_metadata() ||
      (entry.Name() != kStreamOleNative)) {
    return false;
  }
  maldoca::ole::data_structure::OleNativeEmbedded olenative_embedded;
  auto status = olenative_embedded.Parse(stream_content, entry.StreamSize());
  if (!status.ok()) {
    DLOG(WARNING) << "Can not parse OleNative embedded content from stream: "
                  << status;
    ole_proto->clear_olenative_embedded();
    return false;
  }
  auto oe = ole_proto->mutable_olenative_embedded();
  oe->set_native_size(olenative_embedded.NativeSize());
  oe->set_type(olenative_embedded.Type());
  oe->set_file_name(olenative_embedded.FileName());
  oe->set_file_path(olenative_embedded.FilePath());
  oe->set_reserved_unknown(olenative_embedded.Reserved());
  oe->set_temp_path(olenative_embedded.TempPath());
  oe->set_file_size(olenative_embedded.FileSize());
  oe->set_file_hash(olenative_embedded.FileHash());
  if (settings_.include_olenative_content()) {
    oe->set_file_content(olenative_embedded.FileContent());
    // returns true only if the whole content is included in the protobuf,
    // otherwise we are interested in string extracted from the binary blob,
    // thus return false
    return true;
  }
  return false;
}

void OleToProto::DumpStreams(absl::string_view in_buf,
                             const OLEDirectoryEntry *entry, int level,
                             int32_t parent_index, OleFile *ole_proto,
                             RepeatedPtrField<OleDirectoryEntry> *dirs) {
  if (entry == nullptr) {
    return;
  }
  DLOG(INFO) << std::string(level * 2, ' ') << "Directory: " << entry->Name()
             << ", Type: " << maldoca::EntryTypeToString(entry->EntryType())
             << ", Size: " << entry->StreamSize();
  OleDirectoryEntry *dir_entry = nullptr;
  if (settings_.include_directory_structure() && (dirs != nullptr)) {
    dir_entry = dirs->Add();
    if ((parent_index != kOleRootDirIndex) && (parent_index < dirs->size())) {
      dirs->Mutable(parent_index)->add_child_index(dirs->size() - 1);
    }
    dir_entry->set_name(entry->Name());
    dir_entry->set_entry_type(entry->EntryType());
    dir_entry->mutable_clsid()->set_data(entry->Clsid(), 16);
    dir_entry->set_user_flags(entry->UserFlags());
    dir_entry->set_creation_timestamp(entry->CreationTimestamp());
    dir_entry->set_modification_timestamp(entry->ModificationTimestamp());
    if (entry->EntryType() == DirectoryStorageType::Stream) {
      dir_entry->set_stream_size(entry->StreamSize());
    }
  }
  switch (entry->EntryType()) {
    case DirectoryStorageType::Stream: {
      std::string stream_content;
      OLEStream::ReadDirectoryContent(in_buf, header_, *entry, fat_,
                                      &stream_content);
      if (settings_.include_directory_structure() &&
          settings_.include_stream_hashes()) {
        dir_entry->set_stream_hash(Sha1Hash(stream_content));
      }
      bool stream_processed = false;
      if (level == 1) {
        // Include SummaryInformation and DocumentSummaryInformation stored only
        // in the first level of the OLE2 tree. OLE2 format allows embedding
        // multiple OLE Objects inside the OLE2 stream (ObjectPool, MBD streams)
        // and those streams can have their own SummaryInformation and
        // DocumentSummaryInformation streams. At the moment we are only
        // interested in the top-level information streams.
        stream_processed |=
            ParseSummaryInformationStream(stream_content, *entry, ole_proto);
        stream_processed |= ParseDocumentSummaryInformationStream(
            stream_content, *entry, ole_proto);
        stream_processed |=
            ParseComponentObjectStream(stream_content, *entry, ole_proto);
      }
#ifndef MALDOCA_CHROME
      if (entry->Name() == kStreamWorkbook || entry->Name() == kStreamBook) {
        maldoca::ole::BiffObj biff_obj;
        auto status = biff_obj.Parse(stream_content);
        if (status.ok() && (biff_obj.HasFormula() || biff_obj.IsEncrypted())) {
          if (settings_.include_directory_structure() &&
              biff_obj.HasFormula()) {
            dir_entry->set_xls_formula(true);
          }
          if (settings_.include_excel4_macros()) {
            auto macros = biff_obj.GetExcel4Ast();
            ole_proto->set_allocated_excel4_macros(macros.release());
          }
        }
      }
#endif  // MALDOCA_CHROME
      if (settings_.include_olenative_metadata() &&
          entry->Name() == kStreamOleNative) {
        // parsing for OleNative streams with embedded packager objects
        // other OleNative stream types are not parsed
        DLOG(INFO) << "OleNative embedded object extraction";
        stream_processed |=
            ParseOleNativeEmbedded(stream_content, *entry, ole_proto);
      }
      if (settings_.include_vba_code() && (entry->Name() == kStreamVbaDir) &&
          (entry->Parent() != nullptr) &&
          (entry->Parent()->Name() == kStreamVba)) {
        std::string expanded_vba_dir_content;
        if (stream_content.empty()) {
          LOG(WARNING) << "Empty VBA stream.";
          return;
        }
        if (!OLEStream::DecompressStream(stream_content,
                                         &expanded_vba_dir_content)) {
          LOG(ERROR) << "OLEStream::DecompressStream failed.";
          return;
        }
        DLOG(INFO) << "VBA extraction";
        if (in_buf.empty() || expanded_vba_dir_content.empty()) {
          LOG(WARNING) << "Empty VBA stream.";
          return;
        }
        // Extract code chunks from the decompressed VBA/dir content.
        if (vba_root_ == nullptr) {
          LOG(WARNING) << "vba_root_ is null, but VBA streams are present and "
                          "successfully decompressed.";
          return;
        }
        absl::flat_hash_set<uint32_t> extracted_indices;
        std::vector<OLEDirectoryEntry *> dir_entries;
        vba_code::ExtractVBAWithHash(
            in_buf, header_, code_modules_, *vba_root_, fat_,
            expanded_vba_dir_content, &extracted_indices, &dir_entries,
            ole_proto->mutable_vba_code(), /*continue_extraction=*/true);
      }
      // additional check for VBA streams, to exclude all streams that have
      // kStreamVba as a parent from string extraction.
      if (settings_.include_vba_code() && (entry->Parent() != nullptr) &&
          (entry->Parent()->Name() == kStreamVba)) {
        stream_processed = true;
      }
      if (settings_.include_unknown_strings() && !stream_processed) {
        // we want sorted container here for stability reason, otherwise in case
        // of reaching kMaxExtractedStrings limit, we will store random set of
        // strings in the final proto
        std::set<std::string> strings;
        GetStrings(stream_content, kExtractedStringMinLen, &strings);
        // added_strings is incremented by 1 for strings shorter than
        // kExtractedStringMaxLen (most of the extracted strings), and by
        // (len(s) / kExtractedStringMaxLen) + 1 for longer strings,
        // thus maximal size of added strings will never exceed
        // kMaxExtractedStrings * kExtractedStringMaxLen
        int32_t added_strings = 0;
        for (const auto &s : strings) {
          if (IsInterestingString(s)) {
            *dir_entry->mutable_strings()->Add() = s;
            added_strings += ((s.size() / kExtractedStringMaxLen) + 1);
            if (added_strings > kMaxExtractedStrings) {
              LOG(WARNING) << "Max strings for stream reached, early exit.";
              break;
            }
          }
        }
      }
    } break;

    case DirectoryStorageType::Storage:
    case DirectoryStorageType::Root: {
      int32_t parent_index = dirs ? dirs->size() - 1 : 0;
      for (uint32_t i = 0; i < entry->NumberOfChildren(); i++) {
        DumpStreams(in_buf, entry->ChildrenAt(i), level + 1, parent_index,
                    ole_proto,
                    settings_.include_directory_structure() ? dirs : nullptr);
      }
    } break;

    default:
      break;
  }
}

absl::Status OleToProto::ParseOleBuffer(absl::string_view in_buf,
                                        OleFile *ole_proto) {
  std::vector<OLEDirectoryEntry *> dir_entries;
  std::string directory_stream;
  auto status = ReadHeaderFatRoot(in_buf, &header_, &fat_, &root_, &dir_entries,
                                  &directory_stream);
  if (!status.ok()) {
    LOG(ERROR) << status.message();
    return status;
  }
  DLOG(INFO) << root_.ToString();
  if (settings_.include_vba_code()) {
    PrepareForVBA(in_buf);
  }
  DumpStreams(in_buf, &root_, 0, kOleRootDirIndex, ole_proto,
              settings_.include_directory_structure()
                  ? ole_proto->mutable_ole_dirs()
                  : nullptr);
  return absl::OkStatus();
}

StatusOr<OleFile> GetOleFileProto(absl::string_view in_buf,
                                  const OleToProtoSettings &settings) {
  OleToProto ole_to_proto(settings);
  OleFile ole_proto;
  auto status = ole_to_proto.ParseOleBuffer(in_buf, &ole_proto);
  if (status.ok()) {
    return ole_proto;
  }
  return status;
}

StatusOr<office::ParserOutput> GetParserOutputProto(
    absl::string_view in_buf, const OleToProtoSettings &settings) {
  StatusOr<OleFile> status_or = GetOleFileProto(in_buf, settings);
  if (!status_or.ok()) {
    return status_or.status();
  }
  OleFile ole_file = status_or.value();
  office::ParserOutput parser_output;

  // Metadata
  *parser_output.mutable_metadata_features()
       ->mutable_ole_metadata_features()
       ->mutable_reserved_properties() =
      std::move(*ole_file.mutable_reserved_properties());
  *parser_output.mutable_metadata_features()
       ->mutable_ole_metadata_features()
       ->mutable_comp_obj() = std::move(*ole_file.mutable_comp_obj());

  // Structure
  *parser_output.mutable_structure_features()
       ->mutable_ole()
       ->mutable_entries() = std::move(*ole_file.mutable_ole_dirs());

  // Embedded files
  *parser_output.mutable_embedded_file_features()->add_ole_native_embedded() =
      std::move(*ole_file.mutable_olenative_embedded());

  // Scripts
  auto *script_features = parser_output.mutable_script_features();
  *script_features->add_scripts()->mutable_vba_code() =
      std::move(*ole_file.mutable_vba_code());
#ifndef MALDOCA_CHROME
  *script_features->add_scripts()->mutable_excel4_macros() =
      std::move(*ole_file.mutable_excel4_macros());
#endif  // MALDOCA_CHROME

  return parser_output;
}

}  // namespace maldoca

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

// Implementation of the VBA code extraction API. See vba_extract.h for details.
//
// ExtractVBAFromFile will try to determine the nature of the file it
// has to process. The currently handled file formats are: Docfile,
// OOXML and Office 2003 XML.

#include "maldoca/ole/vba_extract.h"

#include <memory>

#include "absl/base/casts.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#ifndef MALDOCA_IN_CHROMIUM
#include "absl/flags/flag.h"  // nogncheck
#endif
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status_macros.h"
#include "maldoca/ole/archive_handler.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/header.h"
#include "maldoca/ole/mso.h"
#include "maldoca/ole/ole_helper.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/ppt.h"
#include "maldoca/ole/proto/extract_vba_settings.pb.h"
#include "maldoca/ole/proto/vba_extraction.pb.h"
#include "maldoca/ole/stream.h"

// Enumerate supported input types to limit the type of input
// ExtractVBAFromStringInternal can handle.
enum : uint32_t {
  TRY_OLE = 1 << 0,
  TRY_OOXML = 1 << 1,
  TRY_OFFICE2003 = 1 << 2,
  TRY_MSO = 1 << 3,
};

// All supported input type detection but OOXML will be tried.
static const uint32_t kSupportAllButOOXMLInputType =
    (TRY_OLE | TRY_OFFICE2003 | TRY_MSO);
// All supported input type detection will be tried.
static const uint32_t kSupportAllInputType =
    (kSupportAllButOOXMLInputType | TRY_OOXML);

#ifdef MALDOCA_IN_CHROMIUM
static const int32_t flag_input_type = -1;
static const bool extract_malformed_content = true;
#else
ABSL_FLAG(int32_t, input_type, -1,
          "This flag is a bitwise combination of TRY_* enum values and "
          "can be set to specify what type of input can be expected "
          "when attempting to extract VBA from a payload. When set to "
          "-1, all input types are considered expected. For instance, "
          "set to TRY_MSO | TRY_OLE = 0b1001 to have only OLE and MSO "
          "input considered at extraction time.");

ABSL_FLAG(bool, extract_malformed_content, true,
          "This flag specifies whether VBA code should be extracted from "
          "orphaned directory entries or otherwise malformed files.");
#endif
namespace maldoca {

// Forward-declare this function, as it is used before being declared.
static absl::Status ExtractFromPPT(absl::string_view content,
                                   const OLEHeader &header,
                                   OLEDirectoryEntry *ppt_root,
                                   const std::vector<uint32_t> &fat,
                                   VBACodeChunks *code_chunks);

// Do the smallest amount of work (in our context) to determine that
// content is most likely an Office 2003 XML file. Return true if
// it's the case.
static bool IsOffice2003Content(absl::string_view content) {
  return absl::StrContains(
      content, "http://schemas.microsoft.com/office/word/2003/wordml");
}

// Do the smallest amount of work (in our context) to determine that
// content is most likely an OLE2 file. Return true if it's the case.
bool IsOLE2Content(absl::string_view content) {
  OLEHeader header;
  return (OLEHeader::ParseHeader(content, &header) && header.IsInitialized());
}

// Helper function used when recursively copying some OLEDirectoryEntry
// content into an OLEDirectoryEntryMessage. The recursion depth is
// controlled.
static absl::Status OLEDirectoryEntryToOLEDirectoryMessage(
    const OLEDirectoryEntry &root, OLEDirectoryEntryMessage *dir,
    int recursion_depth) {
  if (recursion_depth - 1 <= 0) {
    return absl::InternalError("Recursion level exceeded during copy");
  }
  dir->set_name(root.Name());
  for (uint32_t index = 0; index < root.NumberOfChildren(); ++index) {
    const OLEDirectoryEntry *child = root.ChildrenAt(index);
    CHECK(child != nullptr);
    MALDOCA_RETURN_IF_ERROR(OLEDirectoryEntryToOLEDirectoryMessage(
        *child, dir->add_children(), recursion_depth - 1));
  }
  return absl::OkStatus();
}

// Attempt to extract the OLE directory and VBA code from content (under
// the assumption that we have already determined that it's some OLE2
// content.) Error message is propagated through the absl::Status return value.
// dir_entries is filled in when building the directory tree, starting at root.
// On return, directory_stream will be the stream located at the first directory
// sector location. Callers can use it to directory read directory entries (e.g.
// for brute-force searching for VBA code).
static absl::Status ExtractFromOLE2StringInternal(
    absl::string_view content, OLEHeader *header, std::vector<uint32_t> *fat,
    OLEDirectoryEntry *root, std::vector<OLEDirectoryEntry *> *dir_entries,
    absl::flat_hash_set<uint32_t> *extracted_indices,
    std::string *directory_stream,
    absl::node_hash_map<std::string, std::string> *code_modules,
    OLEDirectoryMessage *dir, VBACodeChunks *code_chunks) {
  // First read header, fat and root.
  MALDOCA_RETURN_IF_ERROR(ReadHeaderFatRoot(content, header, fat, root,
                                            dir_entries, directory_stream));

  // At this stage, if requested, we can fill an OLEDirectoryMessage instance.
  if (dir != nullptr) {
    MALDOCA_RETURN_IF_ERROR(
        OLEDirectoryEntryToOLEDirectoryMessage(*root, dir->add_entries(), 10));
  }

  // Check if the stream contains a PowerPoint 97 Document, in which case
  // a dedicated extraction method must be used.
  OLEDirectoryEntry *ppt_root = root->FindPowerPointDocumentRoot();
  if (ppt_root != nullptr) {
    DLOG(INFO) << "Found PowerPoint Document stream";
    MALDOCA_RETURN_IF_ERROR(
        ExtractFromPPT(content, *header, ppt_root, *fat, code_chunks));

    // Do not return immediately, opportunistically keep parsing the document to
    // find extra VBA content.
    // TODO(b/113256605): Store the fact that there is VBA outside of the PPT
    // stream, if any.
  }

  // Find the root of the VBA Content.
  OLEDirectoryEntry *vba_root = root->FindVBAContentRoot();
  if (vba_root == nullptr) {
    return ppt_root != nullptr
               ? absl::OkStatus()  // VBA content was found in the PPT file
               : absl::InternalError(
                     "Can not find a root directory for the VBA content");
  }

  // Find the project stream in the VBA root, read its content and
  // parse it as code modules information.
  OLEDirectoryEntry *project_dir =
      vba_root->FindChildByName("project", DirectoryStorageType::Stream);
  if (project_dir == nullptr) {
    LOG(WARNING) << "Can not find a 'project' stream in the VBA root '"
                 << vba_root->Path() << "'";
  } else {
    std::string project_content;
    if (!OLEStream::ReadDirectoryContent(content, *header, *project_dir, *fat,
                                         &project_content)) {
      return absl::InternalError(absl::StrCat(
          "Can not read stream content for '", project_dir->Path(), "'"));
    }

    if (project_content.empty()) {
      DLOG(WARNING) << "Stream content for " << project_dir->Path() << " empty";
    } else {
      if (!vba_code::ParseCodeModules(project_content, code_modules)) {
        DLOG(WARNING) << "Can not parse code modules content from "
                      << project_dir->Path();
      }
    }
  }

  // Find the VBA/dir entry from the VBA root, read its content and
  // decompress it.
  OLEDirectoryEntry *vba_dir =
      vba_root->FindChildByName("vba", DirectoryStorageType::Storage)
          ->FindChildByName("dir", DirectoryStorageType::Stream);
  if (vba_dir == nullptr) {
    // This shouldn't happen since FindVBAContentRoot() returning a
    // non null result guarantees that VBA/dir exists in the VBA root.
    return absl::InternalError(absl::StrCat(
        "Can not find 'VBA/dir' entry from '", vba_root->Path(), "'"));
  }
  std::string compressed_vba_dir_content;
  if (!OLEStream::ReadDirectoryContent(content, *header, *vba_dir, *fat,
                                       &compressed_vba_dir_content)) {
    return absl::InternalError(absl::StrCat("Can not read stream content for '",
                                            vba_dir->Path(), "'"));
  }
  if (compressed_vba_dir_content.empty()) {
    return absl::InternalError(
        absl::StrCat("Can not read '", vba_dir->Path(), "' stream"));
  }
  std::string expanded_vba_dir_content;
  if (!OLEStream::DecompressStream(compressed_vba_dir_content,
                                   &expanded_vba_dir_content)) {
    return absl::InternalError(absl::StrCat(
        "Can not decompress stream content for '", vba_dir->Path(), "'"));
  }

  // Extract code chunks from the decompressed VBA/dir content.
  bool status = vba_code::ExtractVBA2(
      content, *header, *code_modules, *vba_root, *fat,
      expanded_vba_dir_content, extracted_indices, dir_entries, code_chunks,
      /*continue_extraction=*/false);
  if (!status) {
    if (code_chunks->chunk_size() == 0) {
      return absl::InternalError(absl::StrCat(
          "Can not extract VBA content from '", vba_dir->Path(), "'"));
    }
    DLOG(WARNING) << "Could not extract all VBA content from "
                  << vba_root->Path();
  }
  return absl::OkStatus();
}

static uint32_t TryExtractMalformedAndOrphans(
    absl::string_view input, const std::string &directory_stream,
    const OLEHeader &header,
    const absl::node_hash_map<std::string, std::string> &code_modules,
    const std::vector<uint32_t> &fat,
    const std::vector<OLEDirectoryEntry *> &dir_entries,
    const absl::flat_hash_set<uint32_t> &extracted_indices,
    const OLEDirectoryEntry &root, VBACodeChunks *code_chunks) {
  uint32_t number_vba_code_chunks = 0;
  // Check all non-root directory entries.
  for (int i = 1; i < dir_entries.size(); i++) {
    // No need to check entries with VBA code that has already been extracted.
    if (!extracted_indices.contains(i)) {
      number_vba_code_chunks += vba_code::ExtractMalformedOrOrphan(
          input, i, header, root, directory_stream, code_modules, fat, false,
          dir_entries[i], code_chunks);
    }
  }
  return number_vba_code_chunks;
}

// Attempt to extract the OLE directory and VBA code from content (under
// the assumption that we have already determined that it's some OLE2
// content.) Error message is propagated through the absl::Status return value.
absl::Status ExtractFromOLE2String(absl::string_view content,
                                   OLEDirectoryMessage *dir,
                                   VBACodeChunks *code_chunks) {
  OLEHeader header;
  std::vector<uint32_t> fat;
  std::vector<OLEDirectoryEntry *> dir_entries;
  absl::flat_hash_set<uint32_t> extracted_indices;
  OLEDirectoryEntry root;
  std::string directory_stream;
  absl::node_hash_map<std::string, std::string> code_modules;
  absl::Status status = ExtractFromOLE2StringInternal(
      content, &header, &fat, &root, &dir_entries, &extracted_indices,
      &directory_stream, &code_modules, dir, code_chunks);
  if (!extract_malformed_content) {
    return status;
  } else if (!status.ok()) {
    DLOG(INFO) << "ExtractDirectoryAndVBAFromString returned: " << status;
  }
  uint32_t number_vba_code_chunks = TryExtractMalformedAndOrphans(
      content, directory_stream, header, code_modules, fat, dir_entries,
      extracted_indices, root, code_chunks);
  if (number_vba_code_chunks != 0) {
    LOG(INFO) << "Found " << number_vba_code_chunks
              << " code chunks in orphaned or malformed directory entries.";
  }
  return status;
}

// Attempt to process the input as a straight OLE content, extracting
// VBA code from it. Returns FailedPrecondition in case of not OLE2 content, or
// absl::Status returned by ExtractFromOLE2String.
static absl::Status ExtractFromOLEContentInternal(
    const std::string &original_filename, absl::string_view content,
    OLEDirectoryMessage *directory, VBACodeChunks *code_chunks) {
  if (!IsOLE2Content(content)) {
    return absl::FailedPreconditionError("Not an OLE2 content");
  }

  DLOG(INFO) << "Evaluating " << original_filename << " (" << content.length()
             << " bytes) as a plain OLE2 file";
  return ExtractFromOLE2String(content, directory, code_chunks);
}

// Attempt to process the input as OOXML content, extracting VBA code
// from it. Returns:
// - FailedPrecondition in case of not ZIP archive
// - OkSuccess - in case of success
// - InternalError - in case of errors in any of the OLE2 files embedded in the
//                   ZIP (note, that it may successfully parse some of the OLE2
//                   containers, so InternalError is used here to propagate,
//                   error message for some files and not to signal the error)
//
// The content is opened as a ZIP file, if that works, we try to find
// OLE2 files in it and extract VBA code from them.
static absl::Status ExtractFromOOXMLContentInternal(
    const std::string &original_filename, absl::string_view content,
    OLEDirectoryMessage *directory, VBACodeChunks *code_chunks) {
  auto handler_or = ::maldoca::utils::GetArchiveHandler(
      content, "zip", "" /*dummy location since zip uses in-memory libarchive*/,
      false, false);

  if (!handler_or.ok() || !handler_or.value()->Initialized()) {
    return absl::FailedPreconditionError(
        "Cannot initialize zip ArchiveHandler");
  }
  auto handler = handler_or.value().get();

  DLOG(INFO) << "Evaluating " << original_filename << " (" << content.length()
             << " bytes) as a OOXML file";

  std::string archive_member;
  int64_t size;
  std::string error;
  std::string archive_content;
  while (
      handler->GetNextGoodContent(&archive_member, &size, &archive_content)) {
    if (!IsOLE2Content(absl::string_view(archive_content))) {
      archive_content.clear();
      continue;
    }
    // Attempt to extract VBA code from OLE2 archive members.
    VBACodeChunks file_code_chunks;
    OLEDirectoryMessage current_directory;
    auto status = ExtractFromOLE2String(archive_content, &current_directory,
                                        &file_code_chunks);
    // If no errors were found, add the new code chunks to the existing
    // ones after having prepended the new code chunk paths with the name
    // of the container file. The possibly extracted directory content is
    // added to the returned directory content. Note that even if there are
    // errors, there may be code chunks if the file is malformed or contains
    // orphans.
    for (int i = 0; i < file_code_chunks.chunk_size(); ++i) {
      vba_code::InsertPathPrefix(archive_member,
                                 file_code_chunks.mutable_chunk(i));
    }
    code_chunks->MergeFrom(file_code_chunks);
    if (directory != nullptr) {
      directory->MergeFrom(current_directory);
    }
    if (!status.ok()) {
      // Append the error to the error string, prefixed by the name of the
      // archive entry that triggered the error.
      if (!error.empty()) {
        error.append(", ");
      }
      error.append(absl::StrCat(archive_member, ": ", status.message()));
    }
    archive_content.clear();
  }
  if (!error.empty()) {
    return absl::InternalError(error);
  }
  return absl::OkStatus();
}

// Attempt to process the input as an Office 2003 XML file, extracting an
// OLE directory and VBA code from it.
// Returns:
// - FailedPrecondition in case of not Office2003 content
// - OkSuccess - in case of success
// - InternalError - in case of errors in the embedded OLE2 object
static absl::Status ExtractFromOffice2003ContentInternal(
    const std::string &original_filename, absl::string_view content,
    OLEDirectoryMessage *directory, VBACodeChunks *code_chunks) {
  if (!IsOffice2003Content(content)) {
    return absl::FailedPreconditionError("Not an Office 2003 content");
  }

  DLOG(INFO) << "Evaluating " << original_filename << " (" << content.length()
             << " bytes) as an Office 2003 XML file";
  std::string mso_filename, mso;
  // Attempt to find MSO content in the XML document.
  if (!MSOContent::GetBinDataFromXML(content, &mso_filename, &mso)) {
    return absl::InternalError("No MSO file in Office 2003 document");
  }
  // Attempt to find OLE2 data in the MSO content.
  std::string ole2;
  if (!(MSOContent::GetOLE2Data(mso, &ole2))) {
    return absl::InternalError("No OLE2 content in Office 2003 document");
  }
  // Attempt to extract VBA Code from the OLE2 content. If that
  // fails, prepend the error message with the name of the MSO file
  // we were processing.

  auto status = ExtractFromOLE2String(ole2, directory, code_chunks);
  if (!status.ok() && !mso_filename.empty()) {
    return absl::InternalError(
        absl::StrCat(mso_filename, ": ", status.message()));
  }
  return status;
}

// Attempt to process the input as an MSO file, extracting an OLE directory
// and VBA code from it. Returns FailedPrecondition in case of invalid MSO
// content, or absl::Status returned by ExtractFromOLE2String.
static absl::Status ExtractFromMSOFile(const std::string &original_filename,
                                       absl::string_view content,
                                       OLEDirectoryMessage *directory,
                                       VBACodeChunks *code_chunks) {
  std::string ole2_from_mso;
  if (!MSOContent::GetOLE2Data(content, &ole2_from_mso)) {
    return absl::FailedPreconditionError("Not an OLE2 file");
  }

  DLOG(INFO) << "Evaluating " << original_filename << " (" << content.length()
             << " bytes) as an MSO file with OLE2 content";
  return ExtractFromOLE2String(ole2_from_mso, directory, code_chunks);
}

absl::Status ExtractFromPPT(absl::string_view content, const OLEHeader &header,
                            OLEDirectoryEntry *ppt_root,
                            const std::vector<uint32_t> &fat,
                            VBACodeChunks *code_chunks) {
  // Extract the PPT document stream from the OLE tree
  std::string ppt_stream;
  if (!OLEStream::ReadDirectoryContent(content, header, *ppt_root, fat,
                                       &ppt_stream)) {
    return absl::InternalError(absl::StrCat(
        "Could not read PPT document stream for: ", ppt_root->Path()));
  }

  // Extract all the VBA projects in the PPT document
  MALDOCA_ASSIGN_OR_RETURN(std::vector<std::string> vba_projects,
                           PPT97ExtractVBAStorage(ppt_stream));

  // Extract the VBA code from the VBA projects
  std::string error;
  for (const auto &vba_project : vba_projects) {
    VBACodeChunks project_code_chunks;
    // The VBA project container is a OLE file.
    ExtractVBAFromString(vba_project, &project_code_chunks, &error);
    if (!error.empty()) {
      DLOG(ERROR) << "Error while processing VBA Project: " << error;
      continue;
    }
    code_chunks->MergeFrom(project_code_chunks);
  }
  return absl::OkStatus();
}

// Core implementation of extracting OLE directory and VBA code from OLE2,
// OOXML, Office 2003 XML or MSO input from content; depending on the value
// OR'ed in input_type. If content is related to a filename,
// original_filename is to be set to a non empty value.
static absl::Status ExtractFromStringInternal(
    uint32_t input_type, const std::string &original_filename,
    absl::string_view content, OLEDirectoryMessage *directory,
    VBACodeChunks *code_chunks) {
  // We prefer code chunk to be passed empty here. error as this stage
  // should have been already cleared by the caller due to its
  // transient nature.
  CHECK_EQ(code_chunks->chunk_size(), 0);

  // The content is either a plain OLE2 file or a container for OLE2
  // files. We process the former directly and the latter depending on
  // what we can guess about its nature.

  // The content might be a simple OLE2 file.
  if (input_type & TRY_OLE) {
    auto status = ExtractFromOLEContentInternal(original_filename, content,
                                                directory, code_chunks);
    if (!IsFailedPrecondition(status)) {
      return status;
    } else {
      DLOG(INFO) << "ExtractFromOLEContentInternal returned " << status;
    }
  }

  // The content might be a OOXML (zip) file containing OLE2 entries.
  if (input_type & TRY_OOXML) {
    auto status = ExtractFromOOXMLContentInternal(original_filename, content,
                                                  directory, code_chunks);
    if (!IsFailedPrecondition(status)) {
      return status;
    } else {
      DLOG(INFO) << "ExtractFromOOXMLContentInternal returned " << status;
    }
  }

  // The content might be an Office 2003 file featuring an MSO file
  // with OLE2 content.
  if (input_type & TRY_OFFICE2003) {
    auto status = ExtractFromOffice2003ContentInternal(
        original_filename, content, directory, code_chunks);
    if (!IsFailedPrecondition(status)) {
      return status;
    } else {
      DLOG(INFO) << "ExtractFromOffice2003ContentInternal returned " << status;
    }
  }

  // Let's try our luck at a straight MSO file.
  if (input_type & TRY_MSO) {
    auto status =
        ExtractFromMSOFile(original_filename, content, directory, code_chunks);
    if (!IsFailedPrecondition(status)) {
      return status;
    } else {
      DLOG(INFO) << "ExtractFromMSOFile returned " << status;
    }
  }
  return absl::InternalError("Unrecognized file format");
}

// Do the smallest amount of work (in our context) to determine that
// content is most likely an OLE2 file that contains VBA code. Return
// true if it's the case.
bool LikelyOLE2WithVBAContent(absl::string_view content) {
  if (IsOLE2Content(content)) {
    std::string lower_content = absl::AsciiStrToLower(content);
    return lower_content.find("_vba_project") && lower_content.find("vba");
  }
  return false;
}

void ExtractVBAFromStringLightweight(absl::string_view content,
                                     VBACodeChunks *code_chunks,
                                     std::string *error) {
  error->clear();
  auto status = ExtractFromStringInternal(kSupportAllButOOXMLInputType, "",
                                          content, nullptr, code_chunks);
  error->assign(std::string(status.message()));
}

absl::StatusOr<VBACodeChunks> ExtractVBAFromStringWithSettings(
    absl::string_view content, const vba::ExtractVBASettings &settings) {
  VBACodeChunks code_chunks;
  std::string error;
  if (settings.has_extraction_method() &&
      settings.extraction_method() == vba::EXTRACTION_TYPE_LIGHTWEIGHT) {
    ExtractVBAFromStringLightweight(content, &code_chunks, &error);
  } else {
    ExtractVBAFromString(content, &code_chunks, &error);
  }

  if (error.empty()) {
    return code_chunks;
  }
  return absl::InternalError(error);
}

void ExtractDirectoryAndVBAFromString(absl::string_view content,
                                      OLEDirectoryMessage *directory,
                                      VBACodeChunks *code_chunks,
                                      std::string *error) {
  error->clear();
  uint32_t input_type =
      flag_input_type == -1 ? kSupportAllInputType : flag_input_type;
  auto status = ExtractFromStringInternal(input_type, "", content, directory,
                                          code_chunks);
  error->assign(std::string(status.message()));
}

void ExtractVBAFromString(absl::string_view content, VBACodeChunks *code_chunks,
                          std::string *error) {
  ExtractDirectoryAndVBAFromString(content, nullptr, code_chunks, error);
}

void ExtractVBAFromFile(const std::string &filename, VBACodeChunks *code_chunks,
                        std::string *error, bool xor_decode_file) {
  error->clear();
  // Fail nicely if the file can't be read. ReadFileToString will also log
  // some details about the error.
  std::string content;
  if (!utils::ReadFileToString(filename, &content, true, xor_decode_file)) {
    *error = absl::StrFormat("Can not get content for '%s'", filename);
    return;
  }
  auto status = ExtractFromStringInternal(kSupportAllInputType, filename,
                                          content, nullptr, code_chunks);
  DLOG(INFO) << "ExtractFromStringInternal Status: " << status;
  // Prepend the filename to the error message if one was returned.
  if (!status.message().empty()) {
    error->assign(absl::StrCat(filename, ": ", status.message()));
  }
}
}  // namespace maldoca

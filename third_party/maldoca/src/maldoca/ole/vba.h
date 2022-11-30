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

// Extracting VBA code from OLE2 VBA/dir streams.
//
// vba_code::ExtractVBA will extract VBA files from an existing VBA/dir
// directory stream, creating VBACodeChunk messages that are added to
// a VBACodeChunks protocol buffer. Since a VBA/dir code stream can
// contain several projects featuring VBA code, a container for
// repeated VBACodeChunk messages is necessary to hold all existing
// VBA code chunks.
//
// Here's a typical invocation sequence:
//
//   // Read header, FAT and root directory, find the VBA root.
//   OLEHeader header;
//   CHECK(OLEHeader::ParseHeader(input, &header));
//   CHECK(header.IsInitialized());
//   std::vector<uint32_t> fat;
//   CHECK(FAT::Read(input, header, &fat));
//   CHECK_EQ(fat.size(), header.TotalNumSector());
//   OLEDirectoryEntry root;
//   CHECK(OLEDirectoryEntry::ReadDirectory(input, header, fat, &root));
//   CHECK(root.IsInitialized());
//   vba_root = root.FindVBAContentRoot();
//   CHECK_NOTNULL(vba_root);
//
//   // Read the code modules from the VBA root project file.
//   project_dir = vba_root->
//      FindChildByName("project" ,DirectoryStorageType::Stream);
//   CHECK_NOTNULL(project_dir);
//   CHECK(OLEStream::ReadDirectoryContent(input, header, *project_dir, fat,
//                                         &project_content));
//   CHECK(vba_code::ParseCodeModules(project_content, &code_modules));
//
//   // Read the VBA/dir compressed stream, decompress it and extract the
//   // VBA code it might contain.
//   vba_dir = vba_root->
//      FindChildByName("vba", DirectoryStorageType::Storage)->
//      FindChildByName("dir", DirectoryStorageType::Stream);
//   CHECK_NOTNULL(vba_dir);
//   CHECK(OLEStream::ReadDirectoryContent(input, header, *vba_dir, fat,
//                                         &compressed_vba_dir_content));
//   CHECK(OLEStream::DecompressStream(compressed_vba_dir_content,
//                                     &expanded_vba_dir_content));
//   CHECK(vba_code::ExtractVBA(input, header, code_modules,
//                             *vba_root, fat,
//                             expanded_vba_dir_content, &code_chunks));

#ifndef MALDOCA_OLE_VBA_H_
#define MALDOCA_OLE_VBA_H_

#include <string>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/strings/string_view.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/header.h"
#include "maldoca/ole/proto/vba_extraction.pb.h"

namespace maldoca {
namespace vba_code {
// Prepend the existing path with a prefix followed by a colon.
void InsertPathPrefix(const std::string& prefix, VBACodeChunk* chunk);

// Parse the code module extracting from the <VBA_root>/project
// stream to create a set of module name to file extension
// mappings. Return true upon success.
bool ParseCodeModules(
    absl::string_view project_stream,
    absl::node_hash_map<std::string, std::string>* code_modules);

// Reads malformed entries or orphans from the directory_stream using the given
// directory entry 'index'. Such entries are not accessible from the root
// directory entry. Returns the number of VBA code chunks found in malformed
// entries and orphans.
uint32_t ExtractMalformedOrOrphan(
    absl::string_view input, uint32_t index, const OLEHeader& header,
    const OLEDirectoryEntry& root, absl::string_view directory_stream,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    const std::vector<uint32_t>& fat, bool include_hash,
    OLEDirectoryEntry* entry, VBACodeChunks* code_chunks);

// Extract the VBA code found in the <VBA_root>/VBA/dir
// stream. Return true upon complete success. It's possible that
// false be returned in case of partial success and in this case,
// VBA code chunk might be available in code_chunks. On return,
// 'extracted_indices' contains the indices of entries with VBA code extracted
// while traversing the directory tree, starting at 'root'. dir_entries contains
// the directory entries read while traversing the directory tree.
// TODO(b/120686387): this is named ExtractVBA2 temporarily to avoid an issue
// using sapi.
bool ExtractVBA2(
    absl::string_view main_input_string, const OLEHeader& header,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    const OLEDirectoryEntry& root, const std::vector<uint32_t>& fat,
    absl::string_view dir_input_string,
    absl::flat_hash_set<uint32_t>* extracted_indices,
    std::vector<OLEDirectoryEntry*>* dir_entries, VBACodeChunks* code_chunks,
    bool continue_extraction);

// Extracts the VBA code in the same way as ExtractVBA, but also includes the
// sha1 of the VBA macro. On return, 'extracted_indices' contains the indices of
// entries with VBA code extracted while traversing the directory tree, starting
// at 'root'. dir_entries contains the directory entries read while traversing
// the directory tree.
bool ExtractVBAWithHash(
    absl::string_view main_input_string, const OLEHeader& header,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    const OLEDirectoryEntry& root, const std::vector<uint32_t>& fat,
    absl::string_view dir_input_string,
    absl::flat_hash_set<uint32_t>* extracted_indices,
    std::vector<OLEDirectoryEntry*>* dir_entries, VBACodeChunks* code_chunks,
    bool continue_extraction);

}  // namespace vba_code
}  // namespace maldoca

#endif  // MALDOCA_OLE_VBA_H_

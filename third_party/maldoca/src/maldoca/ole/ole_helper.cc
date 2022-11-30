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

#include "maldoca/ole/ole_helper.h"

#include "absl/strings/string_view.h"
#include "maldoca/base/status.h"
#include "maldoca/ole/fat.h"

namespace maldoca {

// Read the header, the FAT and the OLE root from content. Return InternalError
// if something went wrong.
absl::Status ReadHeaderFatRoot(absl::string_view content, OLEHeader *header,
                               std::vector<uint32_t> *fat,
                               OLEDirectoryEntry *root,
                               std::vector<OLEDirectoryEntry *> *dir_entries,
                               std::string *directory_stream) {
  // Read the header
  if (!OLEHeader::ParseHeader(content, header)) {
    return ::maldoca::InternalError(
        "Can not parse a valid OLE2 header in input",
        MaldocaErrorCode::INVALID_OLE2_HEADER);
  }
  DCHECK(header->IsInitialized());

  // Read the FAT
  if (!FAT::Read(content, *header, fat)) {
    return ::maldoca::InternalError("Can not read a valid FAT from input",
                                    MaldocaErrorCode::INVALID_FAT_HEADER);
  }
  // It's technically possible not to run into errors trying to read
  // the FAT and end up with an empty one.
  if (fat->empty()) {
    return ::maldoca::InternalError("FAT is empty",
                                    MaldocaErrorCode::EMPTY_FAT_HEADER);
  }
  if (fat->size() != header->TotalNumSector()) {
    DLOG(WARNING) << "FAT size is " << fat->size()
                  << ", total number of sector reported by header: "
                  << header->TotalNumSector();
  }

  // Read the OLE2 content directory layout
  // TODO(b/120545604): Possibly continue to try and extract VBA code even if
  // there were errors building the tree.
  if (!OLEDirectoryEntry::ReadDirectory(content, *header, *fat, root,
                                        dir_entries, directory_stream)) {
    return ::maldoca::InternalError(
        "Can not read a valid root directory from input",
        MaldocaErrorCode::INVALID_ROOT_DIR);
  }
  CHECK(root->IsInitialized());
  DLOG(INFO) << root->ToString();
  return absl::OkStatus();
}

}  // namespace maldoca

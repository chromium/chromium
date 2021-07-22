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

#if defined(_WIN32)
#include "maldoca/base/utf8/unicodetext.h"
#endif
#include "maldoca/ole/archive_handler.h"

namespace maldoca {
namespace utils {

ArchiveHandler::ArchiveHandler(absl::string_view content,
                               /* not used */ const std::string &extension) : 
                               content_(content) {
  first_entry_processed_ = false;
  initialized_ = false;

  if (content_.empty()) {
    DLOG(ERROR) << "'content' is empty";
    return;
  }

  if (!zip_reader_.OpenFromString(content_)) {
    DLOG(ERROR) << "Error while opening zip file from memory";
    return;
  }

  initialized_ = true;
}

bool ArchiveHandler::GetNextEntry(std::string *filename, int64_t *size,
                                  bool *isdir) {
  DCHECK(filename);
  DCHECK(size);
  DCHECK(isdir);

  if (!initialized_) {
    DLOG(ERROR) << "ArchiveHandler instance not initialized";
    return false;
  }

  if (first_entry_processed_) {
    if (!zip_reader_.AdvanceToNextEntry()) {
      return false;
    }
  } else {
    first_entry_processed_ = true;
  }
  
  if (!zip_reader_.HasMore()){
      return false;
  }

  if (!zip_reader_.OpenCurrentEntryInZip()) {
    return false;
  }

  ::zip::ZipReader::EntryInfo* current_entry_info = zip_reader_.current_entry_info();
  *size = current_entry_info->original_size();
  *isdir = current_entry_info->is_directory();
#if defined(_WIN32)
  *filename = base::WideToUTF8(current_entry_info->file_path().value());
#else
  *filename = current_entry_info->file_path().value();
#endif  // _WIN32

  return true;
}

bool ArchiveHandler::GetEntryContent(std::string *content) {
  // TODO: make flag
  return zip_reader_.ExtractCurrentEntryToString(/* not used: max_read_bytes */
                                                 1, content);
}

bool ArchiveHandler::GetNextGoodContent(std::string *filename, int64_t *size,
                                        std::string *content) {
  DCHECK(filename);
  DCHECK(size);
  DCHECK(content);

  std::string fn;
  int64_t sz = 0;
  bool isdir = false;
  while (GetNextEntry(&fn, &sz, &isdir)) {
    if (isdir) {
      continue;
    }
    *filename = std::move(fn);
    *size = sz;
    content->reserve(*size);
    bool status = GetEntryContent(content);
    if (status) {
      return status;
    }
    // else failed to get content so log and move on
    DLOG(ERROR) << "Failed to fetch " << *filename << " of size " << *size;
  }
  return false;
}
}  // namespace utils
}  // namespace maldoca

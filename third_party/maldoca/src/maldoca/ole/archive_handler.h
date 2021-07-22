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

#ifndef MALDOCA_OLE_ARCHIVE_HANDLER_ZLIB_H_
#define MALDOCA_OLE_ARCHIVE_HANDLER_ZLIB_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "maldoca/base/logging.h"
#include "third_party/zlib/google/zip_reader.h"

using ::zip::ZipReader;

namespace maldoca {
namespace utils {
// A class to handle zip + gzip archive content decompression using
// zlib/minizip.
//
// Sample usage:
//
//   auto handler = ArchiveHandler(zipped_archive_string, "zip");
//   CHECK(handler.Initialized()) << "Can't initialize, error: "
//                                << handler.GetStatusString();
//   string filename, content;
//   int64_t size;
//   while (handler.GetNextEntry(&filename, &size, &content)) {
//    ...
//   }
// TODO: potentially create interface + factory to select different
// archive handler implementations.
class ArchiveHandler {
 public:
  // Content has the archive as a blob. extension is not used in this
  // implementation.
  ArchiveHandler(absl::string_view content,
                 /* not used */ const std::string& extension);
  ArchiveHandler(const ArchiveHandler&) = delete;
  ArchiveHandler& operator=(const ArchiveHandler&) = delete;

  // Get the next archive entry, filling filename, size and isdir. False is
  // returned when no more entries are available. size can be set to -1 if
  // the size of the entry can not be fetched (but if filename can be set,
  // true will be returned). isdir is set to true if it can be determined
  // that the fetched entry is a directory (false if not, including when
  // fetching that information failed.)
  bool GetNextEntry(std::string* filename, int64_t* size, bool* isdir);
  // Get the content from previously entry fetched by GetNextEntry
  bool GetEntryContent(std::string* content);

  // Get the next good archive entry, filling filename, size and content of the
  // next non-directory entry. By good we mean we successfully fetched the
  // content (it maybe that there is an entry but we can't parse it).
  // If no more such entry, return false. It's a helper method to cover the
  // most often used scenario.
  bool GetNextGoodContent(std::string* filename, int64_t* size,
                          std::string* content);

  // Not implemented.
  void SetIgnoreWarning(bool value) {
    LOG(WARNING) << "ArchiveHandler::SetIgnoreWarning is not implemented!";
  }

  inline bool Initialized() const { return initialized_; }

 private:
  // Wrapper class with utility functions around zlib/minizip.
  ZipReader zip_reader_;
  // Copy of the archive's content to unpack.
  std::string content_;
  // AdvanceToNextEntry() is skipped upon the first call to
  // GetNextEntry() as it is already position correctly by OpenFromString().
  bool first_entry_processed_;
  // Set to true if the archive provided in the constructor opens successfully.
  bool initialized_;
};

}  // namespace utils
}  // namespace maldoca
#endif

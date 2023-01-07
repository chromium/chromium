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

#ifndef MALDOCA_OLE_ARCHIVE_HANDLER_H_
#define MALDOCA_OLE_ARCHIVE_HANDLER_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "maldoca/base/logging.h"

namespace maldoca {
namespace utils {
// A class to handle archive content decompression.
//
// Sample usage:
//
//   auto status_or = ::maldoca::utils::GetArchiveHandler(zipped_archive_string,
//   "zip", "" , false, false);
//   CHECK(status_or.ok() && status_or.value()->Initialized()) <<
//   "Can't initialize, error: " << status_or.status();
//
//   auto handler = status_or.value().get();
//   string filename, content;
//   int64_t size;
//   while (handler->GetNextGoodContent(&filename, &size, &content)) {
//    ...
//   }

class ArchiveHandler {
 public:
  ArchiveHandler(const ArchiveHandler&) = delete;
  ArchiveHandler& operator=(const ArchiveHandler&) = delete;
  virtual ~ArchiveHandler() = default;

  // Get the next archive entry, filling filename, size and isdir. False is
  // returned when no more entries are available. size can be set to -1 if
  // the size of the entry can not be fetched (but if filename can be set,
  // true will be returned). isdir is set to true if it can be determined
  // that the fetched entry is a directory (false if not, including when
  // fetching that information failed.)
  virtual bool GetNextEntry(std::string* filename, int64_t* size,
                            bool* isdir) = 0;
  // Get the content from previously entry fetched by GetNextEntry
  virtual bool GetEntryContent(std::string* content) = 0;

  // Get the next good archive entry, filling filename, size and content of the
  // next non-directory entry. By good we mean we successfully fetched the
  // content (it maybe that there is an entry but we can't parse it).
  // If no more such entry, return false. It's a helper method to cover the
  // most often used scenario.
  bool GetNextGoodContent(std::string* filename, int64_t* size,
                          std::string* content) {
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
      // else failed to get content so log and move on to next one.
      LOG(ERROR) << "Failed to fetch " << *filename << " of size " << *size
                 << " with error code: " << ResultCode();
    }
    return false;
  }

  // Simple accessors
  virtual bool Initialized() const = 0;
  virtual int ResultCode() const = 0;
  virtual int NumberOfFilesInArchive() = 0;

 protected:
  ArchiveHandler();
};

// Templated interface for the archive handler. This makes it easier to support
// different archive handler implementations.
template <class T>
class ArchiveHandlerTemplate : public ArchiveHandler {
 public:
  bool GetNextEntry(std::string* filename, int64_t* size,
                    bool* isdir) override {
    CHECK(handler_ != nullptr);
    return handler_->GetNextEntry(filename, size, isdir);
  }
  bool GetEntryContent(std::string* content) override {
    CHECK(handler_ != nullptr);
    return handler_->GetEntryContent(content);
  }
  int NumberOfFilesInArchive() override {
    CHECK(handler_ != nullptr);
    return handler_->NumberOfFilesInArchive();
  }
  bool Initialized() const override {
    CHECK(handler_ != nullptr);
    return handler_->Initialized();
  }
  int ResultCode() const override {
    CHECK(handler_ != nullptr);
    return handler_->ResultCode();
  }

 protected:
  // Pointer to the archive handler implementation.
  std::unique_ptr<T> handler_;
};

// Gets the appropriate ArchiveHandler for the given parameters.
absl::StatusOr<std::unique_ptr<ArchiveHandler>> GetArchiveHandler(
    absl::string_view content, const std::string& extension,
    const std::string& archive_tmp_dir, bool prefer_7z, bool use_7zfile);

}  // namespace utils
}  // namespace maldoca
#endif

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

#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "maldoca/base/logging.h"
#if defined(_WIN32)
#include "maldoca/base/utf8/unicodetext.h"
#endif
#include "maldoca/ole/archive_handler.h"
#include "third_party/zlib/google/zip_reader.h"

using ::zip::ZipReader;

namespace maldoca {
namespace utils {

// Archive handler using 7z.
class ArchiveHandler7zImpl : public ArchiveHandler {
 public:
  ArchiveHandler7zImpl(absl::string_view content);
  ArchiveHandler7zImpl(const ArchiveHandler7zImpl &) = delete;
  ArchiveHandler7zImpl &operator=(const ArchiveHandler7zImpl &) = delete;

  bool GetNextEntry(std::string *filename, int64_t *size, bool *isdir) override;
  bool GetEntryContent(std::string *content) override;

  int NumberOfFilesInArchive() override {
    LOG(ERROR) << "Not implemented!";
    return -1;
  }
  inline bool Initialized() const override { return initialized_; }
  int ResultCode() const override {
    LOG(ERROR) << "Not implemented!";
    return -1;
  }

 private:
  // Wrapper class with utility functions around zlib/minizip.
  ZipReader zip_reader_;
  // Copy of the archive's content to unpack.
  std::string content_;
  // Set to true if the archive provided in the constructor opens successfully.
  bool initialized_;
};

// Archive handler template using ArchiveHandler7zImpl.
class ArchiveHandler7z : public ArchiveHandlerTemplate<ArchiveHandler7zImpl> {
 public:
  ArchiveHandler7z(absl::string_view content);
};

ArchiveHandler::ArchiveHandler() {}

ArchiveHandler7zImpl::ArchiveHandler7zImpl(absl::string_view content)
    : content_(content) {
  initialized_ = false;

  if (content_.empty()) {
    DLOG(ERROR) << "'content' is empty!";
    return;
  }

  if (!zip_reader_.OpenFromString(content_)) {
    DLOG(ERROR) << "Error while opening zip file from memory!";
    return;
  }

  initialized_ = true;
}

bool ArchiveHandler7zImpl::GetNextEntry(std::string *filename, int64_t *size,
                                        bool *isdir) {
  DCHECK(filename);
  DCHECK(size);
  DCHECK(isdir);

  if (!initialized_) {
    DLOG(ERROR) << "ArchiveHandler instance not initialized!";
    return false;
  }

  const ::zip::ZipReader::Entry *const entry = zip_reader_.Next();
  if (!entry) {
    return false;
  }

  *size = entry->original_size;
  *isdir = entry->is_directory;
  *filename = entry->path.AsUTF8Unsafe();

  return true;
}

bool ArchiveHandler7zImpl::GetEntryContent(std::string *content) {
  return zip_reader_.ExtractCurrentEntryToString(/* not used: max_read_bytes */
                                                 1, content);
}

ArchiveHandler7z::ArchiveHandler7z(absl::string_view content) {
  handler_ = absl::make_unique<ArchiveHandler7zImpl>(content);
}

absl::StatusOr<std::unique_ptr<ArchiveHandler>> GetArchiveHandler(
    absl::string_view content, const std::string &extension,
    const std::string &archive_tmp_dir, bool prefer_7z, bool use_7zfile) {
  return absl::make_unique<ArchiveHandler7z>(content);
}
}  // namespace utils
}  // namespace maldoca

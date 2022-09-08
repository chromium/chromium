// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LZMA_SDK_GOOGLE_SEVEN_ZIP_READER_H_
#define THIRD_PARTY_LZMA_SDK_GOOGLE_SEVEN_ZIP_READER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"

namespace seven_zip {

struct EntryInfo {
  // The relative path of this entry, within the archive.
  base::FilePath file_path;

  // The actual size of the entry.
  size_t file_size;

  // The last modified time from the 7z header, if present; or a null time
  // otherwise.
  base::Time last_modified_time;

  // TODO(crbug/1355567): Surface whether a file is encrypted
};

enum class Result {
  kSuccess,
  kFailedToOpen,
  kFailedToAllocate,
  kFailedToExtract,
  kBadCrc,
  kMemoryMappingFailed,
};

class Delegate {
public:
  virtual ~Delegate() = default;

  // Handles errors that may occur when opening an archive.
  virtual void OnOpenError(Result result) = 0;

  // Handles a single entry in the 7z archive being ready for extraction.
  // Returns `true` to extract the entry, and `false` to stop extraction
  // entirely. When returning `true`, populates `output` with a span for
  // extraction. This span must have size equal to `entry.file_size`.
  virtual bool OnEntry(const EntryInfo &entry, base::span<uint8_t> &output) = 0;

  // Handles a single directory in the 7z archive being found. Returns `true` to
  // continue extraction, and `false` to stop extraction.
  virtual bool OnDirectory(const EntryInfo &entry) = 0;

  // Handles an entry being done extracting. If any errors occurred during
  // extraction, they are provided in `result`. Returns `true` to continue
  // extraction, and `false` to stop extraction.
  virtual bool EntryDone(Result result, const EntryInfo &entry) = 0;
};

// Extracts the 7z archive in `seven_zip_file`, and uses `temp_file` as a
// buffer when multiple 'files' are contained in one 7z 'folder'.
void Extract(base::File seven_zip_file, base::File temp_file,
             Delegate &delegate);

} // namespace seven_zip

#endif // THIRD_PARTY_LZMA_SDK_GOOGLE_SEVEN_ZIP_READER_H_

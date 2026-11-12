/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_H_
#define ART_LIBARTBASE_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_H_

#include <stdint.h>

namespace unix_file {

// A file interface supporting random-access reading and writing of content,
// along with the ability to set the length of a file (smaller or greater than
// its current extent).
//
// This interface does not support a stream position (i.e. every read or write
// must specify an offset). This interface does not imply any buffering policy.
//
// All operations return >= 0 on success or -errno on failure.
//
// Implementations never return EINTR; callers are spared the need to manually
// retry interrupted operations.
//
// Any concurrent access to files should be externally synchronized.
class RandomAccessFile {
 public:
  virtual ~RandomAccessFile() { }

  virtual int Close() = 0;

  // Reads 'byte_count' bytes into 'buf' starting at offset 'offset' in the
  // file. Returns the number of bytes actually read.
  virtual int64_t Read(char* buf, int64_t byte_count, int64_t offset) const = 0;

  // Sets the length of the file to 'new_length'. If this is smaller than the
  // file's current extent, data is discarded. If this is greater than the
  // file's current extent, it is as if a write of the relevant number of zero
  // bytes occurred. Returns 0 on success.
  virtual int SetLength(int64_t new_length) = 0;

  // Returns the current size of this file.
  virtual int64_t GetLength() const = 0;

  // Writes 'byte_count' bytes from 'buf' starting at offset 'offset' in the
  // file. Zero-byte writes are acceptable, and writes past the end are as if
  // a write of the relevant number of zero bytes also occurred. Returns the
  // number of bytes actually written.
  virtual int64_t Write(const char* buf, int64_t byte_count, int64_t offset) = 0;

  // Flushes file data.
  virtual int Flush() = 0;
};

}  // namespace unix_file

#endif  // ART_LIBARTBASE_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_H_

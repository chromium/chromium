// Copyright 2019 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stream/file_output_stream.h"

#include "base/logging.h"

namespace crashpad {

FileOutputStream::FileOutputStream(FileHandle file_handle)
    : writer_(file_handle), flush_needed_(false), flushed_(false) {}

FileOutputStream::~FileOutputStream() {
  DCHECK(!flush_needed_);
}

bool FileOutputStream::Write(const uint8_t* data, size_t size) {
  DCHECK(!flushed_);

  if (!writer_.Write(data, size)) {
    LOG(ERROR) << "Write: Failed";
    return false;
  }
  flush_needed_ = true;
  return true;
}

bool FileOutputStream::Flush() {
  flush_needed_ = false;
  flushed_ = true;
  return true;
}

}  // namespace crashpad

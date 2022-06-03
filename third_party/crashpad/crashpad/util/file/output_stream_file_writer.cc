// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "util/file/output_stream_file_writer.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "util/stream/output_stream_interface.h"

namespace crashpad {

OutputStreamFileWriter::OutputStreamFileWriter(
    std::unique_ptr<OutputStreamInterface> output_stream)
    : output_stream_(std::move(output_stream)),
      flush_needed_(false),
      flushed_(false) {}

OutputStreamFileWriter::~OutputStreamFileWriter() {
  DCHECK(!flush_needed_);
}

bool OutputStreamFileWriter::Write(const void* data, size_t size) {
  DCHECK(!flushed_);
  flush_needed_ =
      output_stream_->Write(static_cast<const uint8_t*>(data), size);
  return flush_needed_;
}

bool OutputStreamFileWriter::WriteIoVec(std::vector<WritableIoVec>* iovecs) {
  DCHECK(!flushed_);
  flush_needed_ = true;
  if (iovecs->empty()) {
    LOG(ERROR) << "no iovecs";
    flush_needed_ = false;
    return false;
  }
  for (const WritableIoVec& iov : *iovecs) {
    if (!output_stream_->Write(static_cast<const uint8_t*>(iov.iov_base),
                               iov.iov_len)) {
      flush_needed_ = false;
      return false;
    }
  }
  return true;
}

FileOffset OutputStreamFileWriter::Seek(FileOffset offset, int whence) {
  NOTREACHED();
  return -1;
}

bool OutputStreamFileWriter::Flush() {
  flush_needed_ = false;
  flushed_ = true;
  return output_stream_->Flush();
}

}  // namespace crashpad

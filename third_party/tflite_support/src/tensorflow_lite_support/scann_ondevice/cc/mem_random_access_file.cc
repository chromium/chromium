/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/scann_ondevice/cc/mem_random_access_file.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "leveldb/env.h"  // from @com_google_leveldb
#include "leveldb/slice.h"  // from @com_google_leveldb
#include "leveldb/status.h"  // from @com_google_leveldb

namespace tflite {
namespace scann_ondevice {

MemRandomAccessFile::MemRandomAccessFile(const char* buffer_data,
                                         size_t buffer_size)
    : buffer_data_(buffer_data), buffer_size_(buffer_size) {}

MemRandomAccessFile::~MemRandomAccessFile() {}

leveldb::Status MemRandomAccessFile::Read(uint64_t offset, size_t n,
                                          leveldb::Slice* result,
                                          char* scratch) const {
  // Sanity check.
  if (offset > buffer_size_) {
    return leveldb::Status::InvalidArgument(
        "Read offset is beyond buffer size");
  }
  // Truncate result if the requested chunk extends beyond the buffer.
  const size_t result_size =
      std::min(n, buffer_size_ - static_cast<size_t>(offset));
  *result = leveldb::Slice(buffer_data_ + offset, result_size);
  return leveldb::Status::OK();
}

}  // namespace scann_ondevice
}  // namespace tflite

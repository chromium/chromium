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

#include "tensorflow_lite_support/scann_ondevice/cc/mem_writable_file.h"

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl

namespace tflite {
namespace scann_ondevice {

/* static */
absl::StatusOr<std::unique_ptr<MemWritableFile>> MemWritableFile::Create(
    std::string* buffer) {
  if (buffer == nullptr) {
    return absl::InvalidArgumentError("Buffer can't be null");
  }
  return absl::WrapUnique(new MemWritableFile(buffer));
}

MemWritableFile::MemWritableFile(std::string* buffer) : buffer_(buffer) {}

leveldb::Status MemWritableFile::Append(const leveldb::Slice& data) {
  buffer_->append(data.data(), data.data() + data.size());
  return leveldb::Status::OK();
}

leveldb::Status MemWritableFile::Close() {
  // Do nothing.
  return leveldb::Status::OK();
}

leveldb::Status MemWritableFile::Flush() {
  // Do nothing.
  return leveldb::Status::OK();
}

leveldb::Status MemWritableFile::Sync() {
  // Do nothing.
  return leveldb::Status::OK();
}

}  // namespace scann_ondevice
}  // namespace tflite

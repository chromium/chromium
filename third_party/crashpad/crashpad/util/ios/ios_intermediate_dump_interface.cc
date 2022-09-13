// Copyright 2021 The Crashpad Authors
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

#include "util/ios/ios_intermediate_dump_interface.h"

#include "util/file/scoped_remove_file.h"

namespace crashpad {
namespace internal {

bool IOSIntermediateDumpFilePath::Initialize(const base::FilePath& path) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  ScopedRemoveFile file_remover(path);
  handle_.reset(LoggingOpenFileForRead(path));
  if (!handle_.is_valid())
    return false;

  reader_ = std::make_unique<WeakFileHandleFileReader>(handle_.get());
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

FileReaderInterface* IOSIntermediateDumpFilePath::FileReader() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return reader_.get();
}

FileOffset IOSIntermediateDumpFilePath::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return LoggingFileSizeByHandle(handle_.get());
}

IOSIntermediateDumpByteArray::IOSIntermediateDumpByteArray(const void* data,
                                                           size_t size) {
  string_file_ = std::make_unique<StringFile>();
  string_file_->SetString(
      std::string(reinterpret_cast<const char*>(data), size));
}

FileReaderInterface* IOSIntermediateDumpByteArray::FileReader() const {
  return string_file_.get();
}

FileOffset IOSIntermediateDumpByteArray::Size() const {
  return string_file_->string().size();
}

}  // namespace internal
}  // namespace crashpad

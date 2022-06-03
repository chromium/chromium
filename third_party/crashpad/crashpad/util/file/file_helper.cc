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

#include "util/file/file_helper.h"

namespace crashpad {

void CopyFileContent(FileReaderInterface* file_reader,
                     FileWriterInterface* file_writer) {
  char buf[4096];
  FileOperationResult read_result;
  do {
    read_result = file_reader->Read(buf, sizeof(buf));
    if (read_result < 0) {
      break;
    }
    if (read_result > 0 && !file_writer->Write(buf, read_result)) {
      break;
    }
  } while (read_result > 0);
}

}  // namespace crashpad

// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATE_DUMP_READER_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATE_DUMP_READER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "util/file/file_reader.h"
#include "util/ios/ios_intermediate_dump_map.h"

namespace crashpad {
namespace internal {

//! \brief Open and parse iOS intermediate dumps.
class IOSIntermediateDumpReader {
 public:
  IOSIntermediateDumpReader() {}

  //! \brief Open and parses \a path, ignoring empty files.
  //!
  //! Will attempt to parse the binary file, similar to a JSON file, using the
  //! same format used by IOSIntermediateDumpWriter, resulting in an
  //! IOSIntermediateDumpMap
  //!
  //! \param[in] path The intermediate dump to read.
  //!
  //! \return On success, returns `true`, otherwise returns `false`. Clients may
  //!     still attempt to parse RootMap, as partial minidumps may still be
  //!     usable.
  bool Initialize(const base::FilePath& path);

  //! \brief Returns an IOSIntermediateDumpMap corresponding to the root of the
  //!     intermediate dump.
  const IOSIntermediateDumpMap* RootMap() { return &minidump_; }

 private:
  bool Parse(FileReaderInterface* reader, FileOffset file_size);
  IOSIntermediateDumpMap minidump_;

  DISALLOW_COPY_AND_ASSIGN(IOSIntermediateDumpReader);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATE_DUMP_READER_H_

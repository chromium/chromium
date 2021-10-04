/*
 * Copyright 2021 Google LLC
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

// Put all files related functions here.

#ifndef MALDOCA_BASE_FILE_H_
#define MALDOCA_BASE_FILE_H_

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/files/file_path.h"
#ifndef MALDOCA_IN_CHROMIUM
#include "google/protobuf/message.h"  // nogncheck
#endif
#include "maldoca/base/statusor.h"
#if defined(_WIN32)
#include "maldoca/base/utf8/unicodetext.h"
#endif  // _WIN32

namespace maldoca {
namespace file {

#ifndef MALDOCA_CHROME
absl::Status Match(absl::string_view pattern,
                   std::vector<std::string>* filenames);
#endif  // MALDOCA_CHROME

absl::Status GetContents(const std::string& path, std::string* content,
                         bool xor_decode_file = false);
StatusOr<std::string> GetContents(absl::string_view path,
                                  bool xor_decode_file = false);

#ifndef MALDOCA_CHROME
absl::Status SetContents(const std::string& path, absl::string_view contents);
inline absl::Status SetContents(absl::string_view path,
                                absl::string_view contents) {
  return SetContents(std::string(path), contents);
}
#endif  // MALDOCA_CHROME

inline std::string JoinPath(const std::string path1, const std::string path2) {
#if defined(_WIN32)
  base::FilePath path =
      base::FilePath(base::UTF8ToWide(path1)).Append(base::UTF8ToWide(path2));
  return base::WideToUTF8(path.value());
#else
  return base::FilePath(path1).Append(path2).value();
#endif  // _WIN32
}

// split a file name into <base, extension>.  Note base will include any prefix.
std::pair<absl::string_view, absl::string_view> SplitFilename(
    absl::string_view path);

#ifndef MALDOCA_CHROME
absl::Status CreateDir(const std::string& path, int mode);
inline absl::Status CreateDir(absl::string_view path) {
  return CreateDir(std::string(path),
                   0770);  // defaults user/group rwx, world none.
}

std::string CreateTempFileAndCloseOrDie(absl::string_view directory,
                                        const std::string& contents);
absl::Status GetTextProto(absl::string_view filename,
                          ::google::protobuf::Message* proto);

// Get a temp dir for testing
inline std::string TestTempDir() {
  auto var = getenv("TEST_TMPDIR");
  if (var != nullptr && var[0]) {
    return var;
  } else {
    return "/tmp";
  }
}
#endif  // MALDOCA_CHROME

}  // namespace file
}  // namespace maldoca

#endif  // MALDOCA_BASE_FILE_H_

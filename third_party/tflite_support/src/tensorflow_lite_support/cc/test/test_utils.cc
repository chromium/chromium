/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/test/test_utils.h"

#include "absl/strings/str_cat.h"  // from @com_google_absl

namespace tflite {
namespace task {

std::string JoinPath(absl::string_view path1, absl::string_view path2) {
  if (path1.empty()) return std::string(path2);
  if (path2.empty()) return std::string(path1);
  if (path1.back() == '/') {
    if (path2.front() == '/')
      return absl::StrCat(path1, absl::ClippedSubstr(path2, 1));
  } else {
    if (path2.front() != '/') return absl::StrCat(path1, "/", path2);
  }
  return absl::StrCat(path1, path2);
}

namespace internal {

// Given a collection of file paths, append them all together,
// ensuring that the proper path separators are inserted between them.
std::string JoinPathImpl(bool honor_abs,
                         std::initializer_list<absl::string_view> paths) {
  std::string result;

  if (paths.size() != 0) {
    // This size calculation is worst-case: it assumes one extra "/" for every
    // path other than the first.
    size_t total_size = paths.size() - 1;
    for (const absl::string_view path : paths) total_size += path.size();
    result.resize(total_size);

    auto begin = result.begin();
    auto out = begin;
    bool trailing_slash = false;
    for (absl::string_view path : paths) {
      if (path.empty()) continue;
      if (path.front() == '/') {
        if (honor_abs) {
          out = begin;  // wipe out whatever we've built up so far.
        } else if (trailing_slash) {
          path.remove_prefix(1);
        }
      } else {
        if (!trailing_slash && out != begin) *out++ = '/';
      }
      const size_t this_size = path.size();
      memcpy(&*out, path.data(), this_size);
      out += this_size;
      trailing_slash = out[-1] == '/';
    }
    result.erase(out - begin);
  }
  return result;
}

}  // namespace internal
}  // namespace task
}  // namespace tflite

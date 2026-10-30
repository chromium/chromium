// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_GROUPED_PATH_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_GROUPED_PATH_H_

#include <stddef.h>

#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace caspian {
struct GroupedPath {
  // TreeNode id_paths can be grouped by component or template, for example
  // "Blink>JavaScript/v8/natives_blob.bin/assets" has the base path
  // "v8/natives_blob.bin/assets" and has been grouped to the component
  // "Blink>JavaScript". This is a lightweight utility class for managing this
  // two-level path structure without wasteful string allocations.
  GroupedPath();
  GroupedPath(const GroupedPath& other);
  GroupedPath(std::string_view group_in, std::string_view path_in);
  ~GroupedPath();

  // Returns |ToString.size()| without actually creating the string.
  size_t size() const {
    size_t sep_size = (group.empty() || path.empty()) ? 0 : 1;  // For '/'.
    return group.size() + path.size() + sep_size;
  }

  std::string_view ShortName(char group_separator) const;
  GroupedPath Parent(char group_separator) const;
  bool IsTopLevelPath() const;
  bool empty() const { return path.empty() && group.empty(); }
  bool operator==(const GroupedPath& other) const;
  std::string ToString() const;

  bool operator<(const GroupedPath& other) const;

  template <typename H>
  friend H AbslHashValue(H h, const GroupedPath& grouped_path) {
    return H::combine(std::move(h), grouped_path.group, grouped_path.path);
  }

  std::string_view group;
  std::string_view path;
};

std::ostream& operator<<(std::ostream& os, const GroupedPath& path);

}  // namespace caspian

#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_GROUPED_PATH_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/grouped_path.h"

#include <stdint.h>

#include <tuple>

namespace {

// Returns |s| with the last |sep| + [suffix] removed, or "" if |sep| is not
// found.
std::string_view RemoveLastSegment(std::string_view s, char sep) {
  size_t sep_idx = s.find_last_of(sep);
  return s.substr(0, (sep_idx == std::string_view::npos) ? 0 : sep_idx);
}

}  // namespace

namespace caspian {

GroupedPath::GroupedPath() = default;
GroupedPath::GroupedPath(const GroupedPath& other) = default;
GroupedPath::GroupedPath(std::string_view group_in, std::string_view path_in)
    : group(group_in), path(path_in) {}
GroupedPath::~GroupedPath() = default;

std::string_view GroupedPath::ShortName(char group_separator) const {
  if (path.empty()) {
    // If there's no group separator, return entire group name;
    return group.substr(group.find_last_of(group_separator) + 1);
  }
  // If there's no path separator, return entire path name.
  return path.substr(path.find_last_of('/') + 1);
}

GroupedPath GroupedPath::Parent(char group_separator) const {
  if (path.empty()) {
    return GroupedPath{RemoveLastSegment(group, group_separator), path};
  }
  return GroupedPath{group, RemoveLastSegment(path, '/')};
}

bool GroupedPath::IsTopLevelPath() const {
  return std::string_view::npos == path.find_first_of('/');
}

bool GroupedPath::operator==(const GroupedPath& other) const {
  return group == other.group && path == other.path;
}

std::string GroupedPath::ToString() const {
  std::string ret;
  ret.reserve(size());
  ret += group;
  if (!group.empty() && !path.empty()) {
    ret += "/";
  }
  ret += path;
  return ret;
}

bool GroupedPath::operator<(const GroupedPath& other) const {
  return std::tie(group, path) < std::tie(other.group, other.path);
}

std::ostream& operator<<(std::ostream& os, const GroupedPath& path) {
  return os << "GroupedPath(group=\"" << path.group << "\", path=\""
            << path.path << "\")";
}

}  // namespace caspian

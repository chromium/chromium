// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_DUMP_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_DUMP_UTIL_H_

#include "base/containers/fixed_flat_map.h"

#include <list>
#include <string>

namespace ui {
class WaylandWindow;

inline std::string ToBoolString(bool b) {
  return b ? "true" : "false";
}

inline std::string GetWindowName(const WaylandWindow* window) {
  // TODO(oshima): Pass name from aura::Window.
  return window ? "exits" : "nullptr";
}

inline std::string ListToString(const std::list<std::string>& list) {
  std::string out;
  for (const auto& i : list) {
    out += i + ",";
  }
  return out;
}

// Produces comma separated string of the values in the map
// whose key matches the give mask.
template <typename M>
std::string ToMatchingKeyMaskString(int mask, const M& map) {
  std::string str;
  for (const auto& pair : map) {
    if (pair.first & mask) {
      str += pair.second;
      str += ",";
    }
  }
  return str;
}

// Return the value of the map matching the given key, or return
// `defualt_value`.
template <typename M, typename K>
const char* GetMapValueOrDefault(const M& map,
                                 const K& key,
                                 const char* default_value = "unknown") {
  auto pair = map.find(key);
  return pair == map.end() ? default_value : pair->second;
}

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_DUMP_UTIL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/display_test_util.h"

#include <cstdint>

#include "base/strings/string_split.h"
#include "ui/display/manager/managed_display_info.h"

namespace display {

std::vector<ManagedDisplayInfo> CreateDisplayInfoListFromSpecs(
    const std::string& display_specs,
    const std::vector<Display>& existing_displays,
    bool generate_new_ids) {
  auto display_info_list = std::vector<ManagedDisplayInfo>();
  std::vector<std::string> parts = base::SplitString(
      display_specs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  size_t index = 0;

  for (const auto& part : parts) {
    int64_t id = (index < existing_displays.size() && !generate_new_ids)
                     ? existing_displays[index].id()
                     : kInvalidDisplayId;
    display_info_list.push_back(
        ManagedDisplayInfo::CreateFromSpecWithID(part, id));
    ++index;
  }
  return display_info_list;
}

}  // namespace display

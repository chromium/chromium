// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen_infos.h"

#include <algorithm>

namespace display {

ScreenInfos::ScreenInfos() = default;
ScreenInfos::ScreenInfos(const ScreenInfo& screen_info)
    : screen_infos{screen_info}, current_display_id(screen_info.display_id) {}
ScreenInfos::ScreenInfos(const ScreenInfos& other) = default;
ScreenInfos::~ScreenInfos() = default;
ScreenInfos& ScreenInfos::operator=(const ScreenInfos& other) = default;

ScreenInfo& ScreenInfos::mutable_current() {
  return const_cast<ScreenInfo&>(
      const_cast<const ScreenInfos*>(this)->current());
}

const ScreenInfo& ScreenInfos::current() const {
  const auto& current_screen_info = std::ranges::find(
      screen_infos, current_display_id, &ScreenInfo::display_id);
  CHECK(current_screen_info != screen_infos.end());
  return *current_screen_info;
}

}  // namespace display

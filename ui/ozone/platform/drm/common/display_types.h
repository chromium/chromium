// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DISPLAY_TYPES_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DISPLAY_TYPES_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"

namespace display {
class DisplaySnapshot;
}  // namespace display

namespace ui {

using MovableDisplaySnapshots =
    std::vector<std::unique_ptr<display::DisplaySnapshot>>;

using EventPropertyMap = base::flat_map<std::string, std::string>;

using MapEdidIdToDisplaySnapshot =
    base::flat_map<int64_t, display::DisplaySnapshot*>;

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DISPLAY_TYPES_H_

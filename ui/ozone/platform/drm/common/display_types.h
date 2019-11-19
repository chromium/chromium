// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DISPLAY_TYPES_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DISPLAY_TYPES_H_

#include <memory>

namespace display {
class DisplaySnapshot;
}  // namespace display

namespace ui {

using MovableDisplaySnapshots =
    std::vector<std::unique_ptr<display::DisplaySnapshot>>;

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DISPLAY_TYPES_H_

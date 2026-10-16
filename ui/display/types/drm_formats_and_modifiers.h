// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DRM_FORMATS_AND_MODIFIERS_H_
#define UI_DISPLAY_TYPES_DRM_FORMATS_AND_MODIFIERS_H_

#include <stdint.h>

#include <vector>

#include "base/containers/flat_map.h"

namespace display {

// A map of DRM formats and modifiers that are supported by the hardware planes
// of the display.
// See third_party/libdrm/src/include/drm/drm_fourcc.h for the canonical list of
// formats and modifiers
using DrmFormatsAndModifiers = base::flat_map<uint32_t, std::vector<uint64_t>>;

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DRM_FORMATS_AND_MODIFIERS_H_

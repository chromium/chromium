// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_HARDWARE_CAPABILITIES_H_
#define UI_OZONE_PUBLIC_HARDWARE_CAPABILITIES_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "ui/gfx/buffer_types.h"

namespace ui {

struct COMPONENT_EXPORT(OZONE_BASE) HardwareCapabilities {
  HardwareCapabilities();
  HardwareCapabilities(const HardwareCapabilities& other);
  HardwareCapabilities& operator=(const HardwareCapabilities& other);
  ~HardwareCapabilities();

  // Whether this is a valid response from the HardwareDisplayPlaneManager.
  bool is_valid = false;
  // Number of planes available to the current CRTC(s).
  // This is specifically the count of non-CURSOR planes, because some boards
  // may have extra PRIMARY planes that could be used for overlays.
  int num_overlay_capable_planes = 0;
  // Whether the CURSOR plane can be displayed independently of the other
  // planes. On some platforms, the CURSOR plane is flattened to the topmost
  // plane before presentation, so all transformations on the topmost plane
  // (e.g. translation, scaling) are erroneously applied to the CURSOR as well.
  bool has_independent_cursor_plane = true;
  // Supported buffer formats for overlaying.
  base::flat_set<gfx::BufferFormat> supported_buffer_formats;
};
using HardwareCapabilitiesCallback =
    base::RepeatingCallback<void(HardwareCapabilities)>;

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_HARDWARE_CAPABILITIES_H_

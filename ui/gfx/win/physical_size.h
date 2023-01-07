// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_PHYSICAL_SIZE_H_
#define UI_GFX_WIN_PHYSICAL_SIZE_H_

#include <string>
#include <vector>

#include "ui/gfx/gfx_export.h"

namespace gfx {

struct PhysicalDisplaySize {
  PhysicalDisplaySize(const std::string& display_name,
                      int width_mm,
                      int height_mm)
      : display_name(display_name), width_mm(width_mm), height_mm(height_mm) {}

  std::string display_name;
  int width_mm;
  int height_mm;
};

// Gets the physical size for all displays.
GFX_EXPORT std::vector<PhysicalDisplaySize> GetPhysicalSizeForDisplays();

}  // namespace gfx

#endif  // UI_GFX_WIN_PHYSICAL_SIZE_H_

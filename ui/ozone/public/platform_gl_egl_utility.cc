// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/platform_gl_egl_utility.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace ui {

PlatformGLEGLUtility::PlatformGLEGLUtility() = default;

PlatformGLEGLUtility::~PlatformGLEGLUtility() = default;

bool PlatformGLEGLUtility::HasVisualManager() {
  return false;
}

bool PlatformGLEGLUtility::UpdateVisualsOnGpuInfoChanged(
    bool software_rendering,
    uint32_t default_visual_id,
    uint32_t transparent_visual_id) {
  NOTREACHED() << "This must not be called if the platform does not support "
                  "X11 visuals.";
  return false;
}

}  // namespace ui

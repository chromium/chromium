// Copyright 2020 The Chromium Authors
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

std::optional<base::ScopedEnvironmentVariableOverride>
PlatformGLEGLUtility::MaybeGetScopedDisplayUnsetForVulkan() {
  return std::nullopt;
}

}  // namespace ui

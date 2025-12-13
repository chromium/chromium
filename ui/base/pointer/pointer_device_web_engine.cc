// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "build/build_config.h"
#include "ui/base/pointer/pointer_device.h"

#if !BUILDFLAG(IS_FUCHSIA)
#error WebEngine only supports Fuchsia.
#endif

namespace ui {

std::pair<int, int> GetAvailablePointerAndHoverTypesImpl() {
  return {POINTER_TYPE_COARSE, HOVER_TYPE_NONE};
}

TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  return 2;
}

}  // namespace ui
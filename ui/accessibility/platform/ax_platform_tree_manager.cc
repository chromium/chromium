// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_tree_manager.h"

namespace ui {

bool AXPlatformTreeManager::IsPlatformTreeManager() const {
  return true;
}

}  // namespace ui

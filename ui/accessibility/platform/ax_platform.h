// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_

#include "base/component_export.h"

namespace ui {

// Process-wide accessibility platform state.
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatform {
 public:
  // Returns the single process-wide instance.
  static AXPlatform& GetInstance();

  // Constructs a new instance. Only one instance may be alive in a process at
  // any given time. Typically, the embedder creates one during process startup
  // and ensures that it is kept alive throughout the process's UX.
  AXPlatform();
  AXPlatform(const AXPlatform&) = delete;
  AXPlatform& operator=(const AXPlatform&) = delete;
  ~AXPlatform();
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_

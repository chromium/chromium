// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_DRM_MODIFIERS_FILTER_H_
#define UI_OZONE_PUBLIC_DRM_MODIFIERS_FILTER_H_

#include <vector>

#include "base/component_export.h"
#include "ui/gfx/buffer_types.h"

namespace ui {

// Class that allows the Ozone platform to filter out DRM modifiers that are
// incompatible with usage elsewhere in Chrome. Chrome may have application-
// specific restrictions on its usable modifiers, and DrmModifiersFilter lets
// us express those restrictions in a way where the platform is agnostic to the
// actual filter logic.
//
// For example, when display compositing with Vulkan, the compositor will
// import a framebuffer allocated by the Ozone platform into Vulkan for draw.
// However, Vulkan implementations may only support a subset of all valid
// modifiers for a GPU, so the platform (which otherwise doesn't know about
// Chrome's requirements) needs the filter to know which modifiers it can
// allocate with.
class COMPONENT_EXPORT(OZONE_BASE) DrmModifiersFilter {
 public:
  virtual ~DrmModifiersFilter() = default;

  virtual std::vector<uint64_t> Filter(
      gfx::BufferFormat format,
      const std::vector<uint64_t>& modifiers) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_DRM_MODIFIERS_FILTER_H_

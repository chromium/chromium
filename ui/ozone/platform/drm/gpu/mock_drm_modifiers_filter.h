// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_MODIFIERS_FILTER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_MODIFIERS_FILTER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "ui/gfx/buffer_types.h"
#include "ui/ozone/public/drm_modifiers_filter.h"

namespace ui {

// Mock implementation of a |DrmModifiersFilter| that can be used in unit
// tests without invoking hardware APIs like Vulkan.
class MockDrmModifiersFilter : public DrmModifiersFilter {
 public:
  explicit MockDrmModifiersFilter(
      const std::vector<uint64_t>& supported_modifiers);

  ~MockDrmModifiersFilter() override;

  std::vector<uint64_t> Filter(gfx::BufferFormat format,
                               const std::vector<uint64_t>& modifiers) override;

 private:
  base::flat_set<uint64_t> supported_modifiers_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_MODIFIERS_FILTER_H_

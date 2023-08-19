// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_modifiers_filter.h"

namespace ui {

MockDrmModifiersFilter::MockDrmModifiersFilter(
    const std::vector<uint64_t>& supported_modifiers)
    : supported_modifiers_{supported_modifiers} {}

MockDrmModifiersFilter::~MockDrmModifiersFilter() = default;

std::vector<uint64_t> MockDrmModifiersFilter::Filter(
    gfx::BufferFormat format,
    const std::vector<uint64_t>& modifiers) {
  std::vector<uint64_t> intersection;
  for (const auto& modifier : modifiers) {
    if (supported_modifiers_.find(modifier) != supported_modifiers_.end()) {
      intersection.push_back(modifier);
    }
  }
  return intersection;
}

}  // namespace ui

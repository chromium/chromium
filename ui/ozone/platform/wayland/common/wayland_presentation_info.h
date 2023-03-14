// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_PRESENTATION_INFO_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_PRESENTATION_INFO_H_

#include "ui/gfx/presentation_feedback.h"

namespace wl {

struct WaylandPresentationInfo {
  WaylandPresentationInfo() = default;

  WaylandPresentationInfo(uint32_t in_frame_id,
                          const gfx::PresentationFeedback& in_feedback)
      : frame_id(in_frame_id), feedback(in_feedback) {}

  WaylandPresentationInfo(const WaylandPresentationInfo& other) = default;
  WaylandPresentationInfo& operator=(const WaylandPresentationInfo& other) =
      default;

  uint32_t frame_id = 0;
  gfx::PresentationFeedback feedback;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_PRESENTATION_INFO_H_

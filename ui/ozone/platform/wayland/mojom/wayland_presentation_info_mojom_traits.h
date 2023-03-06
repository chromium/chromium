// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_PRESENTATION_INFO_MOJOM_TRAITS_H_
#define UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_PRESENTATION_INFO_MOJOM_TRAITS_H_

#include "ui/gfx/mojom/presentation_feedback_mojom_traits.h"
#include "ui/ozone/platform/wayland/common/wayland_presentation_info.h"
#include "ui/ozone/platform/wayland/mojom/wayland_presentation_info.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<wl::mojom::WaylandPresentationInfoDataView,
                    wl::WaylandPresentationInfo> {
  static uint32_t frame_id(const wl::WaylandPresentationInfo& input) {
    return input.frame_id;
  }

  static const gfx::PresentationFeedback& feedback(
      const wl::WaylandPresentationInfo& input) {
    return input.feedback;
  }

  static bool Read(wl::mojom::WaylandPresentationInfoDataView data,
                   wl::WaylandPresentationInfo* out);
};

}  // namespace mojo

#endif  // UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_PRESENTATION_INFO_MOJOM_TRAITS_H_

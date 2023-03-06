// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/mojom/wayland_presentation_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<wl::mojom::WaylandPresentationInfoDataView,
                  wl::WaylandPresentationInfo>::
    Read(wl::mojom::WaylandPresentationInfoDataView data,
         wl::WaylandPresentationInfo* out) {
  out->frame_id = data.frame_id();

  if (!data.ReadFeedback(&out->feedback)) {
    return false;
  }

  return true;
}

}  // namespace mojo

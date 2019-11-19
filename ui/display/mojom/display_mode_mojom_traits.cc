// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_mode_mojom_traits.h"

#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<display::mojom::DisplayModeDataView,
                  std::unique_ptr<display::DisplayMode>>::
    Read(display::mojom::DisplayModeDataView data,
         std::unique_ptr<display::DisplayMode>* out) {
  gfx::Size size;
  if (!data.ReadSize(&size))
    return false;
  *out = std::make_unique<display::DisplayMode>(size, data.is_interlaced(),
                                                data.refresh_rate());
  return true;
}

}  // namespace mojo

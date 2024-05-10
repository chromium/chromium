// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_configuration_params_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<display::mojom::DisplayConfigurationParamsDataView,
                  display::DisplayConfigurationParams>::
    Read(display::mojom::DisplayConfigurationParamsDataView data,
         display::DisplayConfigurationParams* out) {
  gfx::Point origin;
  if (!data.ReadOrigin(&origin))
    return false;

  std::unique_ptr<display::DisplayMode> mode;
  if (!data.ReadMode(&mode))
    return false;

  out->id = data.id();
  out->origin = origin;
  out->mode = std::move(mode);
  out->enable_vrr = data.enable_vrr();

  return true;
}

}  // namespace mojo

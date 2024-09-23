// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_CONFIGURATION_PARAMS_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_CONFIGURATION_PARAMS_MOJOM_TRAITS_H_

#include "ui/display/mojom/display_configuration_params.mojom.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct StructTraits<display::mojom::DisplayConfigurationParamsDataView,
                    display::DisplayConfigurationParams> {
  static int64_t id(
      const display::DisplayConfigurationParams& display_configuration_params) {
    return display_configuration_params.id;
  }

  static gfx::Point origin(
      const display::DisplayConfigurationParams& display_configuration_params) {
    return display_configuration_params.origin;
  }

  static const std::unique_ptr<display::DisplayMode>& mode(
      const display::DisplayConfigurationParams& display_configuration_params) {
    return display_configuration_params.mode;
  }

  static bool enable_vrr(
      const display::DisplayConfigurationParams& display_configuration_params) {
    return display_configuration_params.enable_vrr;
  }

  static bool Read(display::mojom::DisplayConfigurationParamsDataView data,
                   display::DisplayConfigurationParams* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_CONFIGURATION_PARAMS_MOJOM_TRAITS_H_

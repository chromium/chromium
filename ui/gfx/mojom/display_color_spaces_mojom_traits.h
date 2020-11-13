// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_DISPLAY_COLOR_SPACES_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_DISPLAY_COLOR_SPACES_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/display_color_spaces.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::ContentColorUsage, gfx::ContentColorUsage> {
  static gfx::mojom::ContentColorUsage ToMojom(gfx::ContentColorUsage input);
  static bool FromMojom(gfx::mojom::ContentColorUsage input,
                        gfx::ContentColorUsage* output);
};

template <>
struct StructTraits<gfx::mojom::DisplayColorSpacesDataView,
                    gfx::DisplayColorSpaces> {
  static base::span<const gfx::ColorSpace> color_spaces(
      const gfx::DisplayColorSpaces& input);
  static base::span<const gfx::BufferFormat> buffer_formats(
      const gfx::DisplayColorSpaces& input);
  static float sdr_white_level(const gfx::DisplayColorSpaces& input) {
    return input.GetSDRWhiteLevel();
  }

  static bool Read(gfx::mojom::DisplayColorSpacesDataView data,
                   gfx::DisplayColorSpaces* out);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_DISPLAY_COLOR_SPACES_MOJOM_TRAITS_H_

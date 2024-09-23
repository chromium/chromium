// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/display_color_spaces_mojom_traits.h"

#include "base/notreached.h"
#include "skia/public/mojom/skcolorspace_primaries_mojom_traits.h"

namespace mojo {

// static
gfx::mojom::ContentColorUsage
EnumTraits<gfx::mojom::ContentColorUsage, gfx::ContentColorUsage>::ToMojom(
    gfx::ContentColorUsage input) {
  switch (input) {
    case gfx::ContentColorUsage::kSRGB:
      return gfx::mojom::ContentColorUsage::kSRGB;
    case gfx::ContentColorUsage::kWideColorGamut:
      return gfx::mojom::ContentColorUsage::kWideColorGamut;
    case gfx::ContentColorUsage::kHDR:
      return gfx::mojom::ContentColorUsage::kHDR;
  }
  NOTREACHED();
}

// static
bool EnumTraits<gfx::mojom::ContentColorUsage, gfx::ContentColorUsage>::
    FromMojom(gfx::mojom::ContentColorUsage input,
              gfx::ContentColorUsage* output) {
  switch (input) {
    case gfx::mojom::ContentColorUsage::kSRGB:
      *output = gfx::ContentColorUsage::kSRGB;
      return true;
    case gfx::mojom::ContentColorUsage::kWideColorGamut:
      *output = gfx::ContentColorUsage::kWideColorGamut;
      return true;
    case gfx::mojom::ContentColorUsage::kHDR:
      *output = gfx::ContentColorUsage::kHDR;
      return true;
  }
  return false;
}

// static
base::span<const gfx::ColorSpace>
StructTraits<gfx::mojom::DisplayColorSpacesDataView, gfx::DisplayColorSpaces>::
    color_spaces(const gfx::DisplayColorSpaces& input) {
  return input.color_spaces_;
}

// static
base::span<const gfx::BufferFormat>
StructTraits<gfx::mojom::DisplayColorSpacesDataView, gfx::DisplayColorSpaces>::
    buffer_formats(const gfx::DisplayColorSpaces& input) {
  return input.buffer_formats_;
}

// static
bool StructTraits<
    gfx::mojom::DisplayColorSpacesDataView,
    gfx::DisplayColorSpaces>::Read(gfx::mojom::DisplayColorSpacesDataView input,
                                   gfx::DisplayColorSpaces* out) {
  base::span<gfx::BufferFormat> buffer_formats(out->buffer_formats_);
  if (!input.ReadBufferFormats(&buffer_formats))
    return false;

  base::span<gfx::ColorSpace> color_spaces(out->color_spaces_);
  if (!input.ReadColorSpaces(&color_spaces))
    return false;

  SkColorSpacePrimaries primaries = {0.f};
  if (!input.ReadPrimaries(&primaries))
    return false;
  out->SetPrimaries(primaries);

  out->SetSDRMaxLuminanceNits(input.sdr_max_luminance_nits());
  out->SetHDRMaxLuminanceRelative(input.hdr_max_luminance_relative());

  return true;
}

}  // namespace mojo

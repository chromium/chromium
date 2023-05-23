// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_HDR_STATIC_METADATA_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_HDR_STATIC_METADATA_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "ui/gfx/hdr_static_metadata.h"
#include "ui/gfx/mojom/hdr_static_metadata.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::HDRStaticMetadataDataView,
                 gfx::HDRStaticMetadata> {
  static float max(const gfx::HDRStaticMetadata& input) { return input.max; }
  static float max_avg(const gfx::HDRStaticMetadata& input) {
    return input.max_avg;
  }
  static float min(const gfx::HDRStaticMetadata& input) { return input.min; }
  static uint8_t supported_eotf_mask(const gfx::HDRStaticMetadata& input) {
    return input.supported_eotf_mask;
  }

  static bool Read(gfx::mojom::HDRStaticMetadataDataView data,
                   gfx::HDRStaticMetadata* output);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_HDR_STATIC_METADATA_MOJOM_TRAITS_H_

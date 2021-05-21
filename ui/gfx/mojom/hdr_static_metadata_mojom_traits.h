// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_HDR_STATIC_METADATA_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_HDR_STATIC_METADATA_MOJOM_TRAITS_H_

#include "ui/gfx/hdr_static_metadata.h"
#include "ui/gfx/mojom/hdr_static_metadata.mojom.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::HDRStaticMetadataDataView,
                    gfx::HDRStaticMetadata> {
  static float max(const gfx::HDRStaticMetadata& input) { return input.max; }
  static float max_avg(const gfx::HDRStaticMetadata& input) {
    return input.max_avg;
  }
  static float min(const gfx::HDRStaticMetadata& input) { return input.min; }

  static bool Read(gfx::mojom::HDRStaticMetadataDataView data,
                   gfx::HDRStaticMetadata* output);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_HDR_STATIC_METADATA_MOJOM_TRAITS_H_

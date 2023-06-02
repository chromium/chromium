// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/hdr_static_metadata_mojom_traits.h"

namespace mojo {

bool StructTraits<
    gfx::mojom::HDRStaticMetadataDataView,
    gfx::HDRStaticMetadata>::Read(gfx::mojom::HDRStaticMetadataDataView data,
                                  gfx::HDRStaticMetadata* output) {
  output->max = data.max();
  output->max_avg = data.max_avg();
  output->min = data.min();
  output->supported_eotf_mask = data.supported_eotf_mask();
  return true;
}

}  // namespace mojo

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_static_metadata.h"

namespace gfx {

HDRStaticMetadata::HDRStaticMetadata() = default;
HDRStaticMetadata::HDRStaticMetadata(double max,
                                     double max_avg,
                                     double min,
                                     uint8_t supported_eotf_mask)
    : max(max),
      max_avg(max_avg),
      min(min),
      supported_eotf_mask(supported_eotf_mask) {}
HDRStaticMetadata::HDRStaticMetadata(const HDRStaticMetadata& rhs) = default;
HDRStaticMetadata& HDRStaticMetadata::operator=(const HDRStaticMetadata& rhs) =
    default;

}  // namespace gfx

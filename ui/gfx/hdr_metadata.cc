// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_metadata.h"

namespace gfx {

ColorVolumeMetadata::ColorVolumeMetadata() = default;
ColorVolumeMetadata::ColorVolumeMetadata(const ColorVolumeMetadata& rhs) =
    default;
ColorVolumeMetadata& ColorVolumeMetadata::operator=(
    const ColorVolumeMetadata& rhs) = default;

HDRMetadata::HDRMetadata() = default;
HDRMetadata::HDRMetadata(const HDRMetadata& rhs) = default;
HDRMetadata& HDRMetadata::operator=(const HDRMetadata& rhs) = default;

}  // namespace gfx

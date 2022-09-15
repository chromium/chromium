// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ca_layer_params.h"

namespace gfx {

CALayerParams::CALayerParams() {}
CALayerParams::~CALayerParams() = default;
CALayerParams::CALayerParams(CALayerParams&& params) = default;
CALayerParams::CALayerParams(const CALayerParams& params) = default;
CALayerParams& CALayerParams::operator=(CALayerParams&& params) = default;
CALayerParams& CALayerParams::operator=(const CALayerParams& params) = default;

}  // namespace gfx

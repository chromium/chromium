// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_layer_id.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace gfx {

OverlayLayerId::OverlayLayerId() = default;
OverlayLayerId::OverlayLayerId(const NamespaceId& layer_namespace_id,
                               uint32_t layer_id)
    : layer_namespace_id_(layer_namespace_id), layer_id_(layer_id) {
  CHECK(layer_namespace_id_.first != 0 || layer_namespace_id_.second != 0);
}
OverlayLayerId::~OverlayLayerId() = default;

// static
OverlayLayerId OverlayLayerId::MakeForVizInternal(uint32_t layer_id) {
  CHECK_NE(0u, layer_id);
  OverlayLayerId overlay_layer_id;
  overlay_layer_id.layer_id_ = layer_id;
  return overlay_layer_id;
}

std::string OverlayLayerId::ToString() const {
  return base::StringPrintf("%d:%d:%d", layer_namespace_id_.first,
                            layer_namespace_id_.second, layer_id_);
}

}  // namespace gfx

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"

#include "base/check.h"

namespace ui {

WaylandZcrCursorShapes::WaylandZcrCursorShapes(
    zcr_cursor_shapes_v1* zcr_cursor_shapes,
    WaylandConnection* connection)
    : zcr_cursor_shapes_v1_(zcr_cursor_shapes), connection_(connection) {
  DCHECK(zcr_cursor_shapes_v1_);
  DCHECK(connection_);
}

WaylandZcrCursorShapes::~WaylandZcrCursorShapes() = default;

}  // namespace ui

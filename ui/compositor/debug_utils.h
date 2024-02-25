// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_DEBUG_UTILS_H_
#define UI_COMPOSITOR_DEBUG_UTILS_H_

#include <sstream>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/compositor_export.h"

namespace gfx {
class Point;
}

namespace ui {

class Layer;

using DebugLayerChildCallback =
    base::RepeatingCallback<std::vector<raw_ptr<Layer, VectorExperimental>>(
        const Layer*)>;

// Log the layer hierarchy. Mark layers which contain |mouse_location| with '*'.
COMPOSITOR_EXPORT void PrintLayerHierarchy(const Layer* layer,
                                           const gfx::Point& mouse_location);

// Print the layer hierarchy to |out|. Mark layers which contain
// |mouse_location| with '*'.
COMPOSITOR_EXPORT void PrintLayerHierarchy(
    const Layer* layer,
    const gfx::Point& mouse_location,
    std::ostringstream* out,
    DebugLayerChildCallback child_cb = DebugLayerChildCallback());

}  // namespace ui

#endif  // UI_COMPOSITOR_DEBUG_UTILS_H_

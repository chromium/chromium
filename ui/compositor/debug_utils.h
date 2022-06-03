// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_DEBUG_UTILS_H_
#define UI_COMPOSITOR_DEBUG_UTILS_H_

#include <sstream>

#include "ui/compositor/compositor_export.h"

namespace gfx {
class Point;
}

namespace ui {

class Layer;

// Log the layer hierarchy. Mark layers which contain |mouse_location| with '*'.
COMPOSITOR_EXPORT void PrintLayerHierarchy(const Layer* layer,
                                           const gfx::Point& mouse_location);

// Print the layer hierarchy to |out|. Mark layers which contain
// |mouse_location| with '*'.
COMPOSITOR_EXPORT void PrintLayerHierarchy(const Layer* layer,
                                           const gfx::Point& mouse_location,
                                           std::ostringstream* out);

}  // namespace ui

#endif  // UI_COMPOSITOR_DEBUG_UTILS_H_

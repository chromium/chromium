// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_USAGE_UTIL_H_
#define UI_GFX_BUFFER_USAGE_UTIL_H_

#include "base/component_export.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {

// Returns the name of |usage| as a string.
COMPONENT_EXPORT(GFX) const char* BufferUsageToString(BufferUsage usage);

}  // namespace gfx

#endif  // UI_GFX_BUFFER_USAGE_UTIL_H_

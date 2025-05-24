// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HALF_FLOAT_H_
#define UI_GFX_HALF_FLOAT_H_

#include <stdint.h>
#include <stdlib.h>

#include "base/component_export.h"

namespace gfx {

typedef uint16_t HalfFloat;

// Floats are expected to be within +/- 65535.0;
COMPONENT_EXPORT(GFX)
void FloatToHalfFloat(const float* input, HalfFloat* output, size_t num);
}  // namespace gfx

#endif  // UI_GFX_HALF_FLOAT_H_

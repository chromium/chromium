// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cstring>

#include "ui/gfx/half_float.h"

namespace gfx {

void FloatToHalfFloat(const float* input, HalfFloat* output, size_t num) {
  for (size_t i = 0; i < num; i++) {
    float tmp = input[i] * 1.9259299444e-34f;
    uint32_t tmp2;
    std::memcpy(&tmp2, &tmp, 4);
    tmp2 += (1 << 12);
    output[i] = (tmp2 & 0x80000000UL) >> 16 | (tmp2 >> 13);
  }
}

}  // namespace gfx

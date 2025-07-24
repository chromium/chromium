// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/half_float.h"

#include <cstring>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace gfx {

void FloatToHalfFloat(base::span<const float> input,
                      base::span<HalfFloat> output,
                      size_t spanification_suspected_redundant_num) {
  // TODO(crbug.com/431824301): Remove unneeded parameter once validated to be
  // redundant in M143.
  CHECK(spanification_suspected_redundant_num == input.size(),
        base::NotFatalUntil::M143);
  for (size_t i = 0; i < spanification_suspected_redundant_num; i++) {
    float tmp = input[i] * 1.9259299444e-34f;
    uint32_t tmp2;
    UNSAFE_TODO(std::memcpy(&tmp2, &tmp, 4));
    tmp2 += (1 << 12);
    output[i] = (tmp2 & 0x80000000UL) >> 16 | (tmp2 >> 13);
  }
}

}  // namespace gfx

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <string_view>

#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"

std::string_view ReturnOffsetIntoString(size_t offset) {
  // Contrived non-const to trigger arrayification.
  //
  // Expected rewrite:
  // std::array<char, 6> non_const_buffer{"hello"};
  static std::array<char, 6> non_const_buffer{"hello"};
  non_const_buffer[offset] = 'y';

  // Expected rewrite:
  // return std::string_view(
  //     base::span<char>(non_const_buffer).subspan(offset).data(),
  //     base::SpanificationSizeofForStdArray(non_const_buffer) - offset);
  return std::string_view(
      base::span<char>(non_const_buffer).subspan(offset).data(),
      base::SpanificationSizeofForStdArray(non_const_buffer) - offset);
}

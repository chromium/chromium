// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

namespace onnxruntime {

// the semantics of this enum should match std::endian from C++20
enum class endian {
#if defined(_WIN32)
  little = 0,
  big = 1,
  native = little,
#elif defined(__GNUC__) || defined(__clang__)
  little = __ORDER_LITTLE_ENDIAN__,
  big = __ORDER_BIG_ENDIAN__,
  native = __BYTE_ORDER__,
#else
#error onnxruntime::endian is not implemented in this environment.
#endif
};

static_assert(
    endian::native == endian::little || endian::native == endian::big,
    "Only little-endian or big-endian native byte orders are supported.");

}  // namespace onnxruntime

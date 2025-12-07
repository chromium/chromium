// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace nearby {

void RandBytes(void* bytes, size_t length) {
  base::RandBytes(
      // TODO(crbug.com/40284755): Convert all callers in Nearby to use spans
      // and remove this RandBytes overload.
      UNSAFE_TODO(base::span(static_cast<uint8_t*>(bytes), length)));
}

void RandBytes(absl::Span<uint8_t> bytes) {
  base::RandBytes(bytes);
}

}  // namespace nearby

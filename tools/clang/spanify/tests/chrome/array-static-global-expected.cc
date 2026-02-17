// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <tuple>

int UnsafeIndex();  // This function might return an out-of-bound index.

// Expected rewrite:
// static const auto kByteStringsUnlocalized =
//     std::to_array<const char*>({" B", " kB"});
static const auto kByteStringsUnlocalized =
    std::to_array<const char*>({" B", " kB"});

void fct() {
  std::ignore = kByteStringsUnlocalized[UnsafeIndex()];
}

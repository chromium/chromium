// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/rapidhash/rapidhash.h"

void HashesDataWithoutCrashing(std::string input) {
  uint64_t hash =
      rapidhash(reinterpret_cast<const uint8_t*>(input.data()), input.size());

  // Keep the compiler from optimizing away the above call.
  asm volatile("" ::"g"(hash));
}

FUZZ_TEST(rapidhash, HashesDataWithoutCrashing);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/fuzztest/init_helper.h"
#include "third_party/fuzztest/src/fuzztest/init_fuzztest.h"

static void RealInitFunction(int* argc, char*** argv) {
  fuzztest::ParseAbslFlags(*argc, *argv);
  fuzztest::InitFuzzTest(argc, argv);
}

// base/test:test_support is used in test suites containing fuzztests
// and those without. In those without, we want to avoid depending
// on fuzztest's complex dependencies, but on those with fuzztests
// we need to call InitFuzzTest. So, use a static initializer to fill
// in a function pointer in those cases.
class FuzztestInitializer {
 public:
  FuzztestInitializer() {
    fuzztest_init_helper::initialization_function = RealInitFunction;
  }
};

FuzztestInitializer static_initializer;

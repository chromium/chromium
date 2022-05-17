// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if USE_BRILLO
#include "brillo/test_helpers.h"
#else
#include "gtest/gtest.h"
#endif  // USE_BRILLO

// The entry point of the unit tests.
int main(int argc, char** argv) {
#if USE_BRILLO
  base::CommandLine::Init(argc, argv);
  SetUpTests(&argc, argv, true);
#else
  testing::InitGoogleTest(&argc, argv);
#endif
  return RUN_ALL_TESTS();
}

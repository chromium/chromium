// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "standalone/base/logging.h"
#include "standalone/base/stack_trace.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  // TODO(rockot): Implement basic command line parsing for standalone builds.
  ipcz::standalone::StackTrace::EnableStackTraceSymbolization(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "standalone/base/logging.h"
#include "standalone/base/stack_trace.h"
#include "test/multinode_test.h"
#include "test_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
#include "test/test_child_launcher.h"
#endif

int main(int argc, char** argv) {
  // TODO(rockot): Implement basic command line parsing for standalone builds.
  ipcz::standalone::StackTrace::EnableStackTraceSymbolization(argv[0]);
  testing::InitGoogleTest(&argc, argv);

  ipcz::test::RegisterMultinodeTests();

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
  ipcz::test::TestChildLauncher::Initialize(argc, argv);

  int exit_code;
  if (ipcz::test::TestChildLauncher::RunTestChild(exit_code)) {
    return exit_code;
  }
#endif

  return RUN_ALL_TESTS();
}

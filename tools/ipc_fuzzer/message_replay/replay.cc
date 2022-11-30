// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/at_exit.h"
#include "tools/ipc_fuzzer/message_replay/replay_process.h"

int main(int argc, const char** argv) {
  base::AtExitManager exit_manager;
  ipc_fuzzer::ReplayProcess replay;
  if (!replay.Initialize(argc, argv))
    return EXIT_FAILURE;

  replay.OpenChannel();

  if (!replay.OpenTestcase())
    return EXIT_FAILURE;

  replay.Run();
  return EXIT_SUCCESS;
}

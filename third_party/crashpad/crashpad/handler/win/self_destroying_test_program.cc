// Copyright 2015 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <malloc.h>
#include <stdlib.h>
#include <windows.h>
#include <winternl.h>

#include "base/logging.h"
#include "client/crashpad_client.h"
#include "snapshot/win/process_reader_win.h"

namespace crashpad {
namespace {

// Without this, clang optimizes away the _alloca below, which in turn
// makes the VirtualFree() crash with an access violation.
#if defined(__clang__)
#pragma clang optimize off
#endif

// We VirtualFree a region in ourselves (the stack) to confirm that the
// exception reporter captures as much as possible in the minidump and doesn't
// abort. __debugbreak() immediately after doing so because the process is
// clearly in a very broken state at this point.
__declspec(noinline) bool FreeOwnStackAndBreak() {
  ProcessReaderWin process_reader;
  if (!process_reader.Initialize(GetCurrentProcess(),
                                 ProcessSuspensionState::kRunning)) {
    LOG(ERROR) << "ProcessReaderWin Initialize";
    return false;
  }

  const std::vector<ProcessReaderWin::Thread> threads =
      process_reader.Threads();
  if (threads.empty()) {
    LOG(ERROR) << "no threads";
    return false;
  }

  // Push the stack up a bit so that hopefully the crash handler can succeed,
  // but won't be able to read the base of the stack.
  [[maybe_unused]] volatile void* do_not_optimize_away = _alloca(16384);

  // We can't succeed at MEM_RELEASEing this memory, but MEM_DECOMMIT is good
  // enough to make it inaccessible.
  if (!VirtualFree(reinterpret_cast<void*>(threads[0].stack_region_address),
                   100,
                   MEM_DECOMMIT)) {
    PLOG(ERROR) << "VirtualFree";
    return false;
  }

  // If the VirtualFree() succeeds, we may have already crashed. __debugbreak()
  // just to be sure.
  __debugbreak();

  return true;
}

__declspec(noinline) int SelfDestroyingMain(int argc, wchar_t* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %ls <server_pipe_name>\n", argv[0]);
    return EXIT_FAILURE;
  }

  CrashpadClient client;
  if (!client.SetHandlerIPCPipe(argv[1])) {
    LOG(ERROR) << "SetHandler";
    return EXIT_FAILURE;
  }

  if (!FreeOwnStackAndBreak())
    return EXIT_FAILURE;

  // This will never be reached. On success, we'll have crashed above, or
  // otherwise returned before here.
  return EXIT_SUCCESS;
}

#if defined(__clang__)
#pragma clang optimize on
#endif

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::SelfDestroyingMain(argc, argv);
}

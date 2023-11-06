// Copyright 2023 The Crashpad Authors
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

#include <string.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "client/crashpad_client.h"
#include "util/misc/paths.h"

#include <Windows.h>

// We set up a program that crashes with a heap corruption exception.
// STATUS_HEAP_CORRUPTION (0xC0000374 3221226356).
namespace crashpad {
namespace {

void HeapCorruptionCrash() {
  __try {
    HANDLE heap = ::HeapCreate(0, 0, 0);
    CHECK(heap);
    CHECK(HeapSetInformation(
        heap, HeapEnableTerminationOnCorruption, nullptr, 0));
    void* addr = ::HeapAlloc(heap, 0, 0x1000);
    CHECK(addr);
    // Corrupt heap header.
    char* addr_mutable = reinterpret_cast<char*>(addr);
    memset(addr_mutable - sizeof(addr), 0xCC, sizeof(addr));

    HeapFree(heap, 0, addr);
    HeapDestroy(heap);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // Heap corruption exception should never be caught.
    CHECK(false);
  }
  // Should never reach here.
  abort();
}

int CrashyMain(int argc, wchar_t* argv[]) {
  static CrashpadClient* client = new crashpad::CrashpadClient();

  if (argc == 2) {
    // We call this from end_to_end_test.py.
    if (!client->SetHandlerIPCPipe(argv[1])) {
      LOG(ERROR) << "SetHandler";
      return EXIT_FAILURE;
    }
  } else if (argc == 3) {
    // This is helpful for debugging.
    if (!client->StartHandler(base::FilePath(argv[1]),
                              base::FilePath(argv[2]),
                              base::FilePath(),
                              std::string(),
                              std::map<std::string, std::string>(),
                              std::vector<std::string>(),
                              false,
                              true)) {
      LOG(ERROR) << "StartHandler";
      return EXIT_FAILURE;
    }
    // Got to have a handler & registration.
    if (!client->WaitForHandlerStart(10000)) {
      LOG(ERROR) << "Handler failed to start";
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %ls <server_pipe_name>\n", argv[0]);
    fprintf(stderr, "       %ls <handler_path> <database_path>\n", argv[0]);
    return EXIT_FAILURE;
  }

  HeapCorruptionCrash();

  LOG(ERROR) << "Invalid type or exception failed.";
  return EXIT_FAILURE;
}

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::CrashyMain(argc, argv);
}

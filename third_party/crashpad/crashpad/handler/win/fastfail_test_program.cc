// Copyright 2022 The Crashpad Authors
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

// We set up a program that crashes with a CFG exception so must be built and
// linked with /guard:cf. We register the crashpad runtime exception helper
// module to intercept and trigger the crashpad handler. Note that Windows only
// loads the module in WerFault after the crash for Windows 10 >= 20h1 (19041).
namespace crashpad {
namespace {

// This function should not be on our stack as CFG prevents the modified
// icall from happening.
int CallRffeManyTimes() {
  RaiseFailFastException(nullptr, nullptr, 0);
  RaiseFailFastException(nullptr, nullptr, 0);
  RaiseFailFastException(nullptr, nullptr, 0);
  RaiseFailFastException(nullptr, nullptr, 0);
  return 1;
}

using FuncType = decltype(&CallRffeManyTimes);
void IndirectCall(FuncType* func) {
  // This code always generates CFG guards.
  (*func)();
}

void CfgCrash() {
  // Call into the middle of the Crashy function.
  FuncType func = reinterpret_cast<FuncType>(
      (reinterpret_cast<uintptr_t>(CallRffeManyTimes)) + 16);
  __try {
    // Generates a STATUS_STACK_BUFFER_OVERRUN exception if CFG triggers.
    IndirectCall(&func);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // CFG fast fail should never be caught.
    CHECK(false);
  }
  // Should only reach here if CFG is disabled.
  abort();
}

void FastFailCrash() {
  __fastfail(77);
}

int CrashyMain(int argc, wchar_t* argv[]) {
  static CrashpadClient* client = new crashpad::CrashpadClient();

  std::wstring type;
  if (argc == 3) {
    type = argv[2];
    // We call this from end_to_end_test.py.
    if (!client->SetHandlerIPCPipe(argv[1])) {
      LOG(ERROR) << "SetHandler";
      return EXIT_FAILURE;
    }
  } else if (argc == 4) {
    type = argv[3];
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
    fprintf(stderr, "Usage: %ls <server_pipe_name> [cf|ff]\n", argv[0]);
    fprintf(
        stderr, "       %ls <handler_path> <database_path> [cf|ff]\n", argv[0]);
    return EXIT_FAILURE;
  }

  base::FilePath exe_path;
  if (!Paths::Executable(&exe_path)) {
    LOG(ERROR) << "Failed getting path";
    return EXIT_FAILURE;
  }

  // Find the module.
  auto mod_path = exe_path.DirName().Append(L"crashpad_wer.dll");

  // Make sure it is registered in the registry.
  DWORD dwOne = 1;
  LSTATUS reg_res =
      RegSetKeyValueW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\Windows Error Reporting\\"
                      L"RuntimeExceptionHelperModules",
                      mod_path.value().c_str(),
                      REG_DWORD,
                      &dwOne,
                      sizeof(DWORD));
  if (reg_res != ERROR_SUCCESS) {
    LOG(ERROR) << "RegSetKeyValueW";
    return EXIT_FAILURE;
  }

  if (!client->RegisterWerModule(mod_path.value())) {
    LOG(ERROR) << "WerRegisterRuntimeExceptionModule";
    return EXIT_FAILURE;
  }

  // Some versions of python call SetErrorMode() which extends to children, and
  // prevents the WerFault infrastructure from running.
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

  if (type == L"cf")
    CfgCrash();
  if (type == L"ff")
    FastFailCrash();

  LOG(ERROR) << "Invalid type or exception failed.";
  return EXIT_FAILURE;
}

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::CrashyMain(argc, argv);
}

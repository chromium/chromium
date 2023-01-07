// Copyright 2019 The Crashpad Authors
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

#include <windows.h>

#include "util/win/loader_lock.h"

namespace {

bool g_loader_lock_detected = false;

}  // namespace

extern "C" {

__declspec(dllexport) bool LoaderLockDetected() {
  return g_loader_lock_detected;
}

}  // extern "C"

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      g_loader_lock_detected = crashpad::IsThreadInLoaderLock();
      break;
  }

  return TRUE;
}

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

// See:
// https://docs.microsoft.com/en-us/windows/win32/api/werapi/nf-werapi-werregisterruntimeexceptionmodule

#include "handler/win/wer/crashpad_wer.h"

#include <Windows.h>
#include <werapi.h>

// Functions that will be exported from the DLL.
extern "C" {
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  return true;
}

// PFN_WER_RUNTIME_EXCEPTION_EVENT
// pContext is the address of a crashpad::internal::WerRegistration in the
// target process.
HRESULT OutOfProcessExceptionEventCallback(
    PVOID pContext,
    const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    BOOL* pbOwnershipClaimed,
    PWSTR pwszEventName,
    PDWORD pchSize,
    PDWORD pdwSignatureCount) {
  static constexpr DWORD wanted_exceptions[] = {
      0xC0000602,  // STATUS_FAIL_FAST_EXCEPTION
      0xC0000409,  // STATUS_STACK_BUFFER_OVERRUN
  };
  // Default to not-claiming as bailing out is easier.
  *pbOwnershipClaimed = FALSE;
  bool result = crashpad::wer::ExceptionEvent(
      wanted_exceptions,
      sizeof(wanted_exceptions) / sizeof(wanted_exceptions[0]),
      pContext,
      pExceptionInformation);

  if (result) {
    *pbOwnershipClaimed = TRUE;
    // Technically we failed as we terminated the process.
    return E_FAIL;
  }
  // Pass.
  return S_OK;
}

// PFN_WER_RUNTIME_EXCEPTION_EVENT_SIGNATURE
HRESULT OutOfProcessExceptionEventSignatureCallback(
    PVOID pContext,
    const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    DWORD dwIndex,
    PWSTR pwszName,
    PDWORD pchName,
    PWSTR pwszValue,
    PDWORD pchValue) {
  // We handle everything in the call to OutOfProcessExceptionEventCallback.
  // This function should never be called.
  return E_FAIL;
}

// PFN_WER_RUNTIME_EXCEPTION_DEBUGGER_LAUNCH
HRESULT OutOfProcessExceptionEventDebuggerLaunchCallback(
    PVOID pContext,
    const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    PBOOL pbIsCustomDebugger,
    PWSTR pwszDebuggerLaunch,
    PDWORD pchDebuggerLaunch,
    PBOOL pbIsDebuggerAutolaunch) {
  // We handle everything in the call to OutOfProcessExceptionEventCallback.
  // This function should never be called.
  return E_FAIL;
}
}  // extern "C"

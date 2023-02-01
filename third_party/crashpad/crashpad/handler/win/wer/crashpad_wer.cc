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

#include "util/misc/address_types.h"
#include "util/win/registration_protocol_win_structs.h"

#include <Windows.h>
#include <werapi.h>

namespace crashpad::wer {
namespace {
using crashpad::WerRegistration;

// bIsFatal and dwReserved fields are not present in SDK < 19041.
struct WER_RUNTIME_EXCEPTION_INFORMATION_19041 {
  DWORD dwSize;
  HANDLE hProcess;
  HANDLE hThread;
  EXCEPTION_RECORD exceptionRecord;
  CONTEXT context;
  PCWSTR pwszReportId;
  BOOL bIsFatal;
  DWORD dwReserved;
};

// We have our own version of this to avoid pulling in //base.
class ScopedHandle {
 public:
  ScopedHandle() : handle_(INVALID_HANDLE_VALUE) {}
  ScopedHandle(HANDLE from) : handle_(from) {}
  ~ScopedHandle() {
    if (IsValid())
      CloseHandle(handle_);
  }
  bool IsValid() {
    if (handle_ == INVALID_HANDLE_VALUE || handle_ == 0)
      return false;
    return true;
  }
  HANDLE Get() { return handle_; }

 private:
  HANDLE handle_;
};

ScopedHandle DuplicateFromTarget(HANDLE target_process, HANDLE target_handle) {
  HANDLE hTmp;
  if (!DuplicateHandle(target_process,
                       target_handle,
                       GetCurrentProcess(),
                       &hTmp,
                       SYNCHRONIZE | EVENT_MODIFY_STATE,
                       false,
                       0)) {
    return ScopedHandle();
  }
  return ScopedHandle(hTmp);
}

bool ProcessException(const DWORD* handled_exceptions,
                      size_t num_handled_exceptions,
                      const PVOID pContext,
                      const PWER_RUNTIME_EXCEPTION_INFORMATION e_info) {
  // Need to have been given a context.
  if (!pContext)
    return false;

  // Older OSes might provide a smaller structure than SDK 19041 defines.
  if (e_info->dwSize <=
      offsetof(WER_RUNTIME_EXCEPTION_INFORMATION_19041, bIsFatal)) {
    return false;
  }

  // If building with SDK < 19041 then the bIsFatal field isn't defined, so
  // use our internal definition here.
  if (!reinterpret_cast<const WER_RUNTIME_EXCEPTION_INFORMATION_19041*>(e_info)
           ->bIsFatal) {
    return false;
  }

  // Only deal with exceptions that crashpad would not have handled.
  bool found = false;
  for (size_t i = 0; i < num_handled_exceptions; i++) {
    if (handled_exceptions[i] == e_info->exceptionRecord.ExceptionCode) {
      found = true;
      break;
    }
  }
  // If num_handled_exceptions == 0, all exceptions should be passed on.
  if (!found && num_handled_exceptions != 0)
    return false;

  // Grab out the handles to the crashpad server.
  WerRegistration target_registration = {};
  if (!ReadProcessMemory(e_info->hProcess,
                         pContext,
                         &target_registration,
                         sizeof(target_registration),
                         nullptr)) {
    return false;
  }

  // Validate version of registration struct.
  if (target_registration.version != WerRegistration::kWerRegistrationVersion)
    return false;

  // Dupe handles for triggering the dump.
  auto dump_start = DuplicateFromTarget(
      e_info->hProcess, target_registration.dump_without_crashing);
  auto dump_done =
      DuplicateFromTarget(e_info->hProcess, target_registration.dump_completed);

  if (!dump_start.IsValid() || !dump_done.IsValid())
    return false;

  // It's possible that the target crashed while inside a DumpWithoutCrashing
  // call - either in the DumpWithoutCrashing call or in another thread - if so
  // we cannot trigger the dump until the first call's crash is dealth with as
  // the crashpad handler might be reading from structures we will write to. We
  // give the event a short while to be triggered and give up if it is not
  // signalled.
  if (target_registration.in_dump_without_crashing) {
    constexpr DWORD kOneSecondInMs = 1000;
    DWORD wait_result = WaitForSingleObject(dump_done.Get(), kOneSecondInMs);
    if (wait_result != WAIT_OBJECT_0)
      return false;
  }

  // Set up the crashpad handler's info structure.
  crashpad::ExceptionInformation target_non_crash_exception_info{};
  target_non_crash_exception_info.thread_id = GetThreadId(e_info->hThread);
  target_non_crash_exception_info.exception_pointers =
      static_cast<crashpad::VMAddress>(reinterpret_cast<uintptr_t>(pContext)) +
      offsetof(WerRegistration, pointers);

  if (!WriteProcessMemory(e_info->hProcess,
                          target_registration.crashpad_exception_info,
                          &target_non_crash_exception_info,
                          sizeof(target_non_crash_exception_info),
                          nullptr)) {
    return false;
  }

  // Write Exception & Context to the areas reserved by the client.
  if (!WriteProcessMemory(
          e_info->hProcess,
          reinterpret_cast<PVOID>(target_registration.pointers.ExceptionRecord),
          &e_info->exceptionRecord,
          sizeof(e_info->exceptionRecord),
          nullptr)) {
    return false;
  }
  if (!WriteProcessMemory(
          e_info->hProcess,
          reinterpret_cast<PVOID>(target_registration.pointers.ContextRecord),
          &e_info->context,
          sizeof(e_info->context),
          nullptr)) {
    return false;
  }

  // Request dump.
  if (!SetEvent(dump_start.Get()))
    return false;

  constexpr DWORD kTenSecondsInMs = 10 * 1000;
  DWORD result = WaitForSingleObject(dump_done.Get(), kTenSecondsInMs);

  if (result == WAIT_OBJECT_0) {
    // The handler signalled that it has written a dump, so we can terminate the
    // target - this takes over from WER, sorry WER.
    TerminateProcess(e_info->hProcess, e_info->exceptionRecord.ExceptionCode);
    return true;
  }
  // Maybe some other handler can have a go.
  return false;
}
}  // namespace

bool ExceptionEvent(
    const DWORD* handled_exceptions,
    size_t num_handled_exceptions,
    const PVOID pContext,
    const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation) {
  return ProcessException(handled_exceptions,
                          num_handled_exceptions,
                          pContext,
                          pExceptionInformation);
}

}  // namespace crashpad::wer

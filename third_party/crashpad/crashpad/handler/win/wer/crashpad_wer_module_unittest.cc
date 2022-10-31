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

#include "client/crashpad_client.h"

#include "base/files/file_path.h"
#include "gtest/gtest.h"
#include "test/test_paths.h"
#include "util/win/registration_protocol_win.h"

#include <Windows.h>
#include <werapi.h>

namespace crashpad {
namespace test {
namespace {
base::FilePath ModulePath() {
  auto dir = TestPaths::Executable().DirName();
  return dir.Append(FILE_PATH_LITERAL("crashpad_wer.dll"));
}

// Quick sanity check of the module, can't really test dumping etc. outside of
// WerFault.exe loading it.
TEST(CrashpadWerModule, Basic) {
  HRESULT res = 0;
  // Module loads.
  HMODULE hMod = LoadLibraryW(ModulePath().value().c_str());
  ASSERT_TRUE(hMod);

  // Required functions exist.
  auto wref = reinterpret_cast<PFN_WER_RUNTIME_EXCEPTION_EVENT>(
      GetProcAddress(hMod, WER_RUNTIME_EXCEPTION_EVENT_FUNCTION));
  ASSERT_TRUE(wref);
  auto wrees = reinterpret_cast<PFN_WER_RUNTIME_EXCEPTION_EVENT_SIGNATURE>(
      GetProcAddress(hMod, WER_RUNTIME_EXCEPTION_EVENT_SIGNATURE_FUNCTION));
  ASSERT_TRUE(wrees);
  auto wredl = reinterpret_cast<PFN_WER_RUNTIME_EXCEPTION_DEBUGGER_LAUNCH>(
      GetProcAddress(hMod, WER_RUNTIME_EXCEPTION_DEBUGGER_LAUNCH));
  ASSERT_TRUE(wredl);

  // Not-implemented functions return E_FAIL as expected.
  res = wrees(nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, E_FAIL);
  res = wredl(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, E_FAIL);

  // Dummy args for OutOfProcessExceptionEventCallback.
  WER_RUNTIME_EXCEPTION_INFORMATION wer_ex;
  wer_ex.dwSize = sizeof(WER_RUNTIME_EXCEPTION_INFORMATION);
  BOOL bClaimed = FALSE;

  // No context => skip.
  res = wref(nullptr, &wer_ex, &bClaimed, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, S_OK);
  ASSERT_EQ(bClaimed, FALSE);

  // Following tests only make sense if building on SDK >= 19041 as
  // bIsFatalField only exists after that.
#if defined(NTDDI_WIN10_VB) && (WDK_NTDDI_VERSION >= NTDDI_WIN10_VB)
  crashpad::WerRegistration registration;
  // Non-fatal exceptions are skipped.
  wer_ex.bIsFatal = FALSE;
  res = wref(&registration, &wer_ex, &bClaimed, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, S_OK);
  ASSERT_EQ(bClaimed, FALSE);

  // Fatal exception with unhandled code is skipped.
  wer_ex.bIsFatal = TRUE;
  wer_ex.exceptionRecord.ExceptionCode = 0xc0000005;
  res = wref(&registration, &wer_ex, &bClaimed, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, S_OK);
  ASSERT_EQ(bClaimed, FALSE);
#endif  // defined(NTDDI_WIN10_VB) && WDK_NTDDI_VERSION >= NTDDI_WIN10_VB
  FreeLibrary(hMod);
}

}  // namespace
}  // namespace test
}  // namespace crashpad

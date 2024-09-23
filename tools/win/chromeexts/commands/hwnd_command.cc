// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/commands/hwnd_command.h"

#include <windows.h>

#include <dbgeng.h>

#include "base/strings/utf_string_conversions.h"

namespace tools {
namespace win {
namespace chromeexts {

namespace {
constexpr size_t kMaxWindowStringLength = 256;
}  // namespace

HwndCommand::HwndCommand() = default;

HwndCommand::~HwndCommand() = default;

HRESULT HwndCommand::Execute() {
  auto remaining_arguments = command_line().GetArgs();
  if (remaining_arguments.size() != 1) {
    Printf("Only expected 1 argument. Got %d instead.\n",
           remaining_arguments.size());
    return E_FAIL;
  }

  std::string hwnd_expression = base::WideToASCII(remaining_arguments[0]);

  // While sizeof(HWND) can change between 32-bit and 64-bit platforms, Windows
  // only cares about the lower 32-bits. We evaluate as 64-bit as a convenience
  // and truncate the displayed hwnds to 32-bit below.
  // See https://msdn.microsoft.com/en-us/library/aa384203.aspx
  DEBUG_VALUE value;
  HRESULT hr = GetDebugClientAs<IDebugControl>()->Evaluate(
      hwnd_expression.c_str(), DEBUG_VALUE_INT64, &value, nullptr);
  if (FAILED(hr)) {
    PrintErrorf("Unable to evaluate %s\n", hwnd_expression.c_str());
    return hr;
  }

  HWND hwnd = reinterpret_cast<HWND>(value.I64);
  if (!IsWindow(hwnd)) {
    PrintErrorf("Not a window: %s\n", hwnd_expression.c_str());
    return E_FAIL;
  }

  wchar_t title[kMaxWindowStringLength];
  GetWindowText(hwnd, title, ARRAYSIZE(title));
  Printf("Title: %ws\n", title);
  wchar_t window_class[kMaxWindowStringLength];
  GetClassName(hwnd, window_class, ARRAYSIZE(window_class));
  Printf("Class: %ws\n", window_class);
  Printf("Hierarchy: \n");
  Printf("   Owner: %08x Parent: %08x\n", GetWindow(hwnd, GW_OWNER),
         GetParent(hwnd));
  Printf("   Prev:  %08x Next:   %08x\n", GetNextWindow(hwnd, GW_HWNDPREV),
         GetNextWindow(hwnd, GW_HWNDNEXT));
  Printf("Styles: %08x (Ex: %08x)\n", GetWindowLong(hwnd, GWL_STYLE),
         GetWindowLong(hwnd, GWL_EXSTYLE));
  RECT window_rect;
  if (GetWindowRect(hwnd, &window_rect)) {
    Printf("Bounds: (%d, %d) %dx%d\n", window_rect.left, window_rect.top,
           window_rect.right - window_rect.left,
           window_rect.bottom - window_rect.top);
  } else {
    DWORD last_error = GetLastError();
    PrintErrorf("Bounds: Unavailable (Last Error = %d)\n", last_error);
  }
  return S_OK;
}

}  // namespace chromeexts
}  // namespace win
}  // namespace tools

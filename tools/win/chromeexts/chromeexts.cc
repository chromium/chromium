// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbgeng.h>
#include <wrl/client.h>

#include "tools/win/chromeexts/chrome_exts_command.h"
#include "tools/win/chromeexts/commands/crash_info_command.h"
#include "tools/win/chromeexts/commands/gwp_asan_command.h"
#include "tools/win/chromeexts/commands/hwnd_command.h"
#include "tools/win/chromeexts/commands/view_command.h"

namespace {
using Microsoft::WRL::ComPtr;
}  // namespace

HRESULT CALLBACK DebugExtensionInitialize(ULONG* version, ULONG* flags) {
  *version = DEBUG_EXTENSION_VERSION(0, 1);
  *flags = 0;
  return S_OK;
}

void CALLBACK DebugExtensionUninitialize() {}

HRESULT CALLBACK help(IDebugClient* client, PCSTR args) {
  ComPtr<IDebugControl> debug_control;
  HRESULT hr = client->QueryInterface(IID_PPV_ARGS(&debug_control));
  if (FAILED(hr)) {
    return hr;
  }

  debug_control->Output(DEBUG_OUTPUT_NORMAL,
                        "Chrome Windows Debugger Extension\n");
  debug_control->Output(DEBUG_OUTPUT_NORMAL,
                        "hwnd - Displays basic hwnd info.\n");
  debug_control->Output(DEBUG_OUTPUT_NORMAL,
                        "crashinfo - Displays info from a crashpad dump.\n");
  debug_control->Output(
      DEBUG_OUTPUT_NORMAL,
      "gwpasan - Displays info from a GWP-ASan dump.\n Usage: gwpasan "
      "\"[<chrome binary path>;<llvm-symbolizer path>]\"");
  return S_OK;
}

HRESULT CALLBACK RunHwndCommand(IDebugClient* client, PCSTR args) {
  return tools::win::chromeexts::ChromeExtsCommand::Run<
      tools::win::chromeexts::HwndCommand>(client, args);
}

HRESULT CALLBACK RunViewCommand(IDebugClient* client, PCSTR args) {
  return tools::win::chromeexts::ChromeExtsCommand::Run<
      tools::win::chromeexts::ViewCommand>(client, args);
}

HRESULT CALLBACK RunCrashInfoCommand(IDebugClient* client, PCSTR args) {
  return tools::win::chromeexts::ChromeExtsCommand::Run<
      tools::win::chromeexts::CrashInfoCommand>(client, args);
}

HRESULT CALLBACK RunGwpAsanCommand(IDebugClient* client, PCSTR args) {
  return tools::win::chromeexts::ChromeExtsCommand::Run<
      tools::win::chromeexts::GwpAsanCommand>(client, args);
}

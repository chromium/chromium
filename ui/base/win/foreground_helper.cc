// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/foreground_helper.h"

#include <ostream>

#include "base/logging.h"
#include "ui/gfx/win/window_impl.h"

namespace ui {

ForegroundHelper::ForegroundHelper() : window_(NULL) {}
ForegroundHelper::~ForegroundHelper() = default;

// static
HRESULT ForegroundHelper::SetForeground(HWND window) {
  DCHECK(::IsWindow(window));
  ForegroundHelper foreground_helper;
  return foreground_helper.ForegroundHotKey(window);
}

HRESULT ForegroundHelper::ForegroundHotKey(HWND window) {
  // This implementation registers a hot key (F22) and then
  // triggers the hot key.  When receiving the hot key, we'll
  // be in the foreground and allowed to move the target window
  // into the foreground too.

  set_window_style(WS_POPUP);
  Init(NULL, gfx::Rect());

  static const int kHotKeyId = 0x0000baba;
  static const int kHotKeyWaitTimeout = 2000;

  // Store the target window into our USERDATA for use in our
  // HotKey handler.
  window_ = window;
  RegisterHotKey(hwnd(), kHotKeyId, 0, VK_F22);

  // If the calling thread is not yet a UI thread, call PeekMessage
  // to ensure creation of its message queue.
  MSG msg = {0};
  PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

  // Send the Hotkey.
  INPUT hotkey = {0};
  hotkey.type = INPUT_KEYBOARD;
  hotkey.ki.wVk =  VK_F22;
  if (1 != SendInput(1, &hotkey, sizeof(hotkey))) {
    LOG(WARNING) << "Failed to send input; GetLastError(): " << GetLastError();
    return E_FAIL;
  }

  // There are scenarios where the WM_HOTKEY is not dispatched by the
  // the corresponding foreground thread. To prevent us from indefinitely
  // waiting for the hotkey, we set a timer and exit the loop.
  SetTimer(hwnd(), kHotKeyId, kHotKeyWaitTimeout, NULL);

  // Loop until we get the key or the timer fires.
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    if (WM_HOTKEY == msg.message)
      break;
    if (WM_TIMER == msg.message) {
      SetForegroundWindow(window);
      break;
    }
  }

  UnregisterHotKey(hwnd(), kHotKeyId);
  KillTimer(hwnd(), kHotKeyId);
  DestroyWindow(hwnd());

  return S_OK;
}

// Handle the registered Hotkey being pressed.
void ForegroundHelper::OnHotKey(int id, UINT vcode, UINT modifiers) {
  SetForegroundWindow(window_);
}

}  // namespace ui

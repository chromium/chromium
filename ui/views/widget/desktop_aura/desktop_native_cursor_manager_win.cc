// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager_win.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/windows_types.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace views {

namespace {

constexpr int kDefaultCursorSize = 32;

wm::NativeCursorManagerDelegate* g_delegate = nullptr;

}  // namespace

DesktopNativeCursorManagerWin::DesktopNativeCursorManagerWin() = default;

DesktopNativeCursorManagerWin::~DesktopNativeCursorManagerWin() {
  g_delegate = nullptr;
}

void DesktopNativeCursorManagerWin::SetSystemCursorSize() {
  DWORD cursor_base_size = 0;
  if (hkcu_cursor_regkey_.Valid() &&
      hkcu_cursor_regkey_.ReadValueDW(L"CursorBaseSize", &cursor_base_size) ==
          ERROR_SUCCESS) {
    int size = base::checked_cast<int>(cursor_base_size);
    system_cursor_size_ = gfx::Size(size, size);
  }

  // Report cursor size.
  DCHECK(g_delegate);
  g_delegate->CommitSystemCursorSize(system_cursor_size_);
}

void DesktopNativeCursorManagerWin::RegisterCursorRegkeyObserver() {
  if (!hkcu_cursor_regkey_.Valid()) {
    return;
  }

  hkcu_cursor_regkey_.StartWatching(base::BindOnce(
      [](DesktopNativeCursorManagerWin* manager) {
        manager->SetSystemCursorSize();
        // RegKey::StartWatching only provides one notification.
        // Reregistration is required to get future notifications.
        manager->RegisterCursorRegkeyObserver();
      },
      // It's safe to use |base::Unretained(this)| here, because |this| owns
      // the |hkcu_cursor_regkey_|, and the callback will be cancelled if
      // |hkcu_cursor_regkey_| is destroyed.
      base::Unretained(this)));
}

void DesktopNativeCursorManagerWin::InitCursorSizeObserver(
    wm::NativeCursorManagerDelegate* delegate) {
  DCHECK(!g_delegate);
  // DesktopNativeCursorManager(Win) operates as a singleton through
  // aura::client::SetCursorShapeClient().
  g_delegate = delegate;
  // Validity of this key is checked at time-of-use.
  (void)hkcu_cursor_regkey_.Open(HKEY_CURRENT_USER, L"Control Panel\\Cursors",
                                 KEY_READ | KEY_NOTIFY);
  system_cursor_size_ = gfx::Size(kDefaultCursorSize, kDefaultCursorSize);
  RegisterCursorRegkeyObserver();
  SetSystemCursorSize();
}

}  // namespace views

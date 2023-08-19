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

}  // namespace

DesktopNativeCursorManagerWin::DesktopNativeCursorManagerWin() = default;

DesktopNativeCursorManagerWin::~DesktopNativeCursorManagerWin() = default;

void DesktopNativeCursorManagerWin::SetSystemCursorSize(
    wm::NativeCursorManagerDelegate* delegate) {
  DWORD cursor_base_size = 0;
  if (hkcu_cursor_regkey_.Valid() &&
      hkcu_cursor_regkey_.ReadValueDW(L"CursorBaseSize", &cursor_base_size) ==
          ERROR_SUCCESS) {
    int size = base::checked_cast<int>(cursor_base_size);
    system_cursor_size_ = gfx::Size(size, size);
  }

  // Report cursor size.
  delegate->CommitSystemCursorSize(system_cursor_size_);
}

void DesktopNativeCursorManagerWin::RegisterCursorRegkeyObserver(
    wm::NativeCursorManagerDelegate* delegate) {
  if (!hkcu_cursor_regkey_.Valid())
    return;

  hkcu_cursor_regkey_.StartWatching(base::BindOnce(
      [](DesktopNativeCursorManagerWin* manager,
         wm::NativeCursorManagerDelegate* delegate) {
        manager->SetSystemCursorSize(delegate);
        // RegKey::StartWatching only provides one notification.
        // Reregistration is required to get future notifications.
        manager->RegisterCursorRegkeyObserver(delegate);
      },
      // It's safe to use |base::Unretained(this)| here, because |this| owns
      // the |hkcu_cursor_regkey_|, and the callback will be cancelled if
      // |hkcu_cursor_regkey_| is destroyed.
      base::Unretained(this), delegate));
}

void DesktopNativeCursorManagerWin::InitCursorSizeObserver(
    wm::NativeCursorManagerDelegate* delegate) {
  // Validity of this key is checked at time-of-use.
  (void)hkcu_cursor_regkey_.Open(HKEY_CURRENT_USER, L"Control Panel\\Cursors",
                                 KEY_READ | KEY_NOTIFY);
  system_cursor_size_ = gfx::Size(kDefaultCursorSize, kDefaultCursorSize);
  RegisterCursorRegkeyObserver(delegate);
  SetSystemCursorSize(delegate);
}

}  // namespace views

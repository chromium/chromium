// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/singleton_hwnd_hot_key_observer.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace gfx {

namespace {

base::flat_set<int>& GetUsedHotKeyIDs() {
  static base::NoDestructor<base::flat_set<int>> used_hot_key_ids;
  return *used_hot_key_ids;
}

std::optional<int> GetAvailableHotKeyID() {
  // Valid hot key IDs are in the range 0x0000 to 0xBFFF. See
  // https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-registerhotkey
  for (int i = 0x0000; i < 0xBFFF; i++) {
    if (!GetUsedHotKeyIDs().contains(i))
      return i;
  }
  return std::nullopt;
}

void SetHotKeyIDUsed(int id) {
  DCHECK(!GetUsedHotKeyIDs().contains(id));
  GetUsedHotKeyIDs().insert(id);
}

void SetHotKeyIDAvailable(int id) {
  DCHECK(GetUsedHotKeyIDs().contains(id));
  GetUsedHotKeyIDs().erase(id);
}

}  // anonymous namespace

std::unique_ptr<SingletonHwndHotKeyObserver>
SingletonHwndHotKeyObserver::Create(
    const SingletonHwndObserver::WndProc& wnd_proc,
    UINT key_code,
    int modifiers) {
  std::optional<int> hot_key_id = GetAvailableHotKeyID();

  // If there are no available hot key IDs, return null.
  if (!hot_key_id.has_value())
    return nullptr;

  // If we fail to register the hot key, return null. Most likely reason for
  // failure is that another application has already registered the hot key.
  if (!RegisterHotKey(gfx::SingletonHwnd::GetInstance()->hwnd(), *hot_key_id,
                      modifiers, key_code)) {
    return nullptr;
  }

  return base::WrapUnique(
      new SingletonHwndHotKeyObserver(wnd_proc, *hot_key_id));
}

SingletonHwndHotKeyObserver::SingletonHwndHotKeyObserver(
    const SingletonHwndObserver::WndProc& wnd_proc,
    int hot_key_id)
    : observer_(base::BindRepeating(&SingletonHwndHotKeyObserver::OnWndProc,
                                    base::Unretained(this))),
      wnd_proc_(wnd_proc),
      hot_key_id_(hot_key_id) {
  SetHotKeyIDUsed(hot_key_id);
}

SingletonHwndHotKeyObserver::~SingletonHwndHotKeyObserver() {
  bool success = !!UnregisterHotKey(gfx::SingletonHwnd::GetInstance()->hwnd(),
                                    hot_key_id_);
  // This call should always succeed, as long as we pass in the right HWND and
  // an id we've used to register before.
  DCHECK(success);
  SetHotKeyIDAvailable(hot_key_id_);
}

void SingletonHwndHotKeyObserver::OnWndProc(HWND hwnd,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam) {
  // Only propagate WM_HOTKEY messages for this particular hot key to the owner
  // of this observer.
  if (message == WM_HOTKEY && static_cast<int>(wparam) == hot_key_id_)
    wnd_proc_.Run(hwnd, message, wparam, lparam);
}

}  // namespace gfx

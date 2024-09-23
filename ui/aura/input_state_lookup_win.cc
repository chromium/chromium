// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/input_state_lookup_win.h"

#include <windows.h>

#include <winuser.h>

#include "base/memory/ptr_util.h"

namespace aura {

// static
std::unique_ptr<InputStateLookup> InputStateLookup::Create() {
  return base::WrapUnique(new InputStateLookupWin);
}

InputStateLookupWin::InputStateLookupWin() {
}

InputStateLookupWin::~InputStateLookupWin() {
}

bool InputStateLookupWin::IsMouseButtonDown() const {
  return (GetKeyState(VK_LBUTTON) & 0x80) ||
    (GetKeyState(VK_RBUTTON) & 0x80) ||
    (GetKeyState(VK_MBUTTON) & 0x80) ||
    (GetKeyState(VK_XBUTTON1) & 0x80) ||
    (GetKeyState(VK_XBUTTON2) & 0x80);
}

}  // namespace aura

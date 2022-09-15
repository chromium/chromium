// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_MESSAGE_BOX_WIN_H_
#define UI_BASE_WIN_MESSAGE_BOX_WIN_H_

#include <windows.h>

#include <string>

#include "base/component_export.h"

namespace ui {

// A wrapper around Windows' MessageBox function. Using a Chrome specific
// MessageBox function allows us to control certain RTL locale flags so that
// callers don't have to worry about adding these flags when running in a
// right-to-left locale.
COMPONENT_EXPORT(UI_BASE)
int MessageBox(HWND hwnd,
               const std::wstring& text,
               const std::wstring& caption,
               UINT flags);

}  // namespace ui

#endif  // UI_BASE_WIN_MESSAGE_BOX_WIN_H_

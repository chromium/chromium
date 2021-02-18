// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/win/win_cursor.h"

#include <windows.h>

namespace ui {

WinCursor::WinCursor(HCURSOR hcursor) {
  hcursor_ = hcursor;
}

WinCursor::~WinCursor() {
  DestroyIcon(hcursor_);
}

}  // namespace ui

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/win_cursor.h"

#include <windows.h>

#include "base/memory/scoped_refptr.h"

namespace ui {

// static
scoped_refptr<WinCursor> WinCursor::FromPlatformCursor(
    scoped_refptr<PlatformCursor> platform_cursor) {
  return base::WrapRefCounted(static_cast<WinCursor*>(platform_cursor.get()));
}

WinCursor::WinCursor(HCURSOR hcursor, bool should_destroy)
    : should_destroy_(should_destroy), hcursor_(hcursor) {}

WinCursor::~WinCursor() {
  // DestroyIcon shouldn't be used to destroy a shared icon:
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroyicon#remarks
  if (should_destroy_)
    DestroyIcon(hcursor_);
}

}  // namespace ui

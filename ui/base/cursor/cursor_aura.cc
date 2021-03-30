// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "ui/base/cursor/cursor_factory.h"

namespace ui {

void Cursor::RefCustomCursor() {
  if (platform_cursor_)
    CursorFactory::GetInstance()->RefImageCursor(platform_cursor_);
}

void Cursor::UnrefCustomCursor() {
  if (platform_cursor_)
    CursorFactory::GetInstance()->UnrefImageCursor(platform_cursor_);
}

}  // namespace ui

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_win.h"

#include <memory>

#include "base/logging.h"
#include "base/optional.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace ui {

std::unique_ptr<CursorLoader> CursorLoader::Create(bool use_platform_cursors) {
  return std::make_unique<CursorLoaderWin>();
}

CursorLoaderWin::CursorLoaderWin() {
}

CursorLoaderWin::~CursorLoaderWin() {
}

void CursorLoaderWin::UnloadCursors() {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::SetPlatformCursor(gfx::NativeCursor* cursor) {
  if (cursor->type() == mojom::CursorType::kCustom)
    return;

  if (cursor->platform()) {
    cursor->SetPlatformCursor(cursor->platform());
  } else {
    base::Optional<PlatformCursor> default_cursor =
        CursorFactory::GetInstance()->GetDefaultCursor(cursor->type());
    LOG_IF(ERROR, !default_cursor)
        << "Failed to load cursor " << cursor->type();
    cursor->SetPlatformCursor(default_cursor.value_or(nullptr));
  }
}

}  // namespace ui

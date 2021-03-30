// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_WIN_WIN_CURSOR_FACTORY_H_
#define UI_BASE_CURSOR_WIN_WIN_CURSOR_FACTORY_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/base/cursor/win/win_cursor.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class COMPONENT_EXPORT(UI_BASE_CURSOR) WinCursorFactory : public CursorFactory {
 public:
  WinCursorFactory();
  WinCursorFactory(const WinCursorFactory&) = delete;
  WinCursorFactory& operator=(const WinCursorFactory&) = delete;
  ~WinCursorFactory() override;

  // CursorFactory:
  base::Optional<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  PlatformCursor CreateImageCursor(mojom::CursorType type,
                                   const SkBitmap& bitmap,
                                   const gfx::Point& hotspot) override;
  void RefImageCursor(PlatformCursor cursor) override;
  void UnrefImageCursor(PlatformCursor cursor) override;

 private:
  std::map<mojom::CursorType, scoped_refptr<WinCursor>> default_cursors_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_WIN_WIN_CURSOR_FACTORY_H_

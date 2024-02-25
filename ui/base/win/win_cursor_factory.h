// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_WIN_CURSOR_FACTORY_H_
#define UI_BASE_WIN_WIN_CURSOR_FACTORY_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/win/win_cursor.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class COMPONENT_EXPORT(UI_BASE) WinCursorFactory : public CursorFactory {
 public:
  WinCursorFactory();
  WinCursorFactory(const WinCursorFactory&) = delete;
  WinCursorFactory& operator=(const WinCursorFactory&) = delete;
  ~WinCursorFactory() override;

  // CursorFactory:
  scoped_refptr<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  std::optional<CursorData> GetCursorData(mojom::CursorType) override;
  scoped_refptr<PlatformCursor> CreateImageCursor(mojom::CursorType type,
                                                  const SkBitmap& bitmap,
                                                  const gfx::Point& hotspot,
                                                  float scale) override;

 private:
  std::map<mojom::CursorType, scoped_refptr<WinCursor>> default_cursors_;
};

}  // namespace ui

#endif  // UI_BASE_WIN_WIN_CURSOR_FACTORY_H_

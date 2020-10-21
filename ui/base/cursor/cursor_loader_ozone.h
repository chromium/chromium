// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_OZONE_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_OZONE_H_

#include <map>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

namespace gfx {
class Point;
}

namespace ui {
class CursorFactory;

class COMPONENT_EXPORT(UI_BASE_CURSOR) CursorLoaderOzone : public CursorLoader {
 public:
  explicit CursorLoaderOzone(bool use_platform_cursors);
  ~CursorLoaderOzone() override;

  // CursorLoader overrides:
  void SetPlatformCursor(gfx::NativeCursor* cursor) override;

 private:
  // CursorLoader overrides:
  void UnloadCursors() override;

  void LoadImageCursor(mojom::CursorType id,
                       int resource_id,
                       const gfx::Point& hot);
  PlatformCursor CursorFromType(mojom::CursorType type);
  PlatformCursor LoadCursorFromAsset(mojom::CursorType type);

  // Whether to use cursors provided by the underlying platform (e.g. X11
  // cursors). If false or in the case of a failure, Chromium assets will be
  // used instead.
  const bool use_platform_cursors_;

  // Pointers are owned by ResourceBundle and must not be freed here.
  std::map<mojom::CursorType, PlatformCursor> image_cursors_;
  CursorFactory* factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CursorLoaderOzone);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_OZONE_H_

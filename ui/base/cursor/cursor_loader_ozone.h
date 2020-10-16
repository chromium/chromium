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

namespace ui {
class CursorFactory;

class COMPONENT_EXPORT(UI_BASE_CURSOR) CursorLoaderOzone : public CursorLoader {
 public:
  CursorLoaderOzone();
  ~CursorLoaderOzone() override;

  // CursorLoader overrides:
  void LoadImageCursor(mojom::CursorType id,
                       int resource_id,
                       const gfx::Point& hot) override;
  void LoadAnimatedCursor(mojom::CursorType id,
                          int resource_id,
                          const gfx::Point& hot,
                          int frame_delay_ms) override;
  void UnloadAll() override;
  void SetPlatformCursor(gfx::NativeCursor* cursor) override;

 private:
  PlatformCursor CursorFromType(mojom::CursorType type);
  PlatformCursor CreateFallbackCursor(mojom::CursorType type);

  // Pointers are owned by ResourceBundle and must not be freed here.
  std::map<mojom::CursorType, PlatformCursor> image_cursors_;
  CursorFactory* factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CursorLoaderOzone);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_OZONE_H_

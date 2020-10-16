// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_WIN_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_WIN_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/cursor/cursor_loader.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_CURSOR) CursorLoaderWin : public CursorLoader {
 public:
  CursorLoaderWin();
  ~CursorLoaderWin() override;

  // Overridden from CursorLoader:
  void LoadImageCursor(mojom::CursorType id,
                       int resource_id,
                       const gfx::Point& hot) override;
  void LoadAnimatedCursor(mojom::CursorType id,
                          int resource_id,
                          const gfx::Point& hot,
                          int frame_delay_ms) override;
  void UnloadAll() override;
  void SetPlatformCursor(gfx::NativeCursor* cursor) override;

  // Used to pass the cursor resource module name to the cursor loader. This is
  // typically used to load non system cursors.
  static void SetCursorResourceModule(const base::string16& module_name);

 private:
  DISALLOW_COPY_AND_ASSIGN(CursorLoaderWin);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_WIN_H_

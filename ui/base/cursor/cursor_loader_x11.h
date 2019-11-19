// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_X11_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_X11_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/cursor_theme_manager_linux.h"
#include "ui/base/cursor/cursor_theme_manager_linux_observer.h"
#include "ui/base/ui_base_export.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11.h"

namespace ui {

class UI_BASE_EXPORT CursorLoaderX11 : public CursorLoader,
                                       public CursorThemeManagerLinuxObserver {
 public:
  CursorLoaderX11();
  ~CursorLoaderX11() override;

  // Overridden from CursorLoader:
  void LoadImageCursor(CursorType id,
                       int resource_id,
                       const gfx::Point& hot) override;
  void LoadAnimatedCursor(CursorType id,
                          int resource_id,
                          const gfx::Point& hot,
                          int frame_delay_ms) override;
  void UnloadAll() override;
  void SetPlatformCursor(gfx::NativeCursor* cursor) override;

  const XcursorImage* GetXcursorImageForTest(CursorType id);

 protected:
  // CursorThemeManagerLinux:
  void OnCursorThemeNameChanged(const std::string& cursor_theme_name) override;
  void OnCursorThemeSizeChanged(int cursor_theme_size) override;

 private:
  struct ImageCursor {
    ImageCursor(XcursorImage* x_image,
                float scale,
                display::Display::Rotation rotation);
    ~ImageCursor();

    ::Cursor cursor;
    float scale;
    display::Display::Rotation rotation;
  };

  // Returns true if we have an image resource loaded for the |native_cursor|.
  bool IsImageCursor(gfx::NativeCursor native_cursor);

  // Loads a new cursor corresponding to |id|.
  ::Cursor CursorFromId(CursorType id);

  void ClearThemeCursors();

  XDisplay* display_;

  // A map from a cursor native type to X cursor.
  std::map<CursorType, ::Cursor> font_cursors_;

  // A map to hold all image cursors. It maps the cursor ID to the X Cursor, the
  // display's scale factor, and the display's rotation.
  std::map<CursorType, std::unique_ptr<ImageCursor>> image_cursors_;

  // A map to hold all animated cursors. It maps the cursor ID to the pair of
  // the X Cursor and the corresponding XcursorImages. We need a pointer to the
  // images so that we can free them on destruction.
  std::map<CursorType, std::pair<::Cursor, XcursorImages*>> animated_cursors_;

  const XScopedCursor invisible_cursor_;

  ScopedObserver<CursorThemeManagerLinux, CursorThemeManagerLinuxObserver>
      cursor_theme_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(CursorLoaderX11);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_X11_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_CURSOR_FACTORY_H_
#define UI_BASE_X_X11_CURSOR_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_theme_manager.h"
#include "ui/base/cursor/cursor_theme_manager_observer.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

namespace ui {
class X11Cursor;
class XCursorLoader;

// CursorFactoryOzone implementation for X11 cursors.
class COMPONENT_EXPORT(UI_BASE_X) X11CursorFactory
    : public CursorFactory,
      public CursorThemeManagerObserver {
 public:
  X11CursorFactory();
  X11CursorFactory(const X11CursorFactory&) = delete;
  X11CursorFactory& operator=(const X11CursorFactory&) = delete;
  ~X11CursorFactory() override;

  // CursorFactoryOzone:
  base::Optional<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  PlatformCursor CreateImageCursor(mojom::CursorType type,
                                   const SkBitmap& bitmap,
                                   const gfx::Point& hotspot) override;
  PlatformCursor CreateAnimatedCursor(mojom::CursorType type,
                                      const std::vector<SkBitmap>& bitmaps,
                                      const gfx::Point& hotspot,
                                      int frame_delay_ms) override;
  void RefImageCursor(PlatformCursor cursor) override;
  void UnrefImageCursor(PlatformCursor cursor) override;
  void ObserveThemeChanges() override;

 private:
  // CusorThemeManagerObserver:
  void OnCursorThemeNameChanged(const std::string& cursor_theme_name) override;
  void OnCursorThemeSizeChanged(int cursor_theme_size) override;

  void ClearThemeCursors();

  std::unique_ptr<XCursorLoader> cursor_loader_;

  // Loads/caches default cursor or returns cached version.
  scoped_refptr<X11Cursor> GetDefaultCursorInternal(mojom::CursorType type);

  // Holds a single instance of the invisible cursor. X11 has no way to hide
  // the cursor so an invisible cursor mimics that.
  scoped_refptr<X11Cursor> invisible_cursor_;

  std::map<mojom::CursorType, scoped_refptr<X11Cursor>> default_cursors_;

  base::ScopedObservation<CursorThemeManager, CursorThemeManagerObserver>
      cursor_theme_observation_{this};
};

}  // namespace ui

#endif  // UI_BASE_X_X11_CURSOR_FACTORY_H_

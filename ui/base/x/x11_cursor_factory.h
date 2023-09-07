// Copyright 2016 The Chromium Authors
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
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/linux/cursor_theme_manager_observer.h"

namespace ui {
class X11Cursor;
class XCursorLoader;

#if BUILDFLAG(IS_LINUX)
class LinuxUi;
#endif

// CursorFactory implementation for X11 cursors.
class COMPONENT_EXPORT(UI_BASE_X) X11CursorFactory
    : public CursorFactory,
      public CursorThemeManagerObserver {
 public:
  X11CursorFactory();
  X11CursorFactory(const X11CursorFactory&) = delete;
  X11CursorFactory& operator=(const X11CursorFactory&) = delete;
  ~X11CursorFactory() override;

  // CursorFactory:
  scoped_refptr<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  scoped_refptr<PlatformCursor> CreateImageCursor(mojom::CursorType type,
                                                  const SkBitmap& bitmap,
                                                  const gfx::Point& hotspot,
                                                  float scale) override;
  scoped_refptr<PlatformCursor> CreateAnimatedCursor(
      mojom::CursorType type,
      const std::vector<SkBitmap>& bitmaps,
      const gfx::Point& hotspot,
      float scale,
      base::TimeDelta frame_delay) override;
  void ObserveThemeChanges() override;

 private:
  // CusorThemeManagerObserver:
  void OnCursorThemeNameChanged(const std::string& cursor_theme_name) override;
  void OnCursorThemeSizeChanged(int cursor_theme_size) override;

  void ClearThemeCursors();

  std::map<mojom::CursorType, scoped_refptr<X11Cursor>> default_cursors_;

  // `cursor_loader_` must be declared after `default_cursors_` since
  // initializing `cursor_loader_` will modify `default_cursors_`.
  std::unique_ptr<XCursorLoader> cursor_loader_;

#if BUILDFLAG(IS_LINUX)
  base::ScopedObservation<LinuxUi, CursorThemeManagerObserver>
      cursor_theme_observation_{this};
#endif
};

}  // namespace ui

#endif  // UI_BASE_X_X11_CURSOR_FACTORY_H_

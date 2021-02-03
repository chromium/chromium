// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "ui/base/cursor/cursor_theme_manager.h"
#include "ui/base/cursor/cursor_theme_manager_observer.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"

struct wl_cursor_theme;

namespace ui {

class WaylandConnection;

// CursorFactory implementation for Wayland.
class WaylandCursorFactory : public BitmapCursorFactoryOzone,
                             public CursorThemeManagerObserver,
                             public WaylandCursorBufferListener {
 public:
  explicit WaylandCursorFactory(WaylandConnection* connection);
  WaylandCursorFactory(const WaylandCursorFactory&) = delete;
  WaylandCursorFactory& operator=(const WaylandCursorFactory&) = delete;
  ~WaylandCursorFactory() override;

  // CursorFactory:
  void ObserveThemeChanges() override;

  // CursorFactoryOzone:
  base::Optional<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;

 protected:
  // Returns the actual wl_cursor record from the currently loaded theme.
  // Virtual for tests where themes can be empty.
  virtual wl_cursor* GetCursorFromTheme(const std::string& name);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandCursorFactoryTest,
                           RetainOldThemeUntilNewBufferIsAttached);

  struct ThemeData {
    ThemeData();
    ~ThemeData();
    wl::Object<wl_cursor_theme> theme;
    base::flat_map<mojom::CursorType, scoped_refptr<BitmapCursorOzone>> cache;
  };

  // CusorThemeManagerObserver:
  void OnCursorThemeNameChanged(const std::string& cursor_theme_name) override;
  void OnCursorThemeSizeChanged(int cursor_theme_size) override;

  // WaylandCursorBufferListener:
  void OnCursorBufferAttached(wl_cursor* cursor_data) override;

  void ReloadThemeCursors();
  void OnThemeLoaded(const std::string& loaded_theme_name,
                     int loaded_theme_size,
                     wl_cursor_theme* loaded_theme);

  WaylandConnection* const connection_;

  base::ScopedObservation<CursorThemeManager, CursorThemeManagerObserver>
      cursor_theme_observer_{this};

  // Name of the current theme.
  std::string name_;
  // Current size of cursors
  int size_ = 24;

  std::unique_ptr<ThemeData> current_theme_;
  // Holds the reference on the unloaded theme until the cursor is released.
  std::unique_ptr<ThemeData> unloaded_theme_;

  base::WeakPtrFactory<WaylandCursorFactory> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_

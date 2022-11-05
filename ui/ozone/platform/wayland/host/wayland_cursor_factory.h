// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "ui/linux/cursor_theme_manager_observer.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"

struct wl_cursor_theme;

namespace ui {

class BitmapCursor;
class LinuxUi;
class WaylandConnection;

// CursorFactory implementation for Wayland.
class WaylandCursorFactory : public BitmapCursorFactory,
                             public CursorThemeManagerObserver,
                             public WaylandCursorBufferListener {
 public:
  explicit WaylandCursorFactory(WaylandConnection* connection);
  WaylandCursorFactory(const WaylandCursorFactory&) = delete;
  WaylandCursorFactory& operator=(const WaylandCursorFactory&) = delete;
  ~WaylandCursorFactory() override;

  // CursorFactory:
  void ObserveThemeChanges() override;

  // CursorFactory:
  scoped_refptr<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  void SetDeviceScaleFactor(float scale) override;

 protected:
  // Returns the actual wl_cursor record from the currently loaded theme.
  // Virtual for tests where themes can be empty.
  virtual wl_cursor* GetCursorFromTheme(const std::string& name);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandCursorFactoryTest,
                           RetainOldThemeUntilNewBufferIsAttached);
  FRIEND_TEST_ALL_PREFIXES(WaylandCursorFactoryTest,
                           CachesSizesUntilThemeNameIsChanged);

  struct ThemeData {
    ThemeData();
    ~ThemeData();
    wl::Object<wl_cursor_theme> theme;
    base::flat_map<mojom::CursorType, scoped_refptr<BitmapCursor>> cache;
  };

  // CusorThemeManagerObserver:
  void OnCursorThemeNameChanged(const std::string& cursor_theme_name) override;
  void OnCursorThemeSizeChanged(int cursor_theme_size) override;

  // WaylandCursorBufferListener:
  void OnCursorBufferAttached(wl_cursor* cursor_data) override;

  // Returns the theme cached for the size and scale set currently.
  // May return nullptr, which means that the data is not yet loaded.
  ThemeData* GetCurrentTheme();
  // Resets the theme cache and triggers loading the theme again, optionally
  // keeping the existing data until the cursor changes next time.
  void ReloadThemeCursors();
  // Loads the theme with the current size and scale.  Does nothing if data
  // already exists.
  void MaybeLoadThemeCursors();
  void OnThemeLoaded(const std::string& loaded_theme_name,
                     int loaded_theme_size,
                     wl_cursor_theme* loaded_theme);

  const raw_ptr<WaylandConnection> connection_;

  base::ScopedObservation<LinuxUi, CursorThemeManagerObserver>
      cursor_theme_observer_{this};

  // Name of the current theme.
  std::string name_;
  // Current size of cursors
  int size_ = 24;
  // The current scale of the mouse cursor icon.
  float scale_ = 1.0f;

  // Maps sizes of the cursor to the cached shapes of those sizes.
  std::map<int, std::unique_ptr<ThemeData>> theme_cache_;
  // Holds the reference on the unloaded theme until the cursor is released.
  std::unique_ptr<ThemeData> unloaded_theme_;

  base::WeakPtrFactory<WaylandCursorFactory> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_

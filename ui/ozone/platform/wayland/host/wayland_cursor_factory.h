// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "ui/linux/cursor_theme_manager_observer.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_async_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"

struct wl_cursor_theme;

namespace ui {

class BitmapCursor;
class LinuxUi;
class WaylandConnection;

// CursorFactory implementation for Wayland.
//
// This CursorFactory implementation generates WaylandAsyncCursor objects, which
// are wrappers for BitmapCursor that allows cursor images to be asynchronously
// loaded.
//
// During normal operation, and assuming an empty cache, the sequence of
// functions that are called to actually get the cursor image where it needs to
// be goes as follows:
//
// - wl_cursor_theme_load is invoked to load the theme by the IO thread, which
// then invokes...
// - FinishThemeLoad on the UI thread, which invokes ThemeData::SetLoadedTheme,
// which then invokes its registered callbacks, the only one currently being...
// - FinishCursorLoad, which obtains the appropriate cursor image from the
// cursor theme and calls WaylandAsyncCursor::SetLoadedCursor on the cursor
// object that was returned earlier by GetDefaultCursor with the actual
// BitmapCursor.
//
// This will invoke all callbacks registered on the WaylandAsyncCursor object
// using WaylandAsyncCursor::OnCursorLoaded, with the only one currently being
// WaylandWindow::OnCursorLoaded which uses the obtained BitmapCursor object to
// set the cursor image on the window.
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
  scoped_refptr<PlatformCursor> GetDefaultCursor(mojom::CursorType type,
                                                 float scale) override;
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

 protected:
  // Returns the actual wl_cursor record from the currently loaded theme.
  // Virtual for tests where themes can be empty.
  virtual wl_cursor* GetCursorFromTheme(wl_cursor_theme* theme,
                                        const std::string& name);

 private:
  friend class DryRunningWaylandCursorFactory;
  friend class WaylandCursorFactoryTest;
  FRIEND_TEST_ALL_PREFIXES(WaylandCursorFactoryTest,
                           RetainOldThemeUntilNewBufferIsAttached);
  FRIEND_TEST_ALL_PREFIXES(WaylandCursorFactoryTest,
                           CachesSizesUntilThemeNameIsChanged);

  class ThemeData {
   public:
    using Callback = base::OnceCallback<void(wl_cursor_theme*)>;

    ThemeData();
    ~ThemeData();

    void AddThemeLoadedCallback(Callback callback);
    void SetLoadedTheme(wl_cursor_theme* theme);

    base::WeakPtr<ThemeData> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

    wl_cursor_theme* theme() { return theme_.get(); }

    base::flat_map<mojom::CursorType, scoped_refptr<WaylandAsyncCursor>> cache;

   private:
    bool loaded_ = false;
    wl::Object<wl_cursor_theme> theme_;
    std::vector<Callback> callbacks_;
    base::WeakPtrFactory<ThemeData> weak_factory_{this};
  };

  // Maps sizes of the cursor to the cached shapes of those sizes.
  using ThemeCache = std::map<int, std::unique_ptr<ThemeData>>;

  // CusorThemeManagerObserver:
  void OnCursorThemeNameChanged(const std::string& cursor_theme_name) override;
  void OnCursorThemeSizeChanged(int cursor_theme_size) override;

  // WaylandCursorBufferListener:
  void OnCursorBufferAttached(wl_cursor* cursor_data) override;

  // Returns the theme cache for the size set currently and the scale provided,
  // creating it if necessary.
  ThemeData* GetThemeForScale(float scale);
  // Resets the theme cache, causing it to be reloaded when new cursors are
  // loaded, optionally keeping the existing data until the cursor changes next
  // time if `force` is false.
  void FlushThemeCache(bool force);
  // Returns the final size of the cursor images in pixels for the current size
  // and provided scale.
  int GetScaledSize(float scale) const;
  void FinishThemeLoad(base::WeakPtr<ThemeData> cache_entry,
                       wl_cursor_theme* loaded_theme);
  void FinishCursorLoad(scoped_refptr<WaylandAsyncCursor> cursor,
                        mojom::CursorType type,
                        float scale,
                        wl_cursor_theme* loaded_theme);

  const raw_ptr<WaylandConnection> connection_;

  base::ScopedObservation<LinuxUi, CursorThemeManagerObserver>
      cursor_theme_observer_{this};

  // Name of the current theme.
  std::string name_;
  // Current size of cursors
  int size_ = 24;

  // The currently active cursor theme cache.
  std::unique_ptr<ThemeCache> theme_cache_;
  // Holds the reference on the unloaded theme cache until the cursor is
  // released.
  std::unique_ptr<ThemeCache> unloaded_theme_;

  base::WeakPtrFactory<WaylandCursorFactory> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_FACTORY_H_

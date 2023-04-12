// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor_factory.h"

#include <wayland-cursor.h>

#include <cmath>

#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "skia/ext/image_operations.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/linux/linux_ui.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {

// The threshold for rounding down the final scale of the cursor image
// that gets sent to the Wayland compositor. For instance, if the
// original cursor image scale is 1.2, we'll downscale it to 1.0. On
// the other hand, if it's something like 1.5 then we'll upscale it to 2.0.
const float kCursorScaleFlooringThreshold = 0.2;

float GetRoundedScale(float scale) {
  return std::ceil(scale - kCursorScaleFlooringThreshold);
}

}  // namespace

WaylandCursorFactory::ThemeData::ThemeData() = default;

WaylandCursorFactory::ThemeData::~ThemeData() = default;

WaylandCursorFactory::WaylandCursorFactory(WaylandConnection* connection)
    : connection_(connection) {
  connection_->SetCursorBufferListener(this);
  ReloadThemeCursors();
}

WaylandCursorFactory::~WaylandCursorFactory() = default;

void WaylandCursorFactory::ObserveThemeChanges() {
  auto* linux_ui = LinuxUi::instance();
  DCHECK(linux_ui);
  cursor_theme_observer_.Observe(linux_ui);
}

scoped_refptr<PlatformCursor> WaylandCursorFactory::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  // Wayland only supports cursor images with an integer scale, so we
  // must upscale cursor images with non-integer scales to integer scaled
  // images so that the cursor is displayed correctly.
  float rounded_scale = GetRoundedScale(scale_);
  if (std::abs(rounded_scale - scale_) >
          std::numeric_limits<float>::epsilon() &&
      !connection_->surface_submission_in_pixel_coordinates()) {
    const SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
        std::round(bitmap.width() * (rounded_scale / scale_)),
        std::round(bitmap.height() * (rounded_scale / scale_)));
    const gfx::Point scaled_hotspot =
        gfx::ScaleToRoundedPoint(hotspot, rounded_scale / scale_);
    return base::MakeRefCounted<BitmapCursor>(type, scaled_bitmap,
                                              scaled_hotspot, rounded_scale);
  } else {
    return BitmapCursorFactory::CreateImageCursor(type, bitmap, hotspot);
  }
}

scoped_refptr<PlatformCursor> WaylandCursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    base::TimeDelta frame_delay) {
  float rounded_scale = GetRoundedScale(scale_);
  if (std::abs(rounded_scale - scale_) >
          std::numeric_limits<float>::epsilon() &&
      !connection_->surface_submission_in_pixel_coordinates()) {
    std::vector<SkBitmap> scaled_bitmaps;
    for (const auto& bitmap : bitmaps) {
      scaled_bitmaps.push_back(skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
          std::round(bitmap.width() * (rounded_scale / scale_)),
          std::round(bitmap.height() * (rounded_scale / scale_))));
    }
    const gfx::Point scaled_hotspot =
        gfx::ScaleToRoundedPoint(hotspot, rounded_scale / scale_);
    return base::MakeRefCounted<BitmapCursor>(
        type, scaled_bitmaps, scaled_hotspot, frame_delay, rounded_scale);
  } else {
    return BitmapCursorFactory::CreateAnimatedCursor(type, bitmaps, hotspot,
                                                     frame_delay);
  }
}

scoped_refptr<PlatformCursor> WaylandCursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  auto* const current_theme = GetCurrentTheme();
  DCHECK(current_theme);
  if (current_theme->cache.count(type) == 0) {
    for (const std::string& name : CursorNamesFromType(type)) {
      wl_cursor* cursor = GetCursorFromTheme(name);
      if (!cursor)
        continue;

      current_theme->cache[type] = base::MakeRefCounted<BitmapCursor>(
          type, cursor,
          connection_->surface_submission_in_pixel_coordinates()
              ? scale_
              : GetRoundedScale(scale_));
      break;
    }
  }
  if (current_theme->cache.count(type) == 0)
    current_theme->cache[type] = nullptr;

  // Fall back to the base class implementation if the theme has't provided
  // a shape for the requested type.
  if (current_theme->cache[type].get() == nullptr)
    return BitmapCursorFactory::GetDefaultCursor(type);

  return current_theme->cache[type];
}

void WaylandCursorFactory::SetDeviceScaleFactor(float scale) {
  BitmapCursorFactory::SetDeviceScaleFactor(scale);

  if (scale_ == scale)
    return;

  scale_ = scale;
  MaybeLoadThemeCursors();
}

wl_cursor* WaylandCursorFactory::GetCursorFromTheme(const std::string& name) {
  auto* const current_theme = GetCurrentTheme();
  DCHECK(current_theme);

  // Possible if the theme could not be loaded.
  if (!current_theme->theme)
    return nullptr;

  return wl_cursor_theme_get_cursor(current_theme->theme.get(), name.c_str());
}

void WaylandCursorFactory::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  CHECK(!cursor_theme_name.empty());

  if (name_ == cursor_theme_name)
    return;

  name_ = cursor_theme_name;
  ReloadThemeCursors();
}

void WaylandCursorFactory::OnCursorThemeSizeChanged(int cursor_theme_size) {
  if (size_ == cursor_theme_size)
    return;

  size_ = cursor_theme_size;
  MaybeLoadThemeCursors();
}

void WaylandCursorFactory::OnCursorBufferAttached(wl_cursor* cursor_data) {
  if (!unloaded_theme_)
    return;
  if (!cursor_data) {
    unloaded_theme_.reset();
    return;
  }
  auto* const current_theme = GetCurrentTheme();
  DCHECK(current_theme);
  for (auto& item : current_theme->cache) {
    if (item.second->platform_data() == cursor_data) {
      // The cursor that has been just attached is from the current theme.  That
      // means that the theme that has been unloaded earlier can now be deleted.
      unloaded_theme_.reset();
      return;
    }
  }
}

WaylandCursorFactory::ThemeData* WaylandCursorFactory::GetCurrentTheme() {
  auto theme_it = theme_cache_.find(GetCacheKey());
  if (theme_it == theme_cache_.end())
    return nullptr;
  return theme_it->second.get();
}

void WaylandCursorFactory::ReloadThemeCursors() {
  auto* const current_theme = GetCurrentTheme();
  // If we use any cursor when the theme is reloaded, the one can be only from
  // the theme that is currently used.  As soon as we take the next cursor from
  // the next theme, we will destroy it (see OnCursorBufferAttached() above).
  // If more than one theme has been changed but we didn't take any cursors from
  // them (which is possible if the user played with settings but didn't switch
  // into Chromium), we don't need to track them all.
  if (!unloaded_theme_ && current_theme && current_theme->cache.size() > 0)
    unloaded_theme_ = std::move(theme_cache_[GetCacheKey()]);

  theme_cache_.clear();

  MaybeLoadThemeCursors();
}

void WaylandCursorFactory::MaybeLoadThemeCursors() {
  if (GetCurrentTheme())
    return;

  theme_cache_[GetCacheKey()] = std::make_unique<ThemeData>();

  // The task environment is normally not created in tests.  As this factory is
  // part of the platform that is created always and early, posting a task to
  // the pool would fail in many many tests.
  if (!base::ThreadPoolInstance::Get())
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(wl_cursor_theme_load, name_.c_str(), GetCacheKey(),
                     connection_->buffer_factory()->shm()),
      base::BindOnce(&WaylandCursorFactory::OnThemeLoaded,
                     weak_factory_.GetWeakPtr(), name_, size_));
}

int WaylandCursorFactory::GetCacheKey() const {
  if (connection_->surface_submission_in_pixel_coordinates()) {
    // When surface submission in pixel coordinates is enabled, true
    // fractional scaled cursors can be represented without scaling, so
    // load the cursor with its proper size.
    return base::checked_cast<int>(size_ * scale_);
  } else {
    return base::checked_cast<int>(size_ * GetRoundedScale(scale_));
  }
}

void WaylandCursorFactory::OnThemeLoaded(const std::string& loaded_theme_name,
                                         int loaded_theme_size,
                                         wl_cursor_theme* loaded_theme) {
  if (loaded_theme_name == name_) {
    // wl_cursor_theme_load() can return nullptr.  We don't check that here but
    // have to be cautious when we actually load the shape.
    auto* const current_theme = GetCurrentTheme();
    DCHECK(current_theme);
    current_theme->theme.reset(loaded_theme);
    current_theme->cache.clear();
    NotifyObserversOnThemeLoaded();
  }
}

}  // namespace ui

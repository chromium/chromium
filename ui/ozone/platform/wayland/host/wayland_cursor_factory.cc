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

void WaylandCursorFactory::ThemeData::AddThemeLoadedCallback(
    Callback callback) {
  if (loaded_) {
    std::move(callback).Run(theme_.get());
  } else {
    callbacks_.push_back(std::move(callback));
  }
}

void WaylandCursorFactory::ThemeData::SetLoadedTheme(wl_cursor_theme* theme) {
  DCHECK(!loaded_);
  theme_.reset(theme);
  loaded_ = true;
  for (auto& callback : callbacks_) {
    std::move(callback).Run(theme);
  }
  callbacks_.clear();
}

WaylandCursorFactory::WaylandCursorFactory(WaylandConnection* connection)
    : connection_(connection), theme_cache_(std::make_unique<ThemeCache>()) {
  connection_->SetCursorBufferListener(this);
}

WaylandCursorFactory::~WaylandCursorFactory() {
  connection_->SetCursorBufferListener(nullptr);
}

void WaylandCursorFactory::ObserveThemeChanges() {
  auto* linux_ui = LinuxUi::instance();
  DCHECK(linux_ui);
  cursor_theme_observer_.Observe(linux_ui);
}

scoped_refptr<PlatformCursor> WaylandCursorFactory::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot,
    float scale) {
  scoped_refptr<BitmapCursor> bitmap_cursor;

  // Wayland only supports cursor images with an integer scale, so we
  // must upscale cursor images with non-integer scales to integer scaled
  // images so that the cursor is displayed correctly.
  float rounded_scale = GetRoundedScale(scale);
  if (std::abs(rounded_scale - scale) > std::numeric_limits<float>::epsilon() &&
      !connection_->surface_submission_in_pixel_coordinates()) {
    const SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
        std::round(bitmap.width() * (rounded_scale / scale)),
        std::round(bitmap.height() * (rounded_scale / scale)));
    const gfx::Point scaled_hotspot =
        gfx::ScaleToRoundedPoint(hotspot, rounded_scale / scale);
    bitmap_cursor = base::MakeRefCounted<BitmapCursor>(
        type, scaled_bitmap, scaled_hotspot, rounded_scale);
  } else {
    bitmap_cursor =
        base::MakeRefCounted<BitmapCursor>(type, bitmap, hotspot, scale);
  }
  return base::MakeRefCounted<WaylandAsyncCursor>(bitmap_cursor);
}

scoped_refptr<PlatformCursor> WaylandCursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    float scale,
    base::TimeDelta frame_delay) {
  scoped_refptr<BitmapCursor> bitmap_cursor;

  float rounded_scale = GetRoundedScale(scale);
  if (std::abs(rounded_scale - scale) > std::numeric_limits<float>::epsilon() &&
      !connection_->surface_submission_in_pixel_coordinates()) {
    std::vector<SkBitmap> scaled_bitmaps;
    for (const auto& bitmap : bitmaps) {
      scaled_bitmaps.push_back(skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
          std::round(bitmap.width() * (rounded_scale / scale)),
          std::round(bitmap.height() * (rounded_scale / scale))));
    }
    const gfx::Point scaled_hotspot =
        gfx::ScaleToRoundedPoint(hotspot, rounded_scale / scale);
    bitmap_cursor = base::MakeRefCounted<BitmapCursor>(
        type, scaled_bitmaps, scaled_hotspot, frame_delay, rounded_scale);
  } else {
    bitmap_cursor = base::MakeRefCounted<BitmapCursor>(type, bitmaps, hotspot,
                                                       frame_delay, scale);
  }
  return base::MakeRefCounted<WaylandAsyncCursor>(bitmap_cursor);
}

void WaylandCursorFactory::FinishCursorLoad(
    scoped_refptr<WaylandAsyncCursor> cursor,
    mojom::CursorType type,
    float scale,
    wl_cursor_theme* loaded_theme) {
  for (const auto& name : CursorNamesFromType(type)) {
    wl_cursor* theme_cursor = GetCursorFromTheme(loaded_theme, name);
    if (theme_cursor) {
      cursor->SetBitmapCursor(base::MakeRefCounted<BitmapCursor>(
          type, theme_cursor,
          connection_->surface_submission_in_pixel_coordinates()
              ? scale
              : GetRoundedScale(scale)));
      return;
    }
  }
  // Fall back to the BitmapCursorFactory implementation if the theme has't
  // provided a shape for the requested type.
  cursor->SetBitmapCursor(BitmapCursor::FromPlatformCursor(
      BitmapCursorFactory::GetDefaultCursor(type)));
}

scoped_refptr<PlatformCursor> WaylandCursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  // Fall back to 1x for scale if not provided.
  return GetDefaultCursor(type, 1.0);
}

scoped_refptr<PlatformCursor> WaylandCursorFactory::GetDefaultCursor(
    mojom::CursorType type,
    float scale) {
  auto* const current_theme = GetThemeForScale(scale);
  DCHECK(current_theme);
  if (current_theme->cache.count(type) == 0) {
    auto async_cursor = base::MakeRefCounted<WaylandAsyncCursor>();
    current_theme->cache[type] = async_cursor;
    current_theme->AddThemeLoadedCallback(
        base::BindOnce(&WaylandCursorFactory::FinishCursorLoad,
                       weak_factory_.GetWeakPtr(), async_cursor, type, scale));
  }
  return current_theme->cache[type];
}

wl_cursor* WaylandCursorFactory::GetCursorFromTheme(wl_cursor_theme* theme,
                                                    const std::string& name) {
  // Possible if the theme could not be loaded.
  if (!theme) {
    return nullptr;
  }

  return wl_cursor_theme_get_cursor(theme, name.c_str());
}

void WaylandCursorFactory::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  CHECK(!cursor_theme_name.empty());

  if (name_ == cursor_theme_name)
    return;

  name_ = cursor_theme_name;
  FlushThemeCache(false);
}

void WaylandCursorFactory::OnCursorThemeSizeChanged(int cursor_theme_size) {
  size_ = cursor_theme_size;
}

void WaylandCursorFactory::OnCursorBufferAttached(wl_cursor* cursor_data) {
  if (!unloaded_theme_)
    return;
  if (!cursor_data) {
    unloaded_theme_.reset();
    return;
  }
  for (auto& item : *theme_cache_) {
    for (auto& subitem : item.second->cache) {
      auto bitmap_cursor = subitem.second->bitmap_cursor();
      if (bitmap_cursor && bitmap_cursor->platform_data() == cursor_data) {
        // The cursor that has been just attached is from the current theme.
        // That means that the theme that has been unloaded earlier can now be
        // deleted.
        unloaded_theme_.reset();
        return;
      }
    }
  }
}

WaylandCursorFactory::ThemeData* WaylandCursorFactory::GetThemeForScale(
    float scale) {
  auto item = theme_cache_->find(GetScaledSize(scale));
  if (item != theme_cache_->end()) {
    return item->second.get();
  }

  theme_cache_->insert_or_assign(GetScaledSize(scale),
                                 std::make_unique<ThemeData>());
  auto* cache_entry = theme_cache_->at(GetScaledSize(scale)).get();

  // The task environment is normally not created in tests.  As this factory is
  // part of the platform that is created always and early, posting a task to
  // the pool would fail in many many tests.
  if (!base::ThreadPoolInstance::Get())
    return cache_entry;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          wl_cursor_theme_load, name_.empty() ? nullptr : name_.c_str(),
          GetScaledSize(scale), connection_->buffer_factory()->shm()),
      base::BindOnce(&WaylandCursorFactory::FinishThemeLoad,
                     weak_factory_.GetWeakPtr(), cache_entry->GetWeakPtr()));

  return cache_entry;
}

void WaylandCursorFactory::FlushThemeCache(bool force) {
  size_t num_cursor_objects = 0;
  for (auto& entry : *theme_cache_) {
    num_cursor_objects += entry.second->cache.size();
  }
  if (force) {
    unloaded_theme_.reset();
  } else {
    // If we use any cursor when the theme is reloaded, the one can be only from
    // the theme that is currently used.  As soon as we take the next cursor
    // from the next theme, we will destroy it (see OnCursorBufferAttached()
    // above). If more than one theme has been changed but we didn't take any
    // cursors from them (which is possible if the user played with settings but
    // didn't switch into Chromium), we don't need to track them all.
    if (!unloaded_theme_ && num_cursor_objects != 0) {
      unloaded_theme_ = std::move(theme_cache_);
    }
  }
  theme_cache_ = std::make_unique<ThemeCache>();
}

int WaylandCursorFactory::GetScaledSize(float scale) const {
  if (connection_->surface_submission_in_pixel_coordinates()) {
    // When surface submission in pixel coordinates is enabled, true
    // fractional scaled cursors can be represented without scaling, so
    // load the cursor with its proper size.
    return base::checked_cast<int>(size_ * scale);
  } else {
    return base::checked_cast<int>(size_ * GetRoundedScale(scale));
  }
}

void WaylandCursorFactory::FinishThemeLoad(base::WeakPtr<ThemeData> cache_entry,
                                           wl_cursor_theme* loaded_theme) {
  // wl_cursor_theme_load() can return nullptr.  We don't check that here but
  // have to be cautious when we actually load the shape.
  if (cache_entry) {
    cache_entry->SetLoadedTheme(loaded_theme);
  }
}

}  // namespace ui

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_loader.h"

#include <map>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/wm/core/cursor_util.h"

namespace wm {

namespace {

using ::ui::mojom::CursorType;

constexpr base::TimeDelta kAnimatedCursorFrameDelay = base::Milliseconds(25);

// Converts DIPs (Device-independent pixels) to pixels.
int ConvertDipToPixel(int dip, float scale) {
  return dip * scale;
}

}  // namespace

CursorLoader::CursorLoader(bool use_platform_cursors)
    : use_platform_cursors_(use_platform_cursors),
      factory_(ui::CursorFactory::GetInstance()) {
  factory_->AddObserver(this);
}

CursorLoader::~CursorLoader() {
  factory_->RemoveObserver(this);
}

void CursorLoader::OnThemeLoaded() {
  UnloadCursors();
}

bool CursorLoader::SetDisplay(const display::Display& display) {
  const display::Display::Rotation rotation = display.panel_rotation();
  const float scale = display.device_scale_factor();
  if (rotation_ == rotation && scale_ == scale) {
    return false;
  }

  rotation_ = rotation;
  scale_ = scale;
  resource_scale_ = ui::GetScaleForResourceScaleFactor(
      ui::GetSupportedResourceScaleFactor(scale_));

  UnloadCursors();
  return true;
}

void CursorLoader::SetSize(ui::CursorSize size) {
  if (size_ == size)
    return;

  size_ = size;
  UnloadCursors();
}

void CursorLoader::SetLargeCursorSizeInDip(int large_cursor_size_in_dip) {
  if (large_cursor_size_in_dip_ == large_cursor_size_in_dip) {
    return;
  }

  large_cursor_size_in_dip_ = large_cursor_size_in_dip;
  UnloadCursors();
}

void CursorLoader::SetColor(SkColor color) {
  if (color_ == color) {
    return;
  }

  color_ = color;

  // Reset cursor lottie animation cache when new color needs to be applied.
  wm::ClearCursorAnimationCache();

  UnloadCursors();
}

void CursorLoader::SetPlatformCursor(ui::Cursor* cursor) {
  DCHECK(cursor);

  // The platform cursor was already set via WebCursor::GetNativeCursor.
  if (cursor->type() == CursorType::kCustom) {
    return;
  }

  cursor->SetPlatformCursor(CursorFromType(cursor->type()));
}

std::optional<ui::CursorData> CursorLoader::GetCursorData(
    const ui::Cursor& cursor) const {
  CursorType type = cursor.type();
  if (type == CursorType::kNone)
    return ui::CursorData();

  if (type == CursorType::kCustom) {
    auto cursor_data =
        ui::CursorData({cursor.custom_bitmap()}, cursor.custom_hotspot(),
                       cursor.image_scale_factor());
    ApplyColorAndLargeSize(cursor_data);
    return cursor_data;
  }

  if (use_platform_cursors_) {
    auto cursor_data = factory_->GetCursorData(type);
    if (cursor_data) {
      // TODO(crbug.com/40175364): consider either passing `scale_` to
      // `CursorFactory::GetCursorData`, or relying on having called
      // `CursorFactory::SetDeviceScaleFactor`, instead of setting it here.
      cursor_data->scale_factor = scale_;
      ApplyColorAndLargeSize(cursor_data.value());
      return cursor_data;
    }
  }

  const int large_cursor_size_in_px =
      ConvertDipToPixel(large_cursor_size_in_dip_, scale_);

  // TODO(crbug.com/40175364): use the actual `rotation_` if that makes
  // sense for the current use cases of `GetCursorData` (e.g. Chrome Remote
  // Desktop, WebRTC and VideoRecordingWatcher).
  return wm::GetCursorData(type, resource_scale_,
                           size_ == ui::CursorSize::kLarge
                               ? std::make_optional(large_cursor_size_in_px)
                               : std::nullopt,
                           display::Display::ROTATE_0, color_);
}

void CursorLoader::UnloadCursors() {
  image_cursors_.clear();
}

void CursorLoader::ApplyColorAndLargeSize(
    ui::CursorData& data_in_and_out) const {
  if (color_ != ui::kDefaultCursorColor) {
    std::for_each(data_in_and_out.bitmaps.begin(),
                  data_in_and_out.bitmaps.end(), [&](SkBitmap& bitmap) {
                    bitmap = wm::GetColorAdjustedBitmap(bitmap, color_);
                  });
  }
  if (size_ == ui::CursorSize::kLarge) {
    const int large_cursor_size_in_px =
        ConvertDipToPixel(large_cursor_size_in_dip_, scale_);
    const gfx::Size cursor_size_in_px =
        gfx::SkISizeToSize(data_in_and_out.bitmaps[0].dimensions());
    if (large_cursor_size_in_px != cursor_size_in_px.height()) {
      const float rescale = static_cast<float>(large_cursor_size_in_px) /
                            static_cast<float>(cursor_size_in_px.height());
      std::for_each(data_in_and_out.bitmaps.begin(),
                    data_in_and_out.bitmaps.end(), [&](SkBitmap& bitmap) {
                      wm::ScaleAndRotateCursorBitmapAndHotpoint(
                          rescale, display::Display::ROTATE_0, &bitmap,
                          &data_in_and_out.hotspot);
                    });
      data_in_and_out.scale_factor *= rescale;
    }
  }
}

scoped_refptr<ui::PlatformCursor> CursorLoader::CursorFromType(
    CursorType type) {
  // An image cursor is loaded for this type.
  if (image_cursors_.count(type))
    return image_cursors_[type];

  // Check if there's a default platform cursor available.
  // For the none cursor, we also need to use the platform factory to take
  // into account the different ways of creating an invisible cursor.
  scoped_refptr<ui::PlatformCursor> cursor;
  if (use_platform_cursors_ || type == CursorType::kNone) {
    cursor = factory_->GetDefaultCursor(type, scale_);
    if (cursor)
      return cursor;
    // The cursor may fail to load if the cursor theme has just been reset.
    // We will be notified when the theme is loaded, but at this time we have to
    // fall back to the assets.
    LOG(WARNING) << "Failed to load a platform cursor of type " << type;
  }

  // Loads the default Aura cursor bitmap for the cursor type. Falls back on
  // pointer cursor if this fails.
  cursor = LoadCursorFromAsset(type);
  if (!cursor && type != CursorType::kPointer) {
    cursor = CursorFromType(CursorType::kPointer);
    image_cursors_[type] = cursor;
  }
  DCHECK(cursor) << "Failed to load a bitmap for the pointer cursor.";
  return cursor;
}

scoped_refptr<ui::PlatformCursor> CursorLoader::LoadCursorFromAsset(
    CursorType type) {
  std::optional<ui::CursorData> cursor_data = wm::GetCursorData(
      type, resource_scale_,
      size_ == ui::CursorSize::kLarge ? std::make_optional(ConvertDipToPixel(
                                            large_cursor_size_in_dip_, scale_))
                                      : std::nullopt,
      rotation_, color_);
  if (!cursor_data) {
    return nullptr;
  }

  if (cursor_data->bitmaps.size() == 1) {
    image_cursors_[type] = factory_->CreateImageCursor(
        type, cursor_data->bitmaps[0], cursor_data->hotspot,
        cursor_data->scale_factor);
  } else {
    image_cursors_[type] = factory_->CreateAnimatedCursor(
        type, cursor_data->bitmaps, cursor_data->hotspot,
        cursor_data->scale_factor, kAnimatedCursorFrameDelay);
  }
  return image_cursors_[type];
}

}  // namespace wm

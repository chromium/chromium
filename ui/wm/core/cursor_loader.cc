// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_loader.h"

#include <map>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/core/cursor_util.h"
#include "ui/wm/core/cursors_aura.h"

namespace wm {

namespace {

using ::ui::mojom::CursorType;

constexpr CursorType kAnimatedCursorTypes[] = {CursorType::kWait,
                                               CursorType::kProgress};

constexpr base::TimeDelta kAnimatedCursorFrameDelay = base::Milliseconds(25);

}  // namespace

CursorLoader::CursorLoader(bool use_platform_cursors)
    : use_platform_cursors_(use_platform_cursors),
      factory_(ui::CursorFactory::GetInstance()) {
  factory_->AddObserver(this);
}

CursorLoader::~CursorLoader() {
  factory_->RemoveObserver(this);
  UnloadCursors();
}

void CursorLoader::OnThemeLoaded() {
  UnloadCursors();
}

void CursorLoader::UnloadCursors() {
  image_cursors_.clear();
}

bool CursorLoader::SetDisplayData(display::Display::Rotation rotation,
                                  float scale) {
  if (rotation_ == rotation && scale_ == scale)
    return false;

  rotation_ = rotation;
  scale_ = scale;
  UnloadCursors();
  if (use_platform_cursors_)
    factory_->SetDeviceScaleFactor(scale_);
  return true;
}

void CursorLoader::SetSize(ui::CursorSize size) {
  if (size_ == size)
    return;

  size_ = size;
  UnloadCursors();
}

void CursorLoader::SetPlatformCursor(ui::Cursor* cursor) {
  DCHECK(cursor);

  // The platform cursor was already set via WebCursor::GetNativeCursor.
  if (cursor->type() == CursorType::kCustom)
    return;
  cursor->set_image_scale_factor(scale());
  cursor->SetPlatformCursor(CursorFromType(cursor->type()));
}

absl::optional<ui::CursorData> CursorLoader::GetCursorData(
    const ui::Cursor& cursor) const {
  CursorType type = cursor.type();
  if (type == CursorType::kNone)
    return ui::CursorData();

  if (type == CursorType::kCustom)
    return ui::CursorData({cursor.custom_bitmap()}, cursor.custom_hotspot());

  return ui::CursorData({GetDefaultBitmap(cursor)}, GetDefaultHotspot(cursor));
}

void CursorLoader::LoadImageCursor(CursorType type,
                                   int resource_id,
                                   const gfx::Point& hot) {
  gfx::Point hotspot = hot;
  if (base::ranges::count(kAnimatedCursorTypes, type) == 0) {
    SkBitmap bitmap;
    GetImageCursorBitmap(resource_id, scale(), rotation(), &hotspot, &bitmap);
    image_cursors_[type] = factory_->CreateImageCursor(type, bitmap, hotspot);
  } else {
    std::vector<SkBitmap> bitmaps;
    GetAnimatedCursorBitmaps(resource_id, scale(), rotation(), &hotspot,
                             &bitmaps);
    image_cursors_[type] = factory_->CreateAnimatedCursor(
        type, bitmaps, hotspot, kAnimatedCursorFrameDelay);
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
    cursor = factory_->GetDefaultCursor(type);
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
  int resource_id;
  gfx::Point hotspot;
  if (GetCursorDataFor(size(), type, scale(), &resource_id, &hotspot)) {
    LoadImageCursor(type, resource_id, hotspot);
    return image_cursors_[type];
  }
  return nullptr;
}

}  // namespace wm

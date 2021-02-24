// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader.h"

#include <map>
#include <vector>

#include "base/check.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

constexpr mojom::CursorType kAnimatedCursorTypes[] = {
    mojom::CursorType::kWait, mojom::CursorType::kProgress};

const int kAnimatedCursorFrameDelayMs = 25;

}  // namespace

CursorLoader::CursorLoader(bool use_platform_cursors)
    : use_platform_cursors_(use_platform_cursors),
      factory_(CursorFactory::GetInstance()) {}

CursorLoader::~CursorLoader() {
  UnloadCursors();
}

void CursorLoader::UnloadCursors() {
  for (const auto& image_cursor : image_cursors_)
    factory_->UnrefImageCursor(image_cursor.second);
  image_cursors_.clear();
}

bool CursorLoader::SetDisplayData(display::Display::Rotation rotation,
                                  float scale) {
  if (rotation_ == rotation && scale_ == scale)
    return false;

  rotation_ = rotation;
  scale_ = scale;
  UnloadCursors();
  return true;
}

void CursorLoader::SetSize(CursorSize size) {
  if (size_ == size)
    return;

  size_ = size;
  UnloadCursors();
}

void CursorLoader::SetPlatformCursor(Cursor* cursor) {
  DCHECK(cursor);

  // The platform cursor was already set via WebCursor::GetNativeCursor.
  if (cursor->type() == mojom::CursorType::kCustom)
    return;
  cursor->set_image_scale_factor(scale());
  cursor->SetPlatformCursor(CursorFromType(cursor->type()));
}

void CursorLoader::LoadImageCursor(mojom::CursorType type,
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
        type, bitmaps, hotspot, kAnimatedCursorFrameDelayMs);
  }
}

PlatformCursor CursorLoader::CursorFromType(mojom::CursorType type) {
  // An image cursor is loaded for this type.
  if (image_cursors_.count(type))
    return image_cursors_[type];

  // Check if there's a default platform cursor available.
  // For the none cursor, we also need to use the platform factory to take
  // into account the different ways of creating an invisible cursor.
  if (use_platform_cursors_ || type == mojom::CursorType::kNone) {
    base::Optional<PlatformCursor> default_cursor =
        factory_->GetDefaultCursor(type);
    if (default_cursor)
      return *default_cursor;
    LOG(ERROR) << "Failed to load a platform cursor of type " << type;
  }

  // Loads the default Aura cursor bitmap for the cursor type. Falls back on
  // pointer cursor if this fails.
  PlatformCursor platform = LoadCursorFromAsset(type);
  if (!platform && type != mojom::CursorType::kPointer) {
    platform = CursorFromType(mojom::CursorType::kPointer);
    factory_->RefImageCursor(platform);
    image_cursors_[type] = platform;
  }
  DCHECK(platform) << "Failed to load a bitmap for the pointer cursor.";
  return platform;
}

PlatformCursor CursorLoader::LoadCursorFromAsset(mojom::CursorType type) {
  int resource_id;
  gfx::Point hotspot;
  if (GetCursorDataFor(size(), type, scale(), &resource_id, &hotspot)) {
    LoadImageCursor(type, resource_id, hotspot);
    return image_cursors_[type];
  }
  return nullptr;
}

}  // namespace ui

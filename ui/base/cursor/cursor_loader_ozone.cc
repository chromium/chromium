// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_ozone.h"

#include <vector>

#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace ui {

CursorLoaderOzone::CursorLoaderOzone() {
  factory_ = CursorFactory::GetInstance();
}

CursorLoaderOzone::~CursorLoaderOzone() {
  UnloadAll();
}

void CursorLoaderOzone::LoadImageCursor(mojom::CursorType id,
                                        int resource_id,
                                        const gfx::Point& hot) {
  SkBitmap bitmap;
  gfx::Point hotspot = hot;

  GetImageCursorBitmap(resource_id, scale(), rotation(), &hotspot, &bitmap);

  image_cursors_[id] = factory_->CreateImageCursor(bitmap, hotspot);
}

void CursorLoaderOzone::LoadAnimatedCursor(mojom::CursorType id,
                                           int resource_id,
                                           const gfx::Point& hot,
                                           int frame_delay_ms) {
  std::vector<SkBitmap> bitmaps;
  gfx::Point hotspot = hot;

  GetAnimatedCursorBitmaps(
      resource_id, scale(), rotation(), &hotspot, &bitmaps);

  image_cursors_[id] =
      factory_->CreateAnimatedCursor(bitmaps, hotspot, frame_delay_ms);
}

void CursorLoaderOzone::UnloadAll() {
  for (const auto& image_cursor : image_cursors_)
    factory_->UnrefImageCursor(image_cursor.second);
  image_cursors_.clear();
}

void CursorLoaderOzone::SetPlatformCursor(gfx::NativeCursor* cursor) {
  DCHECK(cursor);

  // The platform cursor was already set via WebCursor::GetPlatformCursor.
  if (cursor->type() == mojom::CursorType::kCustom)
    return;
  cursor->set_image_scale_factor(scale());
  cursor->SetPlatformCursor(CursorFromType(cursor->type()));
}

PlatformCursor CursorLoaderOzone::CursorFromType(mojom::CursorType type) {
  // An image cursor is loaded for this type.
  if (image_cursors_.count(type))
    return image_cursors_[type];

  // Check if there's a default platform cursor available.
  base::Optional<PlatformCursor> default_cursor =
      factory_->GetDefaultCursor(type);
  if (default_cursor)
    return *default_cursor;

  // Loads the default Aura cursor bitmap for the cursor type. Falls back on
  // pointer cursor if this fails.
  PlatformCursor platform = CreateFallbackCursor(type);
  if (!platform && type != mojom::CursorType::kPointer) {
    platform = CursorFromType(mojom::CursorType::kPointer);
    factory_->RefImageCursor(platform);
    image_cursors_[type] = platform;
  }
  DCHECK(platform) << "Failed to load a fallback bitmap for cursor " << type;
  return platform;
}

// Gets default Aura cursor bitmap/hotspot and creates a PlatformCursor with it.
PlatformCursor CursorLoaderOzone::CreateFallbackCursor(mojom::CursorType type) {
  int resource_id;
  gfx::Point point;
  if (ui::GetCursorDataFor(ui::CursorSize::kNormal, type, scale(), &resource_id,
                           &point)) {
    LoadImageCursor(type, resource_id, point);
    return image_cursors_[type];
  }
  return nullptr;
}

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderOzone();
}

}  // namespace ui

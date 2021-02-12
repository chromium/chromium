// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_ozone.h"

#include <memory>
#include <vector>

#include "base/ranges/algorithm.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

constexpr mojom::CursorType kAnimatedCursorTypes[] = {
    mojom::CursorType::kWait, mojom::CursorType::kProgress};

const int kAnimatedCursorFrameDelayMs = 25;

}  // namespace

CursorLoaderOzone::CursorLoaderOzone(bool use_platform_cursors)
    : use_platform_cursors_(use_platform_cursors),
      factory_(CursorFactory::GetInstance()) {}

CursorLoaderOzone::~CursorLoaderOzone() {
  UnloadCursors();
}

void CursorLoaderOzone::UnloadCursors() {
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

void CursorLoaderOzone::LoadImageCursor(mojom::CursorType type,
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

PlatformCursor CursorLoaderOzone::CursorFromType(mojom::CursorType type) {
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

// Gets default Aura cursor bitmap/hotspot and creates a PlatformCursor with it.
PlatformCursor CursorLoaderOzone::LoadCursorFromAsset(mojom::CursorType type) {
  int resource_id;
  gfx::Point hotspot;
  if (GetCursorDataFor(size(), type, scale(), &resource_id, &hotspot)) {
    LoadImageCursor(type, resource_id, hotspot);
    return image_cursors_[type];
  }
  return nullptr;
}

std::unique_ptr<CursorLoader> CursorLoader::Create(bool use_platform_cursors) {
  return std::make_unique<CursorLoaderOzone>(use_platform_cursors);
}

}  // namespace ui

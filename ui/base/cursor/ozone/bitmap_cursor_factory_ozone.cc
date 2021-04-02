// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace ui {

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns true if ozone should use the default cursor for |type|, instead of
// loading and storing bitmaps for it. Used on Lacros to skip client-side bitmap
// loading for server-side cursors.
bool UseDefaultCursorForType(mojom::CursorType type) {
  using mojom::CursorType;
  switch (type) {
    case CursorType::kNull:
    case CursorType::kPointer:
    case CursorType::kCross:
    case CursorType::kHand:
    case CursorType::kIBeam:
    case CursorType::kWait:
    case CursorType::kHelp:
    case CursorType::kEastResize:
    case CursorType::kNorthResize:
    case CursorType::kNorthEastResize:
    case CursorType::kNorthWestResize:
    case CursorType::kSouthResize:
    case CursorType::kSouthEastResize:
    case CursorType::kSouthWestResize:
    case CursorType::kWestResize:
    case CursorType::kNorthSouthResize:
    case CursorType::kEastWestResize:
    case CursorType::kNorthEastSouthWestResize:
    case CursorType::kNorthWestSouthEastResize:
    case CursorType::kColumnResize:
    case CursorType::kRowResize:
    case CursorType::kMiddlePanning:
    case CursorType::kEastPanning:
    case CursorType::kNorthPanning:
    case CursorType::kNorthEastPanning:
    case CursorType::kNorthWestPanning:
    case CursorType::kSouthPanning:
    case CursorType::kSouthEastPanning:
    case CursorType::kSouthWestPanning:
    case CursorType::kWestPanning:
    case CursorType::kMove:
    case CursorType::kVerticalText:
    case CursorType::kCell:
    case CursorType::kContextMenu:
    case CursorType::kAlias:
    case CursorType::kProgress:
    case CursorType::kNoDrop:
    case CursorType::kCopy:
    case CursorType::kNotAllowed:
    case CursorType::kZoomIn:
    case CursorType::kZoomOut:
    case CursorType::kGrab:
    case CursorType::kGrabbing:
    case CursorType::kDndNone:
    case CursorType::kDndMove:
    case CursorType::kDndCopy:
    case CursorType::kDndLink:
      return true;
    case CursorType::kNone:
    case CursorType::kMiddlePanningVertical:
    case CursorType::kMiddlePanningHorizontal:
    case CursorType::kCustom:
    case CursorType::kEastWestNoResize:
    case CursorType::kNorthEastSouthWestNoResize:
    case CursorType::kNorthSouthNoResize:
    case CursorType::kNorthWestSouthEastNoResize:
      return false;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

BitmapCursorOzone* ToBitmapCursorOzone(PlatformCursor cursor) {
  return static_cast<BitmapCursorOzone*>(cursor);
}

PlatformCursor ToPlatformCursor(BitmapCursorOzone* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

}  // namespace

BitmapCursorOzone::BitmapCursorOzone(mojom::CursorType type) : type_(type) {}

BitmapCursorOzone::BitmapCursorOzone(mojom::CursorType type,
                                     const SkBitmap& bitmap,
                                     const gfx::Point& hotspot)
    : type_(type), hotspot_(hotspot) {
  if (!bitmap.isNull())
    bitmaps_.push_back(bitmap);
}

BitmapCursorOzone::BitmapCursorOzone(mojom::CursorType type,
                                     const std::vector<SkBitmap>& bitmaps,
                                     const gfx::Point& hotspot,
                                     base::TimeDelta frame_delay)
    : type_(type),
      bitmaps_(bitmaps),
      hotspot_(hotspot),
      frame_delay_(frame_delay) {
  DCHECK_LT(0U, bitmaps.size());
  DCHECK_LE(base::TimeDelta(), frame_delay);
  // No null bitmap should be in the list. Blank cursors should just be an empty
  // vector.
  DCHECK(std::find_if(bitmaps_.begin(), bitmaps_.end(),
                      [](const SkBitmap& bitmap) { return bitmap.isNull(); }) ==
         bitmaps_.end());
}

BitmapCursorOzone::BitmapCursorOzone(mojom::CursorType type,
                                     void* platform_data)
    : type_(type), platform_data_(platform_data) {}

BitmapCursorOzone::~BitmapCursorOzone() = default;

const gfx::Point& BitmapCursorOzone::hotspot() {
  return hotspot_;
}

const SkBitmap& BitmapCursorOzone::bitmap() {
  return bitmaps_[0];
}

const std::vector<SkBitmap>& BitmapCursorOzone::bitmaps() {
  return bitmaps_;
}

base::TimeDelta BitmapCursorOzone::frame_delay() {
  return frame_delay_;
}

BitmapCursorFactoryOzone::BitmapCursorFactoryOzone() {}

BitmapCursorFactoryOzone::~BitmapCursorFactoryOzone() {}

// static
scoped_refptr<BitmapCursorOzone> BitmapCursorFactoryOzone::GetBitmapCursor(
    PlatformCursor platform_cursor) {
  return base::WrapRefCounted(ToBitmapCursorOzone(platform_cursor));
}

PlatformCursor BitmapCursorFactoryOzone::GetDefaultCursor(
    mojom::CursorType type) {
  if (!default_cursors_.count(type)) {
    if (type == mojom::CursorType::kNone
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        || UseDefaultCursorForType(type)
#endif
    ) {
      // Lacros uses server-side cursors for most types. These cursors don't
      // need to load bitmap images on the client.
      // Similarly, the hidden cursor doesn't use any bitmap.
      default_cursors_[type] = base::MakeRefCounted<BitmapCursorOzone>(type);
    } else {
      return nullptr;
    }
  }

  // Returns owned default cursor for this type.
  return default_cursors_[type].get();
}

PlatformCursor BitmapCursorFactoryOzone::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  BitmapCursorOzone* cursor = new BitmapCursorOzone(type, bitmap, hotspot);
  cursor->AddRef();  // Balanced by UnrefImageCursor.
  return ToPlatformCursor(cursor);
}

PlatformCursor BitmapCursorFactoryOzone::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    base::TimeDelta frame_delay) {
  DCHECK_LT(0U, bitmaps.size());
  auto cursor = base::MakeRefCounted<BitmapCursorOzone>(type, bitmaps, hotspot,
                                                        frame_delay);
  cursor->AddRef();  // Balanced by UnrefImageCursor.
  return ToPlatformCursor(cursor.get());
}

void BitmapCursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {
  ToBitmapCursorOzone(cursor)->AddRef();
}

void BitmapCursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {
  ToBitmapCursorOzone(cursor)->Release();
}

}  // namespace ui

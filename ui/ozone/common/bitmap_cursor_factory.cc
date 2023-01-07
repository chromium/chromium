// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/bitmap_cursor_factory.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/ozone/common/bitmap_cursor.h"

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

}  // namespace

BitmapCursorFactory::BitmapCursorFactory() = default;

BitmapCursorFactory::~BitmapCursorFactory() = default;

scoped_refptr<PlatformCursor> BitmapCursorFactory::GetDefaultCursor(
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
      default_cursors_[type] =
          base::MakeRefCounted<BitmapCursor>(type, cursor_scale_factor_);
    } else {
      return nullptr;
    }
  }

  return default_cursors_[type];
}

scoped_refptr<PlatformCursor> BitmapCursorFactory::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  return base::MakeRefCounted<BitmapCursor>(type, bitmap, hotspot,
                                            cursor_scale_factor_);
}

scoped_refptr<PlatformCursor> BitmapCursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    base::TimeDelta frame_delay) {
  DCHECK_LT(0U, bitmaps.size());
  return base::MakeRefCounted<BitmapCursor>(type, bitmaps, hotspot, frame_delay,
                                            cursor_scale_factor_);
}

void BitmapCursorFactory::SetDeviceScaleFactor(float scale) {
  DCHECK_GT(scale, 0.f);
  cursor_scale_factor_ = scale;
}

}  // namespace ui

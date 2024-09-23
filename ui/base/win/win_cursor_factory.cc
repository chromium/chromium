// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/win_cursor_factory.h"

#include <windows.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/icon_util.h"
#include "ui/resources/grit/ui_unscaled_resources.h"

namespace ui {

namespace {

const wchar_t* GetCursorId(mojom::CursorType type) {
  switch (type) {
    case mojom::CursorType::kNull:
    case mojom::CursorType::kPointer:
      return IDC_ARROW;
    case mojom::CursorType::kCross:
      return IDC_CROSS;
    case mojom::CursorType::kHand:
      return IDC_HAND;
    case mojom::CursorType::kIBeam:
      return IDC_IBEAM;
    case mojom::CursorType::kWait:
      return IDC_WAIT;
    case mojom::CursorType::kHelp:
      return IDC_HELP;
    case mojom::CursorType::kEastResize:
    case mojom::CursorType::kWestResize:
    case mojom::CursorType::kEastWestResize:
      return IDC_SIZEWE;
    case mojom::CursorType::kNorthResize:
    case mojom::CursorType::kSouthResize:
    case mojom::CursorType::kNorthSouthResize:
      return IDC_SIZENS;
    case mojom::CursorType::kNorthEastResize:
    case mojom::CursorType::kSouthWestResize:
    case mojom::CursorType::kNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case mojom::CursorType::kNorthWestResize:
    case mojom::CursorType::kSouthEastResize:
    case mojom::CursorType::kNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case mojom::CursorType::kMove:
      return IDC_SIZEALL;
    case mojom::CursorType::kProgress:
      return IDC_APPSTARTING;
    case mojom::CursorType::kNoDrop:
    case mojom::CursorType::kNotAllowed:
    case mojom::CursorType::kEastWestNoResize:
    case mojom::CursorType::kNorthEastSouthWestNoResize:
    case mojom::CursorType::kNorthSouthNoResize:
    case mojom::CursorType::kNorthWestSouthEastNoResize:
      return IDC_NO;
    case mojom::CursorType::kColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case mojom::CursorType::kRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case mojom::CursorType::kMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case mojom::CursorType::kMiddlePanningVertical:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_VERTICAL);
    case mojom::CursorType::kMiddlePanningHorizontal:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_HORIZONTAL);
    case mojom::CursorType::kEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case mojom::CursorType::kNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case mojom::CursorType::kNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case mojom::CursorType::kNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case mojom::CursorType::kSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case mojom::CursorType::kSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case mojom::CursorType::kSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case mojom::CursorType::kWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case mojom::CursorType::kVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case mojom::CursorType::kCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case mojom::CursorType::kZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case mojom::CursorType::kZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case mojom::CursorType::kGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case mojom::CursorType::kGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
    case mojom::CursorType::kCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case mojom::CursorType::kAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case mojom::CursorType::kDndCopy:
    case mojom::CursorType::kDndLink:
    case mojom::CursorType::kDndMove:
    case mojom::CursorType::kDndNone:
    case mojom::CursorType::kContextMenu:
      NOTIMPLEMENTED();
      return IDC_ARROW;
    case mojom::CursorType::kNone:
    case mojom::CursorType::kCustom:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace

WinCursorFactory::WinCursorFactory() = default;

WinCursorFactory::~WinCursorFactory() = default;

scoped_refptr<PlatformCursor> WinCursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  if (!default_cursors_.count(type)) {
    // Using a dark 1x1 bit bmp for the kNone cursor may still cause DWM to do
    // composition work unnecessarily. Better to totally remove it from the
    // screen. crbug.com/1069698
    HCURSOR hcursor = nullptr;
    if (type != mojom::CursorType::kNone) {
      const wchar_t* id = GetCursorId(type);
      hcursor = LoadCursor(nullptr, id);
      // Try loading the cursor from the Chromium resources.
      if (!hcursor)
        hcursor = LoadCursorFromResourcesDataDLL(id);
      if (!hcursor)
        return nullptr;
    }
    default_cursors_[type] = base::MakeRefCounted<WinCursor>(hcursor);
  }

  return default_cursors_[type];
}

std::optional<CursorData> WinCursorFactory::GetCursorData(
    mojom::CursorType type) {
  DCHECK_NE(type, mojom::CursorType::kNone);
  DCHECK_NE(type, mojom::CursorType::kCustom);

  auto cursor = GetDefaultCursor(type);
  if (!cursor) {
    return std::nullopt;
  }

  HCURSOR hcursor = WinCursor::FromPlatformCursor(cursor)->hcursor();
  SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(hcursor);
  if (bitmap.isNull()) {
    return std::nullopt;
  }

  return ui::CursorData({std::move(bitmap)},
                        IconUtil::GetHotSpotFromHICON(hcursor));
}

scoped_refptr<PlatformCursor> WinCursorFactory::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot,
    float scale) {
  return base::MakeRefCounted<WinCursor>(
      IconUtil::CreateCursorFromSkBitmap(bitmap, hotspot).release(),
      /*should_destroy=*/true);
}

}  // namespace ui

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursors_aura.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

#if defined(OS_WIN)
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/win/win_cursor.h"
#include "ui/gfx/icon_util.h"
#endif

namespace ui {

namespace {

struct HotPoint {
  int x;
  int y;
};

struct CursorData {
  mojom::CursorType id;
  int resource_id;
  HotPoint hot_1x;
  HotPoint hot_2x;
};

struct CursorSizeData {
  const CursorSize id;
  const CursorData* cursors;
  const int length;
};

const CursorData kNormalCursors[] = {
    {mojom::CursorType::kNull, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}},
    {mojom::CursorType::kPointer, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}},
    {mojom::CursorType::kNoDrop, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}},
    {mojom::CursorType::kNotAllowed, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}},
    {mojom::CursorType::kCopy, IDR_AURA_CURSOR_COPY, {9, 9}, {18, 18}},
    {mojom::CursorType::kHand, IDR_AURA_CURSOR_HAND, {9, 4}, {19, 8}},
    {mojom::CursorType::kMove, IDR_AURA_CURSOR_MOVE, {11, 11}, {23, 23}},
    {mojom::CursorType::kNorthEastResize,
     IDR_AURA_CURSOR_NORTH_EAST_RESIZE,
     {12, 11},
     {25, 23}},
    {mojom::CursorType::kSouthWestResize,
     IDR_AURA_CURSOR_SOUTH_WEST_RESIZE,
     {12, 11},
     {25, 23}},
    {mojom::CursorType::kSouthEastResize,
     IDR_AURA_CURSOR_SOUTH_EAST_RESIZE,
     {11, 11},
     {24, 23}},
    {mojom::CursorType::kNorthWestResize,
     IDR_AURA_CURSOR_NORTH_WEST_RESIZE,
     {11, 11},
     {24, 23}},
    {mojom::CursorType::kNorthResize,
     IDR_AURA_CURSOR_NORTH_RESIZE,
     {11, 12},
     {23, 23}},
    {mojom::CursorType::kSouthResize,
     IDR_AURA_CURSOR_SOUTH_RESIZE,
     {11, 12},
     {23, 23}},
    {mojom::CursorType::kEastResize,
     IDR_AURA_CURSOR_EAST_RESIZE,
     {12, 11},
     {25, 23}},
    {mojom::CursorType::kWestResize,
     IDR_AURA_CURSOR_WEST_RESIZE,
     {12, 11},
     {25, 23}},
    {mojom::CursorType::kIBeam, IDR_AURA_CURSOR_IBEAM, {12, 12}, {24, 25}},
    {mojom::CursorType::kAlias, IDR_AURA_CURSOR_ALIAS, {8, 6}, {15, 11}},
    {mojom::CursorType::kCell, IDR_AURA_CURSOR_CELL, {11, 11}, {24, 23}},
    {mojom::CursorType::kContextMenu,
     IDR_AURA_CURSOR_CONTEXT_MENU,
     {4, 4},
     {8, 9}},
    {mojom::CursorType::kCross, IDR_AURA_CURSOR_CROSSHAIR, {12, 12}, {24, 24}},
    {mojom::CursorType::kHelp, IDR_AURA_CURSOR_HELP, {4, 4}, {8, 9}},
    {mojom::CursorType::kVerticalText,
     IDR_AURA_CURSOR_XTERM_HORIZ,
     {12, 11},
     {26, 23}},
    {mojom::CursorType::kZoomIn, IDR_AURA_CURSOR_ZOOM_IN, {10, 10}, {20, 20}},
    {mojom::CursorType::kZoomOut, IDR_AURA_CURSOR_ZOOM_OUT, {10, 10}, {20, 20}},
    {mojom::CursorType::kRowResize,
     IDR_AURA_CURSOR_ROW_RESIZE,
     {11, 12},
     {23, 23}},
    {mojom::CursorType::kColumnResize,
     IDR_AURA_CURSOR_COL_RESIZE,
     {12, 11},
     {25, 23}},
    {mojom::CursorType::kEastWestResize,
     IDR_AURA_CURSOR_EAST_WEST_RESIZE,
     {12, 11},
     {25, 23}},
    {mojom::CursorType::kNorthSouthResize,
     IDR_AURA_CURSOR_NORTH_SOUTH_RESIZE,
     {11, 12},
     {23, 23}},
    {mojom::CursorType::kNorthEastSouthWestResize,
     IDR_AURA_CURSOR_NORTH_EAST_SOUTH_WEST_RESIZE,
     {12, 11},
     {25, 23}},
    {mojom::CursorType::kNorthWestSouthEastResize,
     IDR_AURA_CURSOR_NORTH_WEST_SOUTH_EAST_RESIZE,
     {11, 11},
     {24, 23}},
    {mojom::CursorType::kGrab, IDR_AURA_CURSOR_GRAB, {8, 5}, {16, 10}},
    {mojom::CursorType::kGrabbing, IDR_AURA_CURSOR_GRABBING, {9, 9}, {18, 18}},
    {mojom::CursorType::kWait, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}},
    {mojom::CursorType::kProgress, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}},
};

const CursorData kLargeCursors[] = {
    // The 2x hotspots should be double of the 1x, even though the cursors are
    // shown as same size as 1x (64x64), because in 2x dpi screen, the 1x large
    // cursor assets (64x64) are internally enlarged to the double size
    // (128x128)
    // by ResourceBundleImageSource.
    {mojom::CursorType::kNull, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}},
    {mojom::CursorType::kPointer, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}},
    {mojom::CursorType::kNoDrop,
     IDR_AURA_CURSOR_BIG_NO_DROP,
     {10, 10},
     {20, 20}},
    {mojom::CursorType::kNotAllowed,
     IDR_AURA_CURSOR_BIG_NO_DROP,
     {10, 10},
     {20, 20}},
    {mojom::CursorType::kCopy, IDR_AURA_CURSOR_BIG_COPY, {10, 10}, {20, 20}},
    {mojom::CursorType::kHand, IDR_AURA_CURSOR_BIG_HAND, {25, 7}, {50, 14}},
    {mojom::CursorType::kMove, IDR_AURA_CURSOR_BIG_MOVE, {32, 31}, {64, 62}},
    {mojom::CursorType::kNorthEastResize,
     IDR_AURA_CURSOR_BIG_NORTH_EAST_RESIZE,
     {31, 28},
     {62, 56}},
    {mojom::CursorType::kSouthWestResize,
     IDR_AURA_CURSOR_BIG_SOUTH_WEST_RESIZE,
     {31, 28},
     {62, 56}},
    {mojom::CursorType::kSouthEastResize,
     IDR_AURA_CURSOR_BIG_SOUTH_EAST_RESIZE,
     {28, 28},
     {56, 56}},
    {mojom::CursorType::kNorthWestResize,
     IDR_AURA_CURSOR_BIG_NORTH_WEST_RESIZE,
     {28, 28},
     {56, 56}},
    {mojom::CursorType::kNorthResize,
     IDR_AURA_CURSOR_BIG_NORTH_RESIZE,
     {29, 32},
     {58, 64}},
    {mojom::CursorType::kSouthResize,
     IDR_AURA_CURSOR_BIG_SOUTH_RESIZE,
     {29, 32},
     {58, 64}},
    {mojom::CursorType::kEastResize,
     IDR_AURA_CURSOR_BIG_EAST_RESIZE,
     {35, 29},
     {70, 58}},
    {mojom::CursorType::kWestResize,
     IDR_AURA_CURSOR_BIG_WEST_RESIZE,
     {35, 29},
     {70, 58}},
    {mojom::CursorType::kIBeam, IDR_AURA_CURSOR_BIG_IBEAM, {30, 32}, {60, 64}},
    {mojom::CursorType::kAlias, IDR_AURA_CURSOR_BIG_ALIAS, {19, 11}, {38, 22}},
    {mojom::CursorType::kCell, IDR_AURA_CURSOR_BIG_CELL, {30, 30}, {60, 60}},
    {mojom::CursorType::kContextMenu,
     IDR_AURA_CURSOR_BIG_CONTEXT_MENU,
     {11, 11},
     {22, 22}},
    {mojom::CursorType::kCross,
     IDR_AURA_CURSOR_BIG_CROSSHAIR,
     {30, 32},
     {60, 64}},
    {mojom::CursorType::kHelp, IDR_AURA_CURSOR_BIG_HELP, {10, 11}, {20, 22}},
    {mojom::CursorType::kVerticalText,
     IDR_AURA_CURSOR_BIG_XTERM_HORIZ,
     {32, 30},
     {64, 60}},
    {mojom::CursorType::kZoomIn,
     IDR_AURA_CURSOR_BIG_ZOOM_IN,
     {25, 26},
     {50, 52}},
    {mojom::CursorType::kZoomOut,
     IDR_AURA_CURSOR_BIG_ZOOM_OUT,
     {26, 26},
     {52, 52}},
    {mojom::CursorType::kRowResize,
     IDR_AURA_CURSOR_BIG_ROW_RESIZE,
     {29, 32},
     {58, 64}},
    {mojom::CursorType::kColumnResize,
     IDR_AURA_CURSOR_BIG_COL_RESIZE,
     {35, 29},
     {70, 58}},
    {mojom::CursorType::kEastWestResize,
     IDR_AURA_CURSOR_BIG_EAST_WEST_RESIZE,
     {35, 29},
     {70, 58}},
    {mojom::CursorType::kNorthSouthResize,
     IDR_AURA_CURSOR_BIG_NORTH_SOUTH_RESIZE,
     {29, 32},
     {58, 64}},
    {mojom::CursorType::kNorthEastSouthWestResize,
     IDR_AURA_CURSOR_BIG_NORTH_EAST_SOUTH_WEST_RESIZE,
     {32, 30},
     {64, 60}},
    {mojom::CursorType::kNorthWestSouthEastResize,
     IDR_AURA_CURSOR_BIG_NORTH_WEST_SOUTH_EAST_RESIZE,
     {32, 31},
     {64, 62}},
    {mojom::CursorType::kGrab, IDR_AURA_CURSOR_BIG_GRAB, {21, 11}, {42, 22}},
    {mojom::CursorType::kGrabbing,
     IDR_AURA_CURSOR_BIG_GRABBING,
     {20, 12},
     {40, 24}},
    // TODO(https://crbug.com/336867): create IDR_AURA_CURSOR_BIG_THROBBER.
};

const CursorSizeData kCursorSizes[] = {
    {CursorSize::kNormal, kNormalCursors, base::size(kNormalCursors)},
    {CursorSize::kLarge, kLargeCursors, base::size(kLargeCursors)},
};

const CursorSizeData* GetCursorSizeByType(CursorSize cursor_size) {
  for (size_t i = 0; i < base::size(kCursorSizes); ++i) {
    if (kCursorSizes[i].id == cursor_size)
      return &kCursorSizes[i];
  }

  return NULL;
}

bool SearchTable(const CursorData* table,
                 size_t table_length,
                 mojom::CursorType id,
                 float scale_factor,
                 int* resource_id,
                 gfx::Point* point) {
  DCHECK_NE(scale_factor, 0);

  bool resource_2x_available =
      ResourceBundle::GetSharedInstance().GetMaxScaleFactor() ==
      SCALE_FACTOR_200P;
  for (size_t i = 0; i < table_length; ++i) {
    if (table[i].id == id) {
      *resource_id = table[i].resource_id;
      *point = scale_factor == 1.0f || !resource_2x_available
                   ? gfx::Point(table[i].hot_1x.x, table[i].hot_1x.y)
                   : gfx::Point(table[i].hot_2x.x, table[i].hot_2x.y);
      return true;
    }
  }

  return false;
}

}  // namespace

bool GetCursorDataFor(CursorSize cursor_size,
                      mojom::CursorType id,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point) {
  const CursorSizeData* cursor_set = GetCursorSizeByType(cursor_size);
  if (cursor_set && SearchTable(cursor_set->cursors, cursor_set->length, id,
                                scale_factor, resource_id, point)) {
    return true;
  }

  // Falls back to the default cursor set.
  cursor_set = GetCursorSizeByType(ui::CursorSize::kNormal);
  DCHECK(cursor_set);
  return SearchTable(cursor_set->cursors, cursor_set->length, id, scale_factor,
                     resource_id, point);
}

SkBitmap GetDefaultBitmap(const Cursor& cursor) {
#if defined(OS_WIN)
  Cursor cursor_copy = cursor;
  CursorLoader cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor_copy);
  return IconUtil::CreateSkBitmapFromHICON(
      static_cast<WinCursor*>(cursor_copy.platform())->hcursor());
#else
  int resource_id;
  gfx::Point hotspot;
  if (!GetCursorDataFor(ui::CursorSize::kNormal, cursor.type(),
                        cursor.image_scale_factor(), &resource_id, &hotspot)) {
    return SkBitmap();
  }
  return *ResourceBundle::GetSharedInstance()
              .GetImageSkiaNamed(resource_id)
              ->bitmap();
#endif
}

gfx::Point GetDefaultHotspot(const Cursor& cursor) {
#if defined(OS_WIN)
  Cursor cursor_copy = cursor;
  CursorLoader cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor_copy);
  return IconUtil::GetHotSpotFromHICON(
      static_cast<WinCursor*>(cursor_copy.platform())->hcursor());
#else
  int resource_id;
  gfx::Point hotspot;
  if (!GetCursorDataFor(ui::CursorSize::kNormal, cursor.type(),
                        cursor.image_scale_factor(), &resource_id, &hotspot)) {
    return gfx::Point();
  }
  return hotspot;
#endif
}

}  // namespace ui

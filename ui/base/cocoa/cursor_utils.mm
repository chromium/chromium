// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/cursor_utils.h"

#import <AppKit/AppKit.h>
#include <stdint.h>

#include "base/check.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/notreached.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Private interface to CoreCursor. See
// https://github.com/WebKit/WebKit/blob/main/Source/WebCore/PAL/pal/spi/mac/HIServicesSPI.h

enum {
  kArrowCursor = 0,
  kIBeamCursor = 1,
  kMakeAliasCursor = 2,
  kOperationNotAllowedCursor = 3,
  kBusyButClickableCursor = 4,
  kCopyCursor = 5,
  kClosedHandCursor = 11,
  kOpenHandCursor = 12,
  kPointingHandCursor = 13,
  kCountingUpHandCursor = 14,
  kCountingDownHandCursor = 15,
  kCountingUpAndDownHandCursor = 16,
  kResizeLeftCursor = 17,
  kResizeRightCursor = 18,
  kResizeLeftRightCursor = 19,
  kCrosshairCursor = 20,
  kResizeUpCursor = 21,
  kResizeDownCursor = 22,
  kResizeUpDownCursor = 23,
  kContextualMenuCursor = 24,
  kDisappearingItemCursor = 25,
  kVerticalIBeamCursor = 26,
  kResizeEastCursor = 27,
  kResizeEastWestCursor = 28,
  kResizeNortheastCursor = 29,
  kResizeNortheastSouthwestCursor = 30,
  kResizeNorthCursor = 31,
  kResizeNorthSouthCursor = 32,
  kResizeNorthwestCursor = 33,
  kResizeNorthwestSoutheastCursor = 34,
  kResizeSoutheastCursor = 35,
  kResizeSouthCursor = 36,
  kResizeSouthwestCursor = 37,
  kResizeWestCursor = 38,
  kMoveCursor = 39,
  kHelpCursor = 40,
  kCellCursor = 41,
  kZoomInCursor = 42,
  kZoomOutCursor = 43
};
using CrCoreCursorType = int64_t;

@interface CrCoreCursor : NSCursor {
 @private
  CrCoreCursorType _type;
}

+ (id)cursorWithType:(CrCoreCursorType)type;
- (id)initWithType:(CrCoreCursorType)type;
- (CrCoreCursorType)_coreCursorType;

@end

@implementation CrCoreCursor

+ (id)cursorWithType:(CrCoreCursorType)type {
  NSCursor* cursor = [[CrCoreCursor alloc] initWithType:type];
  if (cursor.image) {
    return cursor;
  }

  return nil;
}

- (id)initWithType:(CrCoreCursorType)type {
  if ((self = [super init])) {
    _type = type;
  }
  return self;
}

- (CrCoreCursorType)_coreCursorType {
  return _type;
}

@end

namespace {

NSCursor* LoadCursor(int resource_id, int hotspot_x, int hotspot_y) {
  const gfx::Image& cursor_image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
  DCHECK(!cursor_image.IsEmpty());
  return [[NSCursor alloc] initWithImage:cursor_image.ToNSImage()
                                 hotSpot:NSMakePoint(hotspot_x, hotspot_y)];
}

// Gets a specified cursor from CoreCursor, falling back to loading it from the
// image cache if CoreCursor cannot provide it.
NSCursor* GetCoreCursorWithFallback(CrCoreCursorType type,
                                    int resource_id,
                                    int hotspot_x,
                                    int hotspot_y) {
  NSCursor* cursor = [CrCoreCursor cursorWithType:type];
  if (cursor)
    return cursor;

  return LoadCursor(resource_id, hotspot_x, hotspot_y);
}

NSCursor* CreateCustomCursor(const ui::Cursor& cursor) {
  float custom_scale = cursor.image_scale_factor();
  gfx::Size custom_size(cursor.custom_bitmap().width(),
                        cursor.custom_bitmap().height());

  // Convert from pixels to view units.
  if (custom_scale == 0)
    custom_scale = 1;
  NSSize dip_size = NSSizeFromCGSize(
      gfx::ScaleToFlooredSize(custom_size, 1 / custom_scale).ToCGSize());
  NSPoint dip_hotspot = NSPointFromCGPoint(
      gfx::ScaleToFlooredPoint(cursor.custom_hotspot(), 1 / custom_scale)
          .ToCGPoint());

  // Both the image and its representation need to have the same size for
  // cursors to appear in high resolution on retina displays. Note that the
  // size of a representation is not the same as pixelsWide or pixelsHigh.
  NSImage* cursor_image = skia::SkBitmapToNSImageWithColorSpace(
      cursor.custom_bitmap(), base::mac::GetSRGBColorSpace());
  [cursor_image setSize:dip_size];
  [[[cursor_image representations] objectAtIndex:0] setSize:dip_size];

  return [[NSCursor alloc] initWithImage:cursor_image hotSpot:dip_hotspot];
}

}  // namespace

namespace ui {

// Match Safari's cursor choices; see platform/mac/CursorMac.mm .
NSCursor* GetNativeCursor(const ui::Cursor& cursor) {
  switch (cursor.type()) {
    case ui::mojom::CursorType::kPointer:
      return [NSCursor arrowCursor];
    case ui::mojom::CursorType::kCross:
      return [NSCursor crosshairCursor];
    case ui::mojom::CursorType::kHand:
      return [NSCursor pointingHandCursor];
    case ui::mojom::CursorType::kIBeam:
      return [NSCursor IBeamCursor];
    case ui::mojom::CursorType::kWait:
      return GetCoreCursorWithFallback(kBusyButClickableCursor, IDR_WAIT_CURSOR,
                                       7, 7);
    case ui::mojom::CursorType::kHelp:
      return GetCoreCursorWithFallback(kHelpCursor, IDR_HELP_CURSOR, 8, 8);
    case ui::mojom::CursorType::kEastResize:
    case ui::mojom::CursorType::kEastPanning:
      return GetCoreCursorWithFallback(kResizeEastCursor,
                                       IDR_EAST_RESIZE_CURSOR, 14, 7);
    case ui::mojom::CursorType::kNorthResize:
    case ui::mojom::CursorType::kNorthPanning:
      return GetCoreCursorWithFallback(kResizeNorthCursor,
                                       IDR_NORTH_RESIZE_CURSOR, 7, 1);
    case ui::mojom::CursorType::kNorthEastResize:
    case ui::mojom::CursorType::kNorthEastPanning:
      return GetCoreCursorWithFallback(kResizeNortheastCursor,
                                       IDR_NORTHEAST_RESIZE_CURSOR, 14, 1);
    case ui::mojom::CursorType::kNorthWestResize:
    case ui::mojom::CursorType::kNorthWestPanning:
      return GetCoreCursorWithFallback(kResizeNorthwestCursor,
                                       IDR_NORTHWEST_RESIZE_CURSOR, 0, 0);
    case ui::mojom::CursorType::kSouthResize:
    case ui::mojom::CursorType::kSouthPanning:
      return GetCoreCursorWithFallback(kResizeSouthCursor,
                                       IDR_SOUTH_RESIZE_CURSOR, 7, 14);
    case ui::mojom::CursorType::kSouthEastResize:
    case ui::mojom::CursorType::kSouthEastPanning:
      return GetCoreCursorWithFallback(kResizeSoutheastCursor,
                                       IDR_SOUTHEAST_RESIZE_CURSOR, 14, 14);
    case ui::mojom::CursorType::kSouthWestResize:
    case ui::mojom::CursorType::kSouthWestPanning:
      return GetCoreCursorWithFallback(kResizeSouthwestCursor,
                                       IDR_SOUTHWEST_RESIZE_CURSOR, 1, 14);
    case ui::mojom::CursorType::kWestResize:
    case ui::mojom::CursorType::kWestPanning:
      return GetCoreCursorWithFallback(kResizeWestCursor,
                                       IDR_WEST_RESIZE_CURSOR, 1, 7);
    case ui::mojom::CursorType::kNorthSouthResize:
      return GetCoreCursorWithFallback(kResizeNorthSouthCursor,
                                       IDR_NORTHSOUTH_RESIZE_CURSOR, 7, 7);
    case ui::mojom::CursorType::kEastWestResize:
      return GetCoreCursorWithFallback(kResizeEastWestCursor,
                                       IDR_EASTWEST_RESIZE_CURSOR, 7, 7);
    case ui::mojom::CursorType::kNorthEastSouthWestResize:
      return GetCoreCursorWithFallback(kResizeNortheastSouthwestCursor,
                                       IDR_NORTHEASTSOUTHWEST_RESIZE_CURSOR, 7,
                                       7);
    case ui::mojom::CursorType::kNorthWestSouthEastResize:
      return GetCoreCursorWithFallback(kResizeNorthwestSoutheastCursor,
                                       IDR_NORTHWESTSOUTHEAST_RESIZE_CURSOR, 7,
                                       7);
    case ui::mojom::CursorType::kColumnResize:
      return [NSCursor resizeLeftRightCursor];
    case ui::mojom::CursorType::kRowResize:
      return [NSCursor resizeUpDownCursor];
    case ui::mojom::CursorType::kMiddlePanning:
    case ui::mojom::CursorType::kMiddlePanningVertical:
    case ui::mojom::CursorType::kMiddlePanningHorizontal:
    case ui::mojom::CursorType::kMove:
      return GetCoreCursorWithFallback(kMoveCursor, IDR_MOVE_CURSOR, 7, 7);
    case ui::mojom::CursorType::kVerticalText:
      return [NSCursor IBeamCursorForVerticalLayout];
    case ui::mojom::CursorType::kCell:
      return GetCoreCursorWithFallback(kCellCursor, IDR_CELL_CURSOR, 7, 7);
    case ui::mojom::CursorType::kContextMenu:
      return [NSCursor contextualMenuCursor];
    case ui::mojom::CursorType::kAlias:
      return GetCoreCursorWithFallback(kMakeAliasCursor, IDR_ALIAS_CURSOR, 11,
                                       3);
    case ui::mojom::CursorType::kProgress:
      return GetCoreCursorWithFallback(kBusyButClickableCursor,
                                       IDR_PROGRESS_CURSOR, 3, 2);
    case ui::mojom::CursorType::kNoDrop:
    case ui::mojom::CursorType::kNotAllowed:
    case ui::mojom::CursorType::kEastWestNoResize:
    case ui::mojom::CursorType::kNorthEastSouthWestNoResize:
    case ui::mojom::CursorType::kNorthSouthNoResize:
    case ui::mojom::CursorType::kNorthWestSouthEastNoResize:
      return [NSCursor operationNotAllowedCursor];
    case ui::mojom::CursorType::kCopy:
      return [NSCursor dragCopyCursor];
    case ui::mojom::CursorType::kNone:
      return LoadCursor(IDR_NONE_CURSOR, 7, 7);
    case ui::mojom::CursorType::kZoomIn:
      return GetCoreCursorWithFallback(kZoomInCursor, IDR_ZOOMIN_CURSOR, 7, 7);
    case ui::mojom::CursorType::kZoomOut:
      return GetCoreCursorWithFallback(kZoomOutCursor, IDR_ZOOMOUT_CURSOR, 7,
                                       7);
    case ui::mojom::CursorType::kGrab:
      return [NSCursor openHandCursor];
    case ui::mojom::CursorType::kGrabbing:
      return [NSCursor closedHandCursor];
    case ui::mojom::CursorType::kCustom:
      return CreateCustomCursor(cursor);
    case ui::mojom::CursorType::kNull:
      return [NSCursor arrowCursor];
    case ui::mojom::CursorType::kDndNone:
    case ui::mojom::CursorType::kDndMove:
    case ui::mojom::CursorType::kDndCopy:
    case ui::mojom::CursorType::kDndLink:
      // These cursors do not apply on Mac.
      break;
  }
  NOTREACHED();
  return nil;
}

}  // namespace ui

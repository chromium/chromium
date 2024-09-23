// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/cursor_utils.h"

#import <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <stdint.h>

#include "base/notreached.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

// Private interface to CoreCursor. See
// https://github.com/WebKit/WebKit/blob/main/Source/WebCore/PAL/pal/spi/mac/HIServicesSPI.h
//
// Note that the column/row resize cursors have a bar in the middle (e.g. <-|->)
// whereas the frame resize cursors have no bar in the middle (e.g. <-->).
enum class CrCoreCursorType : int32_t {
  kArrow = 0,                      // NSCursor.arrowCursor
  kIBeam = 1,                      // NSCursor.IBeamCursor
  kMakeAlias = 2,                  // NSCursor.dragLinkCursor
  kOperationNotAllowed = 3,        // NSCursor.operationNotAllowedCursor
  kBusyButClickable = 4,           // NSCursor.busyButClickableCursor (private)
  kCopy = 5,                       // NSCursor.dragCopyCursor
  kScreenShotSelection = 7,        // -
  kScreenShotSelectionToClip = 8,  // -
  kScreenShotWindow = 9,           // -
  kScreenShotWindowToClip = 10,    // -
  kClosedHand = 11,                // NSCursor.closedHandCursor
  kOpenHand = 12,                  // NSCursor.openHandCursor
  kPointingHand = 13,              // NSCursor.pointingHandCursor
  kCountingUpHand = 14,            // -
  kCountingDownHand = 15,          // -
  kCountingUpAndDownHand = 16,     // -
  kColumnResizeLeft = 17,          // [NSCursor columnResizeCursorInDirections:]
  kColumnResizeRight = 18,         // [NSCursor columnResizeCursorInDirections:]
  kColumnResizeLeftRight = 19,     // NSCursor.columnResizeCursor
  kCrosshair = 20,                 // NSCursor.crosshairCursor
  kRowResizeUp = 21,               // [NSCursor rowResizeCursorInDirections:]
  kRowResizeDown = 22,             // [NSCursor rowResizeCursorInDirections:]
  kRowResizeUpDown = 23,           // NSCursor.rowResizeCursor
  kContextualMenu = 24,            // NSCursor.contextualMenuCursor
  kDisappearingItem = 25,          // NSCursor.disappearingItemCursor
  kVerticalIBeam = 26,             // NSCursor.IBeamCursorForVerticalLayout
  kFrameResizeEast =
      27,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeEastWest =
      28,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeNortheast =
      29,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeNortheastSouthwest =
      30,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeNorth =
      31,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeNorthSouth =
      32,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeNorthwest =
      33,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeNorthwestSoutheast =
      34,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeSoutheast =
      35,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeSouth =
      36,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeSouthwest =
      37,  // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kFrameResizeWest =
      38,         // [NSCursor frameResizeCursorFromPosition:inDirections:]
  kMove = 39,     // oddly, not NSCursor._moveCursor (private)
  kHelp = 40,     // NSCursor._helpCursor (private)
  kCell = 41,     // -
  kZoomIn = 42,   // NSCursor.zoomInCursor
  kZoomOut = 43,  // NSCursor.zoomOutCursor
};

@interface CrCoreCursor : NSCursor

+ (id)cursorWithType:(CrCoreCursorType)type;
@property(readonly, nonatomic) CrCoreCursorType _coreCursorType;

@end

@implementation CrCoreCursor

@synthesize _coreCursorType = _type;

+ (id)cursorWithType:(CrCoreCursorType)type {
  return [[CrCoreCursor alloc] initWithType:type];
}

- (id)initWithType:(CrCoreCursorType)type {
  if ((self = [super init])) {
    _type = type;
  }
  return self;
}

@end

namespace {

NSCursor* NoneNSCursor() {
  static NSCursor* cursor = [[NSCursor alloc]
      initWithImage:[[NSImage alloc] initWithSize:NSMakeSize(1, 1)]
            hotSpot:NSZeroPoint];

  return cursor;
}

NSCursor* CustomNSCursor(const ui::Cursor& cursor) {
  float custom_scale = cursor.image_scale_factor();
  gfx::Size custom_size(cursor.custom_bitmap().width(),
                        cursor.custom_bitmap().height());

  // Convert from pixels to view units.
  if (custom_scale == 0) {
    custom_scale = 1;
  }
  NSSize dip_size = NSSizeFromCGSize(
      gfx::ScaleToFlooredSize(custom_size, 1 / custom_scale).ToCGSize());
  NSPoint dip_hotspot = NSPointFromCGPoint(
      gfx::ScaleToFlooredPoint(cursor.custom_hotspot(), 1 / custom_scale)
          .ToCGPoint());

  // Both the image and its representation need to have the same size for
  // cursors to appear in high resolution on retina displays. Note that the
  // size of a representation is not the same as pixelsWide or pixelsHigh.
  NSImage* cursor_image = skia::SkBitmapToNSImage(cursor.custom_bitmap());
  cursor_image.size = dip_size;
  cursor_image.representations[0].size = dip_size;

  return [[NSCursor alloc] initWithImage:cursor_image hotSpot:dip_hotspot];
}

}  // namespace

namespace ui {

// Match Safari's cursor choices; see
// https://github.com/WebKit/WebKit/blob/main/Source/WebCore/platform/mac/CursorMac.mm
NSCursor* GetNativeCursor(const ui::Cursor& cursor) {
  switch (cursor.type()) {
    case ui::mojom::CursorType::kNull:
    case ui::mojom::CursorType::kPointer:
      return NSCursor.arrowCursor;
    case ui::mojom::CursorType::kCross:
      return NSCursor.crosshairCursor;
    case ui::mojom::CursorType::kHand:
      return NSCursor.pointingHandCursor;
    case ui::mojom::CursorType::kIBeam:
      return NSCursor.IBeamCursor;
    case ui::mojom::CursorType::kWait:
      return [CrCoreCursor cursorWithType:CrCoreCursorType::kBusyButClickable];
    case ui::mojom::CursorType::kHelp:
      return [CrCoreCursor cursorWithType:CrCoreCursorType::kHelp];
    case ui::mojom::CursorType::kEastResize:
    case ui::mojom::CursorType::kEastPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionRight
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return [CrCoreCursor cursorWithType:CrCoreCursorType::kFrameResizeEast];
      }
    case ui::mojom::CursorType::kNorthResize:
    case ui::mojom::CursorType::kNorthPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionTop
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return
            [CrCoreCursor cursorWithType:CrCoreCursorType::kFrameResizeNorth];
      }
    case ui::mojom::CursorType::kNorthEastResize:
    case ui::mojom::CursorType::kNorthEastPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionTopRight
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeNortheast];
      }
    case ui::mojom::CursorType::kNorthWestResize:
    case ui::mojom::CursorType::kNorthWestPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionTopLeft
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeNorthwest];
      }
    case ui::mojom::CursorType::kSouthResize:
    case ui::mojom::CursorType::kSouthPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionBottom
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return
            [CrCoreCursor cursorWithType:CrCoreCursorType::kFrameResizeSouth];
      }
    case ui::mojom::CursorType::kSouthEastResize:
    case ui::mojom::CursorType::kSouthEastPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionBottomRight
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeSoutheast];
      }
    case ui::mojom::CursorType::kSouthWestResize:
    case ui::mojom::CursorType::kSouthWestPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionBottomLeft
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeSouthwest];
      }
    case ui::mojom::CursorType::kWestResize:
    case ui::mojom::CursorType::kWestPanning:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionLeft
                             inDirections:NSCursorFrameResizeDirectionsOutward];
      } else {
        return [CrCoreCursor cursorWithType:CrCoreCursorType::kFrameResizeWest];
      }
    case ui::mojom::CursorType::kNorthSouthResize:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionTop
                             inDirections:NSCursorFrameResizeDirectionsAll];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeNorthSouth];
      }
    case ui::mojom::CursorType::kEastWestResize:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionLeft
                             inDirections:NSCursorFrameResizeDirectionsAll];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeEastWest];
      }
    case ui::mojom::CursorType::kNorthEastSouthWestResize:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionTopRight
                             inDirections:NSCursorFrameResizeDirectionsAll];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeNortheastSouthwest];
      }
    case ui::mojom::CursorType::kNorthWestSouthEastResize:
      if (@available(macOS 15.0, *)) {
        return [NSCursor
            frameResizeCursorFromPosition:NSCursorFrameResizePositionTopLeft
                             inDirections:NSCursorFrameResizeDirectionsAll];
      } else {
        return [CrCoreCursor
            cursorWithType:CrCoreCursorType::kFrameResizeNorthwestSoutheast];
      }
    case ui::mojom::CursorType::kColumnResize:
      if (@available(macOS 15.0, *)) {
        return NSCursor.columnResizeCursor;
      } else {
        return NSCursor.resizeLeftRightCursor;
      }
    case ui::mojom::CursorType::kRowResize:
      if (@available(macOS 15.0, *)) {
        return NSCursor.rowResizeCursor;
      } else {
        return NSCursor.resizeUpDownCursor;
      }
    case ui::mojom::CursorType::kMiddlePanning:
    case ui::mojom::CursorType::kMiddlePanningVertical:
    case ui::mojom::CursorType::kMiddlePanningHorizontal:
    case ui::mojom::CursorType::kMove:
      return [CrCoreCursor cursorWithType:CrCoreCursorType::kMove];
    case ui::mojom::CursorType::kVerticalText:
      return NSCursor.IBeamCursorForVerticalLayout;
    case ui::mojom::CursorType::kCell:
      return [CrCoreCursor cursorWithType:CrCoreCursorType::kCell];
    case ui::mojom::CursorType::kContextMenu:
      return NSCursor.contextualMenuCursor;
    case ui::mojom::CursorType::kAlias:
      return NSCursor.dragLinkCursor;
    case ui::mojom::CursorType::kProgress:
      return [CrCoreCursor cursorWithType:CrCoreCursorType::kBusyButClickable];
    case ui::mojom::CursorType::kNoDrop:
    case ui::mojom::CursorType::kNotAllowed:
    case ui::mojom::CursorType::kEastWestNoResize:
    case ui::mojom::CursorType::kNorthEastSouthWestNoResize:
    case ui::mojom::CursorType::kNorthSouthNoResize:
    case ui::mojom::CursorType::kNorthWestSouthEastNoResize:
      return NSCursor.operationNotAllowedCursor;
    case ui::mojom::CursorType::kCopy:
      return NSCursor.dragCopyCursor;
    case ui::mojom::CursorType::kNone:
      return NoneNSCursor();
    case ui::mojom::CursorType::kZoomIn:
      if (@available(macOS 15.0, *)) {
        return NSCursor.zoomInCursor;
      } else {
        return [CrCoreCursor cursorWithType:CrCoreCursorType::kZoomIn];
      }
    case ui::mojom::CursorType::kZoomOut:
      if (@available(macOS 15.0, *)) {
        return NSCursor.zoomOutCursor;
      } else {
        return [CrCoreCursor cursorWithType:CrCoreCursorType::kZoomOut];
      }
    case ui::mojom::CursorType::kGrab:
      return NSCursor.openHandCursor;
    case ui::mojom::CursorType::kGrabbing:
      return NSCursor.closedHandCursor;
    case ui::mojom::CursorType::kCustom:
      return CustomNSCursor(cursor);
    case ui::mojom::CursorType::kDndNone:
    case ui::mojom::CursorType::kDndMove:
    case ui::mojom::CursorType::kDndCopy:
    case ui::mojom::CursorType::kDndLink:
      // These cursors do not apply on Mac.
      break;
  }
  NOTREACHED();
}

}  // namespace ui

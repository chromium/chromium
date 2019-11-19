// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojom/cursor_mojom_traits.h"

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/mojom/cursor.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
ui::mojom::CursorType
EnumTraits<ui::mojom::CursorType, ui::CursorType>::ToMojom(
    ui::CursorType input) {
  switch (input) {
    case ui::CursorType::kNull:
      return ui::mojom::CursorType::kNull;
    case ui::CursorType::kPointer:
      return ui::mojom::CursorType::kPointer;
    case ui::CursorType::kCross:
      return ui::mojom::CursorType::kCross;
    case ui::CursorType::kHand:
      return ui::mojom::CursorType::kHand;
    case ui::CursorType::kIBeam:
      return ui::mojom::CursorType::kIBeam;
    case ui::CursorType::kWait:
      return ui::mojom::CursorType::kWait;
    case ui::CursorType::kHelp:
      return ui::mojom::CursorType::kHelp;
    case ui::CursorType::kEastResize:
      return ui::mojom::CursorType::kEastResize;
    case ui::CursorType::kNorthResize:
      return ui::mojom::CursorType::kNorthResize;
    case ui::CursorType::kNorthEastResize:
      return ui::mojom::CursorType::kNorthEastResize;
    case ui::CursorType::kNorthWestResize:
      return ui::mojom::CursorType::kNorthWestResize;
    case ui::CursorType::kSouthResize:
      return ui::mojom::CursorType::kSouthResize;
    case ui::CursorType::kSouthEastResize:
      return ui::mojom::CursorType::kSouthEastResize;
    case ui::CursorType::kSouthWestResize:
      return ui::mojom::CursorType::kSouthWestResize;
    case ui::CursorType::kWestResize:
      return ui::mojom::CursorType::kWestResize;
    case ui::CursorType::kNorthSouthResize:
      return ui::mojom::CursorType::kNorthSouthResize;
    case ui::CursorType::kEastWestResize:
      return ui::mojom::CursorType::kEastWestResize;
    case ui::CursorType::kNorthEastSouthWestResize:
      return ui::mojom::CursorType::kNorthEastSouthWestResize;
    case ui::CursorType::kNorthWestSouthEastResize:
      return ui::mojom::CursorType::kNorthWestSouthEastResize;
    case ui::CursorType::kColumnResize:
      return ui::mojom::CursorType::kColumnResize;
    case ui::CursorType::kRowResize:
      return ui::mojom::CursorType::kRowResize;
    case ui::CursorType::kMiddlePanning:
      return ui::mojom::CursorType::kMiddlePanning;
    case ui::CursorType::kMiddlePanningVertical:
      return ui::mojom::CursorType::kMiddlePanningVertical;
    case ui::CursorType::kMiddlePanningHorizontal:
      return ui::mojom::CursorType::kMiddlePanningHorizontal;
    case ui::CursorType::kEastPanning:
      return ui::mojom::CursorType::kEastPanning;
    case ui::CursorType::kNorthPanning:
      return ui::mojom::CursorType::kNorthPanning;
    case ui::CursorType::kNorthEastPanning:
      return ui::mojom::CursorType::kNorthEastPanning;
    case ui::CursorType::kNorthWestPanning:
      return ui::mojom::CursorType::kNorthWestPanning;
    case ui::CursorType::kSouthPanning:
      return ui::mojom::CursorType::kSouthPanning;
    case ui::CursorType::kSouthEastPanning:
      return ui::mojom::CursorType::kSouthEastPanning;
    case ui::CursorType::kSouthWestPanning:
      return ui::mojom::CursorType::kSouthWestPanning;
    case ui::CursorType::kWestPanning:
      return ui::mojom::CursorType::kWestPanning;
    case ui::CursorType::kMove:
      return ui::mojom::CursorType::kMove;
    case ui::CursorType::kVerticalText:
      return ui::mojom::CursorType::kVerticalText;
    case ui::CursorType::kCell:
      return ui::mojom::CursorType::kCell;
    case ui::CursorType::kContextMenu:
      return ui::mojom::CursorType::kContextMenu;
    case ui::CursorType::kAlias:
      return ui::mojom::CursorType::kAlias;
    case ui::CursorType::kProgress:
      return ui::mojom::CursorType::kProgress;
    case ui::CursorType::kNoDrop:
      return ui::mojom::CursorType::kNoDrop;
    case ui::CursorType::kCopy:
      return ui::mojom::CursorType::kCopy;
    case ui::CursorType::kNone:
      return ui::mojom::CursorType::kNone;
    case ui::CursorType::kNotAllowed:
      return ui::mojom::CursorType::kNotAllowed;
    case ui::CursorType::kZoomIn:
      return ui::mojom::CursorType::kZoomIn;
    case ui::CursorType::kZoomOut:
      return ui::mojom::CursorType::kZoomOut;
    case ui::CursorType::kGrab:
      return ui::mojom::CursorType::kGrab;
    case ui::CursorType::kGrabbing:
      return ui::mojom::CursorType::kGrabbing;
    case ui::CursorType::kCustom:
      return ui::mojom::CursorType::kCustom;
    case ui::CursorType::kDndNone:
    case ui::CursorType::kDndMove:
    case ui::CursorType::kDndCopy:
    case ui::CursorType::kDndLink:
      // The mojom version is the same as the restricted Webcursor constants;
      // don't allow system cursors to be transmitted.
      NOTREACHED();
      return ui::mojom::CursorType::kNull;
  }
  NOTREACHED();
  return ui::mojom::CursorType::kNull;
}

// static
bool EnumTraits<ui::mojom::CursorType, ui::CursorType>::FromMojom(
    ui::mojom::CursorType input,
    ui::CursorType* out) {
  switch (input) {
    case ui::mojom::CursorType::kNull:
      *out = ui::CursorType::kNull;
      return true;
    case ui::mojom::CursorType::kPointer:
      *out = ui::CursorType::kPointer;
      return true;
    case ui::mojom::CursorType::kCross:
      *out = ui::CursorType::kCross;
      return true;
    case ui::mojom::CursorType::kHand:
      *out = ui::CursorType::kHand;
      return true;
    case ui::mojom::CursorType::kIBeam:
      *out = ui::CursorType::kIBeam;
      return true;
    case ui::mojom::CursorType::kWait:
      *out = ui::CursorType::kWait;
      return true;
    case ui::mojom::CursorType::kHelp:
      *out = ui::CursorType::kHelp;
      return true;
    case ui::mojom::CursorType::kEastResize:
      *out = ui::CursorType::kEastResize;
      return true;
    case ui::mojom::CursorType::kNorthResize:
      *out = ui::CursorType::kNorthResize;
      return true;
    case ui::mojom::CursorType::kNorthEastResize:
      *out = ui::CursorType::kNorthEastResize;
      return true;
    case ui::mojom::CursorType::kNorthWestResize:
      *out = ui::CursorType::kNorthWestResize;
      return true;
    case ui::mojom::CursorType::kSouthResize:
      *out = ui::CursorType::kSouthResize;
      return true;
    case ui::mojom::CursorType::kSouthEastResize:
      *out = ui::CursorType::kSouthEastResize;
      return true;
    case ui::mojom::CursorType::kSouthWestResize:
      *out = ui::CursorType::kSouthWestResize;
      return true;
    case ui::mojom::CursorType::kWestResize:
      *out = ui::CursorType::kWestResize;
      return true;
    case ui::mojom::CursorType::kNorthSouthResize:
      *out = ui::CursorType::kNorthSouthResize;
      return true;
    case ui::mojom::CursorType::kEastWestResize:
      *out = ui::CursorType::kEastWestResize;
      return true;
    case ui::mojom::CursorType::kNorthEastSouthWestResize:
      *out = ui::CursorType::kNorthEastSouthWestResize;
      return true;
    case ui::mojom::CursorType::kNorthWestSouthEastResize:
      *out = ui::CursorType::kNorthWestSouthEastResize;
      return true;
    case ui::mojom::CursorType::kColumnResize:
      *out = ui::CursorType::kColumnResize;
      return true;
    case ui::mojom::CursorType::kRowResize:
      *out = ui::CursorType::kRowResize;
      return true;
    case ui::mojom::CursorType::kMiddlePanning:
      *out = ui::CursorType::kMiddlePanning;
      return true;
    case ui::mojom::CursorType::kMiddlePanningVertical:
      *out = ui::CursorType::kMiddlePanningVertical;
      return true;
    case ui::mojom::CursorType::kMiddlePanningHorizontal:
      *out = ui::CursorType::kMiddlePanningHorizontal;
      return true;
    case ui::mojom::CursorType::kEastPanning:
      *out = ui::CursorType::kEastPanning;
      return true;
    case ui::mojom::CursorType::kNorthPanning:
      *out = ui::CursorType::kNorthPanning;
      return true;
    case ui::mojom::CursorType::kNorthEastPanning:
      *out = ui::CursorType::kNorthEastPanning;
      return true;
    case ui::mojom::CursorType::kNorthWestPanning:
      *out = ui::CursorType::kNorthWestPanning;
      return true;
    case ui::mojom::CursorType::kSouthPanning:
      *out = ui::CursorType::kSouthPanning;
      return true;
    case ui::mojom::CursorType::kSouthEastPanning:
      *out = ui::CursorType::kSouthEastPanning;
      return true;
    case ui::mojom::CursorType::kSouthWestPanning:
      *out = ui::CursorType::kSouthWestPanning;
      return true;
    case ui::mojom::CursorType::kWestPanning:
      *out = ui::CursorType::kWestPanning;
      return true;
    case ui::mojom::CursorType::kMove:
      *out = ui::CursorType::kMove;
      return true;
    case ui::mojom::CursorType::kVerticalText:
      *out = ui::CursorType::kVerticalText;
      return true;
    case ui::mojom::CursorType::kCell:
      *out = ui::CursorType::kCell;
      return true;
    case ui::mojom::CursorType::kContextMenu:
      *out = ui::CursorType::kContextMenu;
      return true;
    case ui::mojom::CursorType::kAlias:
      *out = ui::CursorType::kAlias;
      return true;
    case ui::mojom::CursorType::kProgress:
      *out = ui::CursorType::kProgress;
      return true;
    case ui::mojom::CursorType::kNoDrop:
      *out = ui::CursorType::kNoDrop;
      return true;
    case ui::mojom::CursorType::kCopy:
      *out = ui::CursorType::kCopy;
      return true;
    case ui::mojom::CursorType::kNone:
      *out = ui::CursorType::kNone;
      return true;
    case ui::mojom::CursorType::kNotAllowed:
      *out = ui::CursorType::kNotAllowed;
      return true;
    case ui::mojom::CursorType::kZoomIn:
      *out = ui::CursorType::kZoomIn;
      return true;
    case ui::mojom::CursorType::kZoomOut:
      *out = ui::CursorType::kZoomOut;
      return true;
    case ui::mojom::CursorType::kGrab:
      *out = ui::CursorType::kGrab;
      return true;
    case ui::mojom::CursorType::kGrabbing:
      *out = ui::CursorType::kGrabbing;
      return true;
    case ui::mojom::CursorType::kCustom:
      *out = ui::CursorType::kCustom;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ui::mojom::CursorSize
EnumTraits<ui::mojom::CursorSize, ui::CursorSize>::ToMojom(
    ui::CursorSize input) {
  switch (input) {
    case ui::CursorSize::kNormal:
      return ui::mojom::CursorSize::kNormal;
    case ui::CursorSize::kLarge:
      return ui::mojom::CursorSize::kLarge;
  }

  NOTREACHED();
  return ui::mojom::CursorSize::kNormal;
}

// static
bool EnumTraits<ui::mojom::CursorSize, ui::CursorSize>::FromMojom(
    ui::mojom::CursorSize input,
    ui::CursorSize* out) {
  switch (input) {
    case ui::mojom::CursorSize::kNormal:
      *out = ui::CursorSize::kNormal;
      return true;
    case ui::mojom::CursorSize::kLarge:
      *out = ui::CursorSize::kLarge;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
gfx::Point StructTraits<ui::mojom::CursorDataView, ui::Cursor>::hotspot(
    const ui::Cursor& c) {
  return c.GetHotspot();
}

// static
SkBitmap StructTraits<ui::mojom::CursorDataView, ui::Cursor>::bitmap(
    const ui::Cursor& c) {
  return c.GetBitmap();
}

// static
bool StructTraits<ui::mojom::CursorDataView, ui::Cursor>::Read(
    ui::mojom::CursorDataView data,
    ui::Cursor* out) {
  ui::CursorType type;
  if (!data.ReadNativeType(&type))
    return false;

  if (type != ui::CursorType::kCustom) {
    *out = ui::Cursor(type);
    return true;
  }

  gfx::Point hotspot;
  SkBitmap bitmap;

  if (!data.ReadHotspot(&hotspot) || !data.ReadBitmap(&bitmap))
    return false;

  // TODO(estade): do we even need this field? It doesn't appear to be used
  // anywhere and is a property of the display, not the cursor.
  float device_scale_factor = data.device_scale_factor();

  *out = ui::Cursor(type);
  out->set_custom_bitmap(bitmap);
  out->set_custom_hotspot(hotspot);
  out->set_device_scale_factor(device_scale_factor);
  return true;
}

}  // namespace mojo

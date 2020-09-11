// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_factory.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/base/x/x11_cursor_loader.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

X11Cursor* ToX11Cursor(PlatformCursor cursor) {
  return static_cast<X11Cursor*>(cursor);
}

PlatformCursor ToPlatformCursor(X11Cursor* cursor) {
  return static_cast<PlatformCursor>(cursor);
}

scoped_refptr<X11Cursor> CreateInvisibleCursor(XCursorLoader* cursor_loader) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  return cursor_loader->CreateCursor(bitmap, gfx::Point(0, 0));
}

// Returns a cursor name, compatible with either X11 or the FreeDesktop.org
// cursor spec
// (https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#x_font_cursors
// and https://www.freedesktop.org/wiki/Specifications/cursor-spec/), followed
// by fallbacks that can work as replacements in some environments where the
// original may not be available (e.g. desktop environments other than
// GNOME and KDE).
// TODO(hferreiro): each list starts with the FreeDesktop.org icon name but
// "ns-resize", "ew-resize", "nesw-resize", "nwse-resize", "grab", "grabbing",
// which were not available in older versions of Breeze, the default KDE theme.
std::vector<std::string> CursorNamesFromType(mojom::CursorType type) {
  switch (type) {
    case mojom::CursorType::kMove:
      // Returning "move" is the correct thing here, but Blink doesn't make a
      // distinction between move and all-scroll.  Other platforms use a cursor
      // more consistent with all-scroll, so use that.
    case mojom::CursorType::kMiddlePanning:
    case mojom::CursorType::kMiddlePanningVertical:
    case mojom::CursorType::kMiddlePanningHorizontal:
      return {"all-scroll", "fleur"};
    case mojom::CursorType::kEastPanning:
    case mojom::CursorType::kEastResize:
      return {"e-resize", "right_side"};
    case mojom::CursorType::kNorthPanning:
    case mojom::CursorType::kNorthResize:
      return {"n-resize", "top_side"};
    case mojom::CursorType::kNorthEastPanning:
    case mojom::CursorType::kNorthEastResize:
      return {"ne-resize", "top_right_corner"};
    case mojom::CursorType::kNorthWestPanning:
    case mojom::CursorType::kNorthWestResize:
      return {"nw-resize", "top_left_corner"};
    case mojom::CursorType::kSouthPanning:
    case mojom::CursorType::kSouthResize:
      return {"s-resize", "bottom_side"};
    case mojom::CursorType::kSouthEastPanning:
    case mojom::CursorType::kSouthEastResize:
      return {"se-resize", "bottom_right_corner"};
    case mojom::CursorType::kSouthWestPanning:
    case mojom::CursorType::kSouthWestResize:
      return {"sw-resize", "bottom_left_corner"};
    case mojom::CursorType::kWestPanning:
    case mojom::CursorType::kWestResize:
      return {"w-resize", "left_side"};
    case mojom::CursorType::kNone:
      return {"none"};
    case mojom::CursorType::kGrab:
      return {"openhand", "grab"};
    case mojom::CursorType::kGrabbing:
      return {"closedhand", "grabbing", "hand2"};
    case mojom::CursorType::kCross:
      return {"crosshair", "cross"};
    case mojom::CursorType::kHand:
      return {"pointer", "hand", "hand2"};
    case mojom::CursorType::kIBeam:
      return {"text", "xterm"};
    case mojom::CursorType::kProgress:
      return {"progress", "left_ptr_watch", "watch"};
    case mojom::CursorType::kWait:
      return {"wait", "watch"};
    case mojom::CursorType::kHelp:
      return {"help"};
    case mojom::CursorType::kNorthSouthResize:
      return {"sb_v_double_arrow", "ns-resize"};
    case mojom::CursorType::kEastWestResize:
      return {"sb_h_double_arrow", "ew-resize"};
    case mojom::CursorType::kColumnResize:
      return {"col-resize", "sb_h_double_arrow"};
    case mojom::CursorType::kRowResize:
      return {"row-resize", "sb_v_double_arrow"};
    case mojom::CursorType::kNorthEastSouthWestResize:
      return {"size_bdiag", "nesw-resize", "fd_double_arrow"};
    case mojom::CursorType::kNorthWestSouthEastResize:
      return {"size_fdiag", "nwse-resize", "bd_double_arrow"};
    case mojom::CursorType::kVerticalText:
      return {"vertical-text"};
    case mojom::CursorType::kZoomIn:
      return {"zoom-in"};
    case mojom::CursorType::kZoomOut:
      return {"zoom-out"};
    case mojom::CursorType::kCell:
      return {"cell", "plus"};
    case mojom::CursorType::kContextMenu:
      return {"context-menu"};
    case mojom::CursorType::kAlias:
      return {"alias"};
    case mojom::CursorType::kNoDrop:
      return {"no-drop"};
    case mojom::CursorType::kCopy:
      return {"copy"};
    case mojom::CursorType::kNotAllowed:
      return {"not-allowed", "crossed_circle"};
    case mojom::CursorType::kDndNone:
      return {"dnd-none", "hand2"};
    case mojom::CursorType::kDndMove:
      return {"dnd-move", "hand2"};
    case mojom::CursorType::kDndCopy:
      return {"dnd-copy", "hand2"};
    case mojom::CursorType::kDndLink:
      return {"dnd-link", "hand2"};
    case mojom::CursorType::kCustom:
      // kCustom is for custom image cursors. The platform cursor will be set
      // at WebCursor::GetPlatformCursor().
      NOTREACHED();
      FALLTHROUGH;
    case mojom::CursorType::kNull:
    case mojom::CursorType::kPointer:
      return {"left_ptr"};
  }
  NOTREACHED();
  return {"left_ptr"};
}

}  // namespace

X11CursorFactory::X11CursorFactory()
    : cursor_loader_(std::make_unique<XCursorLoader>(x11::Connection::Get())),
      invisible_cursor_(CreateInvisibleCursor(cursor_loader_.get())) {}

X11CursorFactory::~X11CursorFactory() = default;

base::Optional<PlatformCursor> X11CursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  auto cursor = GetDefaultCursorInternal(type);
  if (!cursor)
    return base::nullopt;
  return ToPlatformCursor(cursor.get());
}

PlatformCursor X11CursorFactory::CreateImageCursor(const SkBitmap& bitmap,
                                                   const gfx::Point& hotspot) {
  // There is a problem with custom cursors that have no custom data. The
  // resulting SkBitmap is empty and X crashes when creating a zero size cursor
  // image. Return invisible cursor here instead.
  if (bitmap.drawsNothing()) {
    // The result of |invisible_cursor_| is owned by the caller, and will be
    // Unref()ed by code far away. (Usually in web_cursor.cc in content, among
    // others.) If we don't manually add another reference before we cast this
    // to a void*, we can end up with |invisible_cursor_| being freed out from
    // under us.
    invisible_cursor_->AddRef();
    return ToPlatformCursor(invisible_cursor_.get());
  }

  auto cursor = cursor_loader_->CreateCursor(bitmap, hotspot);
  cursor->AddRef();
  return ToPlatformCursor(cursor.get());
}

PlatformCursor X11CursorFactory::CreateAnimatedCursor(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  std::vector<XCursorLoader::Image> images;
  images.reserve(bitmaps.size());
  for (const auto& bitmap : bitmaps)
    images.push_back(XCursorLoader::Image{bitmap, hotspot, frame_delay_ms});
  auto cursor = cursor_loader_->CreateCursor(images);
  cursor->AddRef();
  return ToPlatformCursor(cursor.get());
}

void X11CursorFactory::RefImageCursor(PlatformCursor cursor) {
  ToX11Cursor(cursor)->AddRef();
}

void X11CursorFactory::UnrefImageCursor(PlatformCursor cursor) {
  ToX11Cursor(cursor)->Release();
}

void X11CursorFactory::ObserveThemeChanges() {
  auto* cursor_theme_manager = CursorThemeManager::GetInstance();
  DCHECK(cursor_theme_manager);
  cursor_theme_observer_.Add(cursor_theme_manager);
}

void X11CursorFactory::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  ClearThemeCursors();
}

void X11CursorFactory::OnCursorThemeSizeChanged(int cursor_theme_size) {
  ClearThemeCursors();
}

scoped_refptr<X11Cursor> X11CursorFactory::GetDefaultCursorInternal(
    mojom::CursorType type) {
  if (type == mojom::CursorType::kNone)
    return invisible_cursor_;

  if (!default_cursors_.count(type)) {
    // Try to load a predefined X11 cursor.
    default_cursors_[type] =
        cursor_loader_->LoadCursor(CursorNamesFromType(type));
  }

  // Returns owned default cursor for this type.
  return default_cursors_[type];
}

void X11CursorFactory::ClearThemeCursors() {
  default_cursors_.clear();
}

}  // namespace ui

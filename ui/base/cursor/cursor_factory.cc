// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_factory.h"

#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"

namespace ui {

namespace {

CursorFactory* g_instance = nullptr;

}  // namespace

CursorFactoryObserver::~CursorFactoryObserver() = default;

CursorFactory::CursorFactory() {
  DCHECK(!g_instance) << "There should only be a single CursorFactory.";
  g_instance = this;
}

CursorFactory::~CursorFactory() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

CursorFactory* CursorFactory::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

void CursorFactory::AddObserver(CursorFactoryObserver* observer) {
  observers_.AddObserver(observer);
}

void CursorFactory::RemoveObserver(CursorFactoryObserver* observer) {
  observers_.RemoveObserver(observer);
}

void CursorFactory::NotifyObserversOnThemeLoaded() {
  observers_.Notify(&CursorFactoryObserver::OnThemeLoaded);
}

scoped_refptr<PlatformCursor> CursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<PlatformCursor> CursorFactory::GetDefaultCursor(
    mojom::CursorType type,
    float scale) {
  // If the backend doesn't provide its own implementation of
  // GetDefaultCursor(type, scale) it is assumed that the cursor objects
  // returned by GetDefaultCursor(type) are independent of display scale values.
  return GetDefaultCursor(type);
}

scoped_refptr<PlatformCursor> CursorFactory::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot,
    float scale) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<CursorData> CursorFactory::GetCursorData(mojom::CursorType type) {
  return std::nullopt;
}

scoped_refptr<PlatformCursor> CursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    float scale,
    base::TimeDelta frame_delay) {
  NOTIMPLEMENTED();
  return nullptr;
}

void CursorFactory::ObserveThemeChanges() {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Returns a cursor name compatible with either X11 or the FreeDesktop.org
// cursor spec ([1] and [2]), followed by fallbacks that can work as
// replacements in some environments where the original may not be available
// (e.g. desktop environments other than GNOME and KDE).
//
// TODO(hferreiro): each list starts with the FreeDesktop.org icon name but
// "ns-resize", "ew-resize", "nesw-resize", "nwse-resize", "grab", "grabbing",
// which were not available in older versions of Breeze, the default KDE theme.
//
// [1]
// https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#x_font_cursors
// [2] https://www.freedesktop.org/wiki/Specifications/cursor-spec/
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
      return {};
    case mojom::CursorType::kGrab:
      return {"openhand", "grab", "hand1"};
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
    case mojom::CursorType::kNorthSouthNoResize:
    case mojom::CursorType::kEastWestNoResize:
    case mojom::CursorType::kNorthEastSouthWestNoResize:
    case mojom::CursorType::kNorthWestSouthEastNoResize:
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
      // at WebCursor::GetNativeCursor().
      NOTREACHED();
    case mojom::CursorType::kNull:
    case mojom::CursorType::kPointer:
      return {"left_ptr"};
  }
  NOTREACHED();
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace ui

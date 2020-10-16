// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_win.h"

#include <windows.h>

#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/resources/grit/ui_unscaled_resources.h"

namespace ui {

namespace {

base::LazyInstance<base::string16>::DestructorAtExit
    g_cursor_resource_module_name;

const wchar_t* GetCursorId(gfx::NativeCursor native_cursor) {
  switch (native_cursor.type()) {
    case mojom::CursorType::kNull:
      return IDC_ARROW;
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
      return IDC_SIZEWE;
    case mojom::CursorType::kNorthResize:
      return IDC_SIZENS;
    case mojom::CursorType::kNorthEastResize:
      return IDC_SIZENESW;
    case mojom::CursorType::kNorthWestResize:
      return IDC_SIZENWSE;
    case mojom::CursorType::kSouthResize:
      return IDC_SIZENS;
    case mojom::CursorType::kSouthEastResize:
      return IDC_SIZENWSE;
    case mojom::CursorType::kSouthWestResize:
      return IDC_SIZENESW;
    case mojom::CursorType::kWestResize:
      return IDC_SIZEWE;
    case mojom::CursorType::kNorthSouthResize:
      return IDC_SIZENS;
    case mojom::CursorType::kEastWestResize:
      return IDC_SIZEWE;
    case mojom::CursorType::kNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case mojom::CursorType::kNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case mojom::CursorType::kMove:
      return IDC_SIZEALL;
    case mojom::CursorType::kProgress:
      return IDC_APPSTARTING;
    case mojom::CursorType::kNoDrop:
      return IDC_NO;
    case mojom::CursorType::kNotAllowed:
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
    case mojom::CursorType::kContextMenu:
    case mojom::CursorType::kCustom:
    case mojom::CursorType::kNone:
      NOTIMPLEMENTED();
      return IDC_ARROW;
    default:
      NOTREACHED();
      return IDC_ARROW;
  }
}

}  // namespace

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderWin;
}

CursorLoaderWin::CursorLoaderWin() {
}

CursorLoaderWin::~CursorLoaderWin() {
}

void CursorLoaderWin::LoadImageCursor(mojom::CursorType id,
                                      int resource_id,
                                      const gfx::Point& hot) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::LoadAnimatedCursor(mojom::CursorType id,
                                         int resource_id,
                                         const gfx::Point& hot,
                                         int frame_delay_ms) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::UnloadAll() {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::SetPlatformCursor(gfx::NativeCursor* cursor) {
  if (cursor->type() == mojom::CursorType::kCustom)
    return;

  // Using a dark 1x1 bit bmp kNone cursor may still cause DWM to do composition
  // work unnecessarily. Better to totally remove it from the screen.
  // crbug.com/1069698
  if (cursor->type() == mojom::CursorType::kNone) {
    cursor->SetPlatformCursor(nullptr);
    return;
  }

  if (cursor->platform()) {
    cursor->SetPlatformCursor(cursor->platform());
  } else {
    const wchar_t* cursor_id = GetCursorId(*cursor);
    PlatformCursor platform_cursor = LoadCursor(nullptr, cursor_id);
    if (!platform_cursor && !g_cursor_resource_module_name.Get().empty()) {
      platform_cursor = LoadCursor(
          GetModuleHandle(g_cursor_resource_module_name.Get().c_str()),
          cursor_id);
    }
    cursor->SetPlatformCursor(platform_cursor);
  }
}

// static
void CursorLoaderWin::SetCursorResourceModule(
    const base::string16& module_name) {
  g_cursor_resource_module_name.Get() = module_name;
}

}  // namespace ui

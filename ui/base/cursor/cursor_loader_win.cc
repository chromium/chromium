// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_win.h"

#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "ui/base/cursor/cursor.h"
#include "ui/resources/grit/ui_unscaled_resources.h"

#include <windows.h>

namespace ui {

namespace {

base::LazyInstance<base::string16>::DestructorAtExit
    g_cursor_resource_module_name;

const wchar_t* GetCursorId(gfx::NativeCursor native_cursor) {
  switch (native_cursor.native_type()) {
    case CursorType::kNull:
      return IDC_ARROW;
    case CursorType::kPointer:
      return IDC_ARROW;
    case CursorType::kCross:
      return IDC_CROSS;
    case CursorType::kHand:
      return IDC_HAND;
    case CursorType::kIBeam:
      return IDC_IBEAM;
    case CursorType::kWait:
      return IDC_WAIT;
    case CursorType::kHelp:
      return IDC_HELP;
    case CursorType::kEastResize:
      return IDC_SIZEWE;
    case CursorType::kNorthResize:
      return IDC_SIZENS;
    case CursorType::kNorthEastResize:
      return IDC_SIZENESW;
    case CursorType::kNorthWestResize:
      return IDC_SIZENWSE;
    case CursorType::kSouthResize:
      return IDC_SIZENS;
    case CursorType::kSouthEastResize:
      return IDC_SIZENWSE;
    case CursorType::kSouthWestResize:
      return IDC_SIZENESW;
    case CursorType::kWestResize:
      return IDC_SIZEWE;
    case CursorType::kNorthSouthResize:
      return IDC_SIZENS;
    case CursorType::kEastWestResize:
      return IDC_SIZEWE;
    case CursorType::kNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case CursorType::kNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case CursorType::kMove:
      return IDC_SIZEALL;
    case CursorType::kProgress:
      return IDC_APPSTARTING;
    case CursorType::kNoDrop:
      return IDC_NO;
    case CursorType::kNotAllowed:
      return IDC_NO;
    case CursorType::kColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case CursorType::kRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case CursorType::kMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case CursorType::kMiddlePanningVertical:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_VERTICAL);
    case CursorType::kMiddlePanningHorizontal:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_HORIZONTAL);
    case CursorType::kEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case CursorType::kNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case CursorType::kNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case CursorType::kNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case CursorType::kSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case CursorType::kSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case CursorType::kSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case CursorType::kWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case CursorType::kVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case CursorType::kCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case CursorType::kZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case CursorType::kZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case CursorType::kGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case CursorType::kGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
    case CursorType::kCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case CursorType::kAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case CursorType::kNone:
      return MAKEINTRESOURCE(IDC_CURSOR_NONE);
    case CursorType::kContextMenu:
    case CursorType::kCustom:
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

void CursorLoaderWin::LoadImageCursor(CursorType id,
                                      int resource_id,
                                      const gfx::Point& hot) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::LoadAnimatedCursor(CursorType id,
                                         int resource_id,
                                         const gfx::Point& hot,
                                         int frame_delay_ms) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::UnloadAll() {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::SetPlatformCursor(gfx::NativeCursor* cursor) {
  if (cursor->native_type() != CursorType::kCustom) {
    if (cursor->platform()) {
      cursor->SetPlatformCursor(cursor->platform());
    } else {
      const wchar_t* cursor_id = GetCursorId(*cursor);
      PlatformCursor platform_cursor = LoadCursor(NULL, cursor_id);
      if (!platform_cursor && !g_cursor_resource_module_name.Get().empty()) {
        platform_cursor = LoadCursor(
            GetModuleHandle(g_cursor_resource_module_name.Get().c_str()),
                            cursor_id);
      }
      cursor->SetPlatformCursor(platform_cursor);
    }
  }
}

// static
void CursorLoaderWin::SetCursorResourceModule(
    const base::string16& module_name) {
  g_cursor_resource_module_name.Get() = module_name;
}

}  // namespace ui

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_TYPES_CURSOR_TYPES_H_
#define UI_BASE_CURSOR_TYPES_CURSOR_TYPES_H_

namespace ui {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui_base.web
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: CursorType
enum class CursorType {
  kNull = -1,  // Set to -1 to maintain back-compat with PP API defines.
  kPointer,
  kCross,
  kHand,
  kIBeam,
  kWait,
  kHelp,
  kEastResize,
  kNorthResize,
  kNorthEastResize,
  kNorthWestResize,
  kSouthResize,
  kSouthEastResize,
  kSouthWestResize,
  kWestResize,
  kNorthSouthResize,
  kEastWestResize,
  kNorthEastSouthWestResize,
  kNorthWestSouthEastResize,
  kColumnResize,
  kRowResize,
  kMiddlePanning,
  kEastPanning,
  kNorthPanning,
  kNorthEastPanning,
  kNorthWestPanning,
  kSouthPanning,
  kSouthEastPanning,
  kSouthWestPanning,
  kWestPanning,
  kMove,
  kVerticalText,
  kCell,
  kContextMenu,
  kAlias,
  kProgress,
  kNoDrop,
  kCopy,
  kNone,
  kNotAllowed,
  kZoomIn,
  kZoomOut,
  kGrab,
  kGrabbing,
  kMiddlePanningVertical,
  kMiddlePanningHorizontal,
  kCustom,
  kDndNone,
  kDndMove,
  kDndCopy,
  kDndLink,
  kMaxValue = kDndLink
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_TYPES_CURSOR_TYPES_H_

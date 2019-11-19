// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/hit_test_x11.h"

#include "ui/base/hit_test.h"

namespace ui {

namespace {

// These constants are said to be defined in the Extended Window Manager Hints
// standard but to be not found in any headers...
constexpr int kSizeTopLeft = 0;
constexpr int kSizeTop = 1;
constexpr int kSizeTopRight = 2;
constexpr int kSizeRight = 3;
constexpr int kSizeBottomRight = 4;
constexpr int kSizeBottom = 5;
constexpr int kSizeBottomLeft = 6;
constexpr int kSizeLeft = 7;
constexpr int kMove = 8;

}  // namespace

int HitTestToWmMoveResizeDirection(int hittest) {
  switch (hittest) {
    case HTBOTTOM:
      return kSizeBottom;
    case HTBOTTOMLEFT:
      return kSizeBottomLeft;
    case HTBOTTOMRIGHT:
      return kSizeBottomRight;
    case HTCAPTION:
      return kMove;
    case HTLEFT:
      return kSizeLeft;
    case HTRIGHT:
      return kSizeRight;
    case HTTOP:
      return kSizeTop;
    case HTTOPLEFT:
      return kSizeTopLeft;
    case HTTOPRIGHT:
      return kSizeTopRight;
    default:
      return -1;
  }
}

}  // namespace ui

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/hit_test.h"

namespace ui {

bool IsResizingComponent(int component) {
  switch (component) {
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTLEFT:
    case HTRIGHT:
    case HTTOP:
    case HTTOPLEFT:
    case HTTOPRIGHT:
      return true;
    default:
      return false;
  }
}

bool CanPerformDragOrResize(int component) {
  return component == HTCAPTION || IsResizingComponent(component);
}

}  // namespace ui

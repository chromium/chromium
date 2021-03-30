// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_HIT_TEST_X11_H_
#define UI_BASE_HIT_TEST_X11_H_

#include "base/component_export.h"

namespace ui {

// Converts a HitTestCompat into an X11 direction recognisable by
// NET_WM_MOVERESIZE event.  Returns -1 if no conversion is possible.
COMPONENT_EXPORT(UI_BASE) int HitTestToWmMoveResizeDirection(int hittest);

}  // namespace ui

#endif  // UI_BASE_HIT_TEST_X11_H_

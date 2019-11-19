// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_TEST_IDS_H_
#define UI_COLOR_COLOR_TEST_IDS_H_

#include "ui/color/color_id.h"

namespace ui {

// Test-only color IDs.
enum TestColorIds : ColorId {
  kTestColorsStart = kUiColorsEnd,

  kColorTest0 = kTestColorsStart,
  kColorTest1,
  kColorTest2,

  kTestColorsEnd,
};

// Test-only color set IDs.
enum TestColorSetIds : ColorSetId {
  kTestColorSetsStart = kUiColorSetsEnd,

  kColorSetTest0 = kTestColorSetsStart,
  kColorSetTest1,
  kColorSetTest2,

  kTestColorSetsEnd,
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_TEST_IDS_H_

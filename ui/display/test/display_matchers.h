// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_DISPLAY_MATCHERS_H_
#define UI_DISPLAY_TEST_DISPLAY_MATCHERS_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/types/display_mode.h"

namespace display {

// Matcher for DisplayMode size and refresh rate.
testing::Matcher<const DisplayMode&> IsDisplayMode(int width,
                                                   int height,
                                                   float refresh_rate = 60.0f);

}  // namespace display

#endif  // UI_DISPLAY_TEST_DISPLAY_MATCHERS_H_

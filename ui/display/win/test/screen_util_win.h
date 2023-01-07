// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_TEST_SCREEN_UTIL_WIN_H_
#define UI_DISPLAY_WIN_TEST_SCREEN_UTIL_WIN_H_

#include <windows.h>

#include <string>

namespace gfx {
class Rect;
}  // namespace gfx

namespace display {
namespace win {
namespace test {

// Creates a MONITORINFOEX from |monitor|, |work|, and |device_name|.
MONITORINFOEX CreateMonitorInfo(const gfx::Rect& monitor,
                                const gfx::Rect& work,
                                const std::wstring& device_name);

}  // namespace test
}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_TEST_SCREEN_UTIL_WIN_H_

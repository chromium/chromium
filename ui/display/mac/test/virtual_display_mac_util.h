// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_TEST_VIRTUAL_DISPLAY_MAC_UTIL_H_
#define UI_DISPLAY_MAC_TEST_VIRTUAL_DISPLAY_MAC_UTIL_H_

namespace gfx {
class Size;
}  // namespace gfx

namespace display::test {

// This interface creates system-level virtual displays to support the automated
// integration testing of display information and window placement APIs in
// multi-screen device environments. It updates the displays that the normal mac
// screen impl sees, but not `TestScreenMac`.
class VirtualDisplayMacUtil {
 public:
  static void AddDisplay(int display_id, const gfx::Size& size);
  static void RemoveDisplay(int display_id);
  static bool ShouldSkip();
};

}  // namespace display::test

#endif  // UI_DISPLAY_MAC_TEST_VIRTUAL_DISPLAY_MAC_UTIL_H_

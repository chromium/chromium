// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_VIRTUAL_DISPLAY_UTIL_H_
#define UI_DISPLAY_TEST_VIRTUAL_DISPLAY_UTIL_H_

#include <memory>
#include "ui/display/display_observer.h"

namespace display {
class Screen;

namespace test {
struct DisplayParams;

// This interface creates system-level virtual displays to support the automated
// integration testing of display information and window management APIs in
// multi-screen device environments. It updates displays that display::Screen
// impl sees, but is generally not compatible with `TestScreen` subclasses.
class VirtualDisplayUtil {
 public:
  virtual ~VirtualDisplayUtil() = default;

  // Creates an instance for the current platform if available. Returns nullptr
  // on unsupported platforms (i.e. not implemented, or host is missing
  // requirements).
  static std::unique_ptr<VirtualDisplayUtil> TryCreate(Screen* screen);

  // `id` is used to uniquely identify the virtual display. This function
  // returns the generated display::Display id, which can be used with the
  // Screen instance or passed to `RemoveDisplay`.
  virtual int64_t AddDisplay(uint8_t id,
                             const DisplayParams& display_params) = 0;
  // Remove a virtual display corresponding to the specified display ID.
  virtual void RemoveDisplay(int64_t display_id) = 0;
  // Remove all added virtual displays.
  virtual void ResetDisplays() = 0;

  // Supported Display configurations.
  static const DisplayParams k1920x1080;
  static const DisplayParams k1024x768;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_TEST_VIRTUAL_DISPLAY_UTIL_H_

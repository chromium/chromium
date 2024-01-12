// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_PINNED_MODE_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_PINNED_MODE_EXTENSION_H_

#include "base/component_export.h"

namespace ui {

class PlatformWindow;

// A pinned mode extension that platforms can use to add support for pinned
// mode operations which are used e.g. in EDU tests / quizzes.
class COMPONENT_EXPORT(PLATFORM_WINDOW) PinnedModeExtension {
 public:
  // Pins/locks a window to the screen so that the user cannot do anything
  // else before the mode is released. If trusted is set, it is an invocation
  // from a trusted app like a school test mode app.
  virtual void Pin(bool trusted) = 0;

  // Releases the pinned mode and allows the user to do other things again.
  virtual void Unpin() = 0;

  // Returns true if the configure event can handle
  // PlatformWindowState::kPinnedFullscreen/kTrustedPinnedFullscreen.
  virtual bool SupportsConfigurePinnedState() const = 0;

 protected:
  virtual ~PinnedModeExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  static void SetPinnedModeExtension(PlatformWindow* window,
                                     PinnedModeExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
PinnedModeExtension* GetPinnedModeExtension(const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_PINNED_MODE_EXTENSION_H_

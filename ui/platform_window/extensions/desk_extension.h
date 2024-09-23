// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_DESK_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_DESK_EXTENSION_H_

#include <string>

#include "base/component_export.h"

namespace ui {

class PlatformWindow;

// A desk extension that platforms can use to add support for virtual desktop.
// The APIs currently match with what ash provides from desks_controller to
// support "Move window to desk" menu in LaCrOS.
// TODO(crbug.com/40769556): Support virtual desktop protocol for linux/wayland
// as well.
class COMPONENT_EXPORT(PLATFORM_WINDOW) DeskExtension {
 public:
  // Returns the current number of desks.
  virtual int GetNumberOfDesks() const = 0;

  // Returns the active desk index for window.
  virtual int GetActiveDeskIndex() const = 0;

  // Returns the name of the desk at |index|.
  virtual std::u16string GetDeskName(int index) const = 0;

  // Requests the underneath platform to send the window to a desk at |index|.
  // |index| must be valid.
  virtual void SendToDeskAtIndex(int index) = 0;

 protected:
  virtual ~DeskExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetDeskExtension(PlatformWindow* window, DeskExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
DeskExtension* GetDeskExtension(const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_DESK_EXTENSION_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_SYSTEM_MODAL_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_SYSTEM_MODAL_EXTENSION_H_

#include "base/component_export.h"

namespace ui {

class PlatformWindow;

class COMPONENT_EXPORT(PLATFORM_WINDOW) SystemModalExtension {
 public:
  virtual void SetSystemModal(bool modal) = 0;

 protected:
  virtual ~SystemModalExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  static void SetSystemModalExtension(PlatformWindow* window,
                                      SystemModalExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
SystemModalExtension* GetSystemModalExtension(const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_SYSTEM_MODAL_EXTENSION_H_
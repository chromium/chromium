// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class PlatformWindow;
class X11ExtensionDelegate;

// Linux extensions that linux platforms can use to extend the platform windows
// APIs. Please refer to README for more details.
//
// The extension mustn't be called until PlatformWindow is initialized.
class COMPONENT_EXPORT(PLATFORM_WINDOW) X11Extension {
 public:
  // Returns whether an XSync extension is available at the current platform.
  virtual bool IsSyncExtensionAvailable() const = 0;

  // Returns a best-effort guess as to whether the WM is tiling (true) or
  // stacking (false).
  virtual bool IsWmTiling() const = 0;

  // Handles CompleteSwapAfterResize event coming from the compositor observer.
  virtual void OnCompleteSwapAfterResize() = 0;

  // Returns the current bounds in terms of the X11 Root Window including the
  // borders provided by the window manager (if any).
  virtual gfx::Rect GetXRootWindowOuterBounds() const = 0;

  // Asks X11 to lower the Xwindow down the stack so that it does not obscure
  // any sibling windows.
  virtual void LowerXWindow() = 0;

  // Forces this window to be unmanaged by the window manager if
  // |override_redirect| is true.
  virtual void SetOverrideRedirect(bool override_redirect) = 0;

  // Returns true if SetOverrideRedirect() would be compatible with the WM.
  virtual bool CanResetOverrideRedirect() const = 0;

  virtual void SetX11ExtensionDelegate(X11ExtensionDelegate* delegate) = 0;

 protected:
  virtual ~X11Extension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetX11Extension(PlatformWindow* platform_window,
                       X11Extension* x11_extensions);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
X11Extension* GetX11Extension(const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_LINUX_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_LINUX_H_

#include "base/component_export.h"
#include "ui/base/buildflags.h"
#include "ui/platform_window/platform_window_delegate_base.h"

#if BUILDFLAG(USE_ATK)
using AtkKeyEventStruct = struct _AtkKeyEventStruct;
#endif

class SkPath;

namespace gfx {
class Size;
}

namespace ui {

// This is an optional linux delegate interface, which should be implemented by
// linux-based platforms. It contains both Wayland and X11 specific and common
// interfaces.
class COMPONENT_EXPORT(PLATFORM_WINDOW) PlatformWindowDelegateLinux
    : public PlatformWindowDelegateBase {
 public:
  PlatformWindowDelegateLinux();
  ~PlatformWindowDelegateLinux() override;

  // Notifies the delegate that the window got mapped in the X server. Wayland
  // does not support this interface.
  virtual void OnXWindowMapped();
  virtual void OnXWindowUnmapped();

  // Notifies if the PlatformWindow looses a mouse grab. This can be useful for
  // Wayland or X11. Both of them provide pointer enter and leave notifications,
  // which non-ozone X11 (just an example) use to be using to notify about lost
  // pointer grab along with explicit grabs. Wayland also has this technique.
  // However, explicit grab is available only for popup (menu) windows.
  virtual void OnLostMouseGrab();

  // Notifies the delegate if the PlatformWindow has changed the workspace it is
  // located in.
  virtual void OnWorkspaceChanged();

  // Returns a mask to be used to clip the window for the given
  // size. This is used to create the non-rectangular window shape.
  virtual void GetWindowMask(const gfx::Size& size, SkPath* window_mask);

#if BUILDFLAG(USE_ATK)
  virtual bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event);
#endif
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_LINUX_H_

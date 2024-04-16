// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_DELEGATE_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_DELEGATE_H_

#include "base/component_export.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(USE_ATK)
using AtkKeyEventStruct = struct _AtkKeyEventStruct;
#endif

namespace ui {

class COMPONENT_EXPORT(PLATFORM_WINDOW) X11ExtensionDelegate {
 public:
  // Notifies if the PlatformWindow looses a mouse grab. This can be useful
  // for Wayland or X11. Both of them provide pointer enter and leave
  // notifications, which non-ozone X11 (just an example) use to be using to
  // notify about lost pointer grab along with explicit grabs. Wayland also
  // has this technique. However, explicit grab is available only for popup
  // (menu) windows.
  virtual void OnLostMouseGrab() = 0;

#if BUILDFLAG(USE_ATK)
  // Notifies an ATK key event to be processed. The transient parameter will be
  // true if the event target is a transient window (e.g. a modal dialog)
  // "hanging" from our window. Return true to stop propagation of the original
  // key event.
  virtual bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event,
                             bool transient) = 0;
#endif

  // Returns true if this window should be in a forced override-redirect state
  // (not managed by the window manager).
  virtual bool IsOverrideRedirect() const = 0;

  // Returns guessed size we will have after the switch to/from fullscreen:
  // - (may) avoid transient states
  // - works around Flash content which expects to have the size updated
  //   synchronously.
  // See https://crbug.com/361408
  // TODO(crbug.com/40136193): remove this and let this managed by
  // X11ScreenOzone that Ozone's X11Window should be able to access instead.
  // This delegate method is required as non-Ozone/X11 is not able to determine
  // matching display as it requires to know bounds in dip.
  virtual gfx::Rect GetGuessedFullScreenSizeInPx() const = 0;

 protected:
  virtual ~X11ExtensionDelegate() = default;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_X11_EXTENSION_DELEGATE_H_

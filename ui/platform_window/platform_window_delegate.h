// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

class SkPath;

namespace ui {

class Event;

enum class PlatformWindowState {
  kUnknown,
  kMaximized,
  kMinimized,
  kNormal,
  kFullScreen,
};

class COMPONENT_EXPORT(PLATFORM_WINDOW) PlatformWindowDelegate {
 public:
  PlatformWindowDelegate();
  virtual ~PlatformWindowDelegate();

  // Note that |new_bounds| is in physical screen coordinates.
  virtual void OnBoundsChanged(const gfx::Rect& new_bounds) = 0;

  // Note that |damaged_region| is in the platform-window's coordinates, in
  // physical pixels.
  virtual void OnDamageRect(const gfx::Rect& damaged_region) = 0;

  virtual void DispatchEvent(Event* event) = 0;

  virtual void OnCloseRequest() = 0;
  virtual void OnClosed() = 0;

  virtual void OnWindowStateChanged(PlatformWindowState new_state) = 0;

  virtual void OnLostCapture() = 0;

  virtual void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) = 0;

  // Notifies the delegate that the widget is about to be destroyed.
  virtual void OnWillDestroyAcceleratedWidget() = 0;

  // Notifies the delegate that the widget cannot be used anymore until
  // a new widget is made available through OnAcceleratedWidgetAvailable().
  // Must not be called when the PlatformWindow is being destroyed.
  virtual void OnAcceleratedWidgetDestroyed() = 0;

  virtual void OnActivationChanged(bool active) = 0;

  // Requests size constraints for the PlatformWindow.
  virtual base::Optional<gfx::Size> GetMinimumSizeForWindow();
  virtual base::Optional<gfx::Size> GetMaximumSizeForWindow();

  // Returns a mask to be used to clip the window for the size of
  // |WindowTreeHost::GetBoundsInPixels|.
  // This is used to create the non-rectangular window shape.
  virtual SkPath GetWindowMaskForWindowShapeInPixels();

  // Called when the location of mouse pointer entered the window.  This is
  // different from ui::ET_MOUSE_ENTERED which may not be generated when mouse
  // is captured either by implicitly or explicitly.
  virtual void OnMouseEnter() = 0;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_

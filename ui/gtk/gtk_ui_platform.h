// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_PLATFORM_H_
#define UI_GTK_GTK_UI_PLATFORM_H_

#include "base/functional/callback_forward.h"
#include "ui/events/event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gtk/gtk_compat.h"

using GtkWindow = struct _GtkWindow;
using GtkWidget = struct _GtkWidget;
using GdkWindow = struct _GdkWindow;

namespace ui {
class LinuxInputMethodContext;
class LinuxInputMethodContextDelegate;
}  // namespace ui

namespace gtk {

// GtkUiPlatform encapsulates platform-specific functionalities required by
// a Gtk-based LinuxUI implementation.
class GtkUiPlatform {
 public:
  virtual ~GtkUiPlatform() = default;

  // Called when the GtkUi instance initialization process finished. |widget| is
  // a dummy window passed in for context.
  virtual void OnInitialized(GtkWidget* widget) = 0;

  // Gets the GdkKeymap instance, which is used to translate KeyEvents into
  // GdkEvents before filtering them through GtkIM API.
  virtual GdkKeymap* GetGdkKeymap() = 0;

  // Gets the GDK key event state for a KeyEvent.
  virtual GdkModifierType GetGdkKeyEventState(
      const ui::KeyEvent& key_event) = 0;

  // Gets the GDK key event group for a KeyEvent.
  virtual int GetGdkKeyEventGroup(const ui::KeyEvent& key_event) = 0;

  // Creates/Gets a GdkWindow out of a Aura window id. Caller owns the returned
  // object. This function is meant to be used in GtkIM-based IME implementation
  // and is supported only in X11 backend (both Aura and Ozone).
  virtual GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) = 0;

  // Gtk dialog windows must be set transient for the browser window. This
  // function abstracts away such functionality.
  virtual bool SetGtkWidgetTransientFor(GtkWidget* widget,
                                        gfx::AcceleratedWidget parent) = 0;
  virtual void ClearTransientFor(gfx::AcceleratedWidget parent) = 0;

  // Presents |window|, doing all the necessary platform-specific operations
  // needed, if any.
  virtual void ShowGtkWindow(GtkWindow* window) = 0;

  // Creates a new IME context or may return nullptr.
  virtual std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const = 0;

  // If true, the device scale factor should be multiplied by the font scale. If
  // false, the font size should be multiplied by the font scale.
  virtual bool IncludeFontScaleInDeviceScale() const = 0;
};

}  // namespace gtk

#endif  // UI_GTK_GTK_UI_PLATFORM_H_

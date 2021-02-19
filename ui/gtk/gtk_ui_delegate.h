// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_DELEGATE_H_
#define UI_GTK_GTK_UI_DELEGATE_H_

#include "base/component_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gtk/gtk_buildflags.h"

using GdkKeymap = struct _GdkKeymap;
using GtkWindow = struct _GtkWindow;

#if BUILDFLAG(GTK_VERSION) == 3
using GdkWindow = struct _GdkWindow;
#else
using GdkWindow = struct _GdkSurface;
#endif

namespace ui {

// GtkUiDelegate encapsulates platform-specific functionalities required by
// a Gtk-based LinuxUI implementation. The main goal of this interface is to
// make GtkUi platform agnostic, moving the platform specifics to lower level
// layers (e.g: ozone). Linux backends (e.g: ozone/x11, aura/x11, ozone/wayland)
// must provide a GtkUiDelegate implementation and inject its singleton instance
// of it via |SetInstance| in order to be able to use GtkUi.
class COMPONENT_EXPORT(GTK) GtkUiDelegate {
 public:
  virtual ~GtkUiDelegate() = default;

  // Sets the singleton delegate instance to be used by GtkUi. This makes it
  // possible for ozone-based backends, for example, to inject the GtkUiDelegate
  // object without polluting Ozone API, since just a small subset of ozone
  // backends make use of GtkUi. This pointer is not owned, and if this method
  // is called a second time, the first instance is not deleted.
  static void SetInstance(GtkUiDelegate* instance);

  // Returns the current active instance.
  static GtkUiDelegate* instance();

  // Called when the GtkUi instance initialization process finished.
  virtual void OnInitialized() = 0;

  // Gets the GdkKeymap instance, which is used to translate KeyEvents into
  // GdkEvents before filtering them through GtkIM API.
  virtual GdkKeymap* GetGdkKeymap() = 0;

  // Creates/Gets a GdkWindow out of a Aura window id. Caller owns the returned
  // object. This function is meant to be used in GtkIM-based IME implementation
  // and is supported only in X11 backend (both Aura and Ozone).
  virtual GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) = 0;

  // Gtk dialog windows must be set transient for the browser window. This
  // function abstracts away such functionality.
  virtual bool SetGdkWindowTransientFor(GdkWindow* window,
                                        gfx::AcceleratedWidget parent) = 0;
  virtual void ClearTransientFor(gfx::AcceleratedWidget parent) = 0;

  // Presents |window|, doing all the necessary platform-specific operations
  // needed, if any.
  virtual void ShowGtkWindow(GtkWindow* window) = 0;

  // Get the current keyboard modifier state.
  virtual int GetGdkKeyState() = 0;
};

}  // namespace ui

#endif  // UI_GTK_GTK_UI_DELEGATE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_X_GTK_EVENT_LOOP_X11_H_
#define UI_GTK_X_GTK_EVENT_LOOP_X11_H_

#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/gtk/gtk_compat.h"

namespace gtk {

class GtkEventLoopX11 {
 public:
  explicit GtkEventLoopX11(GtkWidget* widget);
  ~GtkEventLoopX11();

  GtkEventLoopX11(const GtkEventLoopX11&) = delete;
  GtkEventLoopX11& operator=(const GtkEventLoopX11&) = delete;

 private:
  // This state is only used on GTK4.
  GdkSurface* surface_ = nullptr;
  gulong signal_id_ = 0;

  // Only called on GTK3.
  static void DispatchGdkEvent(GdkEvent* gdk_event, gpointer);

  // Only called on GTK4.
  CHROMEG_CALLBACK_0(GtkEventLoopX11, gboolean, OnEvent, GdkEvent*);
};

}  // namespace gtk

#endif  // UI_GTK_X_GTK_EVENT_LOOP_X11_H_

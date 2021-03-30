// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_X_GTK_EVENT_LOOP_X11_H_
#define UI_GTK_X_GTK_EVENT_LOOP_X11_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/gtk/gtk_buildflags.h"

namespace ui {

class GtkEventLoopX11 {
 public:
  explicit GtkEventLoopX11(GtkWidget* widget);
  ~GtkEventLoopX11();

  GtkEventLoopX11(const GtkEventLoopX11&) = delete;
  GtkEventLoopX11& operator=(const GtkEventLoopX11&) = delete;

 private:
#if BUILDFLAG(GTK_VERSION) >= 4
  CHROMEG_CALLBACK_0(GtkEventLoopX11, gboolean, OnEvent, GdkEvent*);
  GdkSurface* surface_ = nullptr;
  gulong signal_id_ = 0;
#else
  static void DispatchGdkEvent(GdkEvent* gdk_event, gpointer);
#endif
};

}  // namespace ui

#endif  // UI_GTK_X_GTK_EVENT_LOOP_X11_H_

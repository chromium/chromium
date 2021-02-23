// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_X_GTK_EVENT_LOOP_X11_H_
#define UI_GTK_X_GTK_EVENT_LOOP_X11_H_

#include <gdk/gdk.h>

#include "ui/base/glib/glib_integers.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}

namespace ui {

class GtkEventLoopX11 {
 public:
  static GtkEventLoopX11* EnsureInstance();

  GtkEventLoopX11(const GtkEventLoopX11&) = delete;
  GtkEventLoopX11& operator=(const GtkEventLoopX11&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<GtkEventLoopX11>;

  GtkEventLoopX11();
  ~GtkEventLoopX11();

  static void DispatchGdkEvent(GdkEvent* gdk_event, gpointer);
  static void ProcessGdkEventKey(GdkEvent* gdk_event);
};

}  // namespace ui

#endif  // UI_GTK_X_GTK_EVENT_LOOP_X11_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_TYPES_H_
#define UI_GTK_GTK_TYPES_H_

#include <gdk/gdk.h>

#include "ui/gtk/gtk_buildflags.h"

// This file provides types that are only available in specific versions of GTK.

extern "C" {
#if BUILDFLAG(GTK_VERSION) == 3
enum GskRenderNodeType : int;

using GskRenderNode = struct _GskRenderNode;
#else
enum GtkWidgetHelpType : int;

using GtkWidgetPath = struct _GtkWidgetPath;
using GtkContainer = struct _GtkContainer;
using GdkEventKey = struct _GdkEventKey;
using GdkWindow = struct _GdkWindow;
using GdkKeymap = struct _GdkKeymap;

struct _GdkEventKey {
  GdkEventType type;
  GdkWindow* window;
  gint8 send_event;
  guint32 time;
  guint state;
  guint keyval;
  gint length;
  gchar* string;
  guint16 hardware_keycode;
  guint8 group;
  guint is_modifier : 1;
};
#endif
}

#endif  // UI_GTK_GTK_TYPES_H_

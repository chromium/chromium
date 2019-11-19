// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GLIB_GLIB_INTEGERS_H_
#define UI_BASE_GLIB_GLIB_INTEGERS_H_

#include <cstdint>

// GLib/GObject/Gtk all use their own integer typedefs. They are copied here
// for forward declaration reasons so we don't pull in all of glib/gtypes.h
// when we just need a gpointer.
typedef char gchar;
typedef short gshort;
typedef long glong;
typedef int gint;
typedef gint gboolean;
typedef unsigned char guchar;
typedef unsigned short gushort;
typedef unsigned long gulong;
typedef unsigned int guint;
typedef double gdouble;

typedef int64_t gint64;

typedef void* gpointer;
typedef const void *gconstpointer;

#endif  // UI_BASE_GLIB_GLIB_INTEGERS_H_

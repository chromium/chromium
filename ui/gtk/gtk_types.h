// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_TYPES_H_
#define UI_GTK_GTK_TYPES_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

// This file provides types that are only available in specific versions of GTK.

// This struct uses doubles in Gtk3, but floats in Gtk4.
#define GdkRGBA Do_not_use_GdkRGBA_because_it_is_not_ABI_compatible

extern "C" {
#if GTK_MAJOR_VERSION == 3
using GskRenderNodeType = enum {
  GSK_NOT_A_RENDER_NODE = 0,
  GSK_CONTAINER_NODE,
  GSK_CAIRO_NODE,
  GSK_COLOR_NODE,
  GSK_LINEAR_GRADIENT_NODE,
  GSK_REPEATING_LINEAR_GRADIENT_NODE,
  GSK_RADIAL_GRADIENT_NODE,
  GSK_REPEATING_RADIAL_GRADIENT_NODE,
  GSK_CONIC_GRADIENT_NODE,
  GSK_BORDER_NODE,
  GSK_TEXTURE_NODE,
  GSK_INSET_SHADOW_NODE,
  GSK_OUTSET_SHADOW_NODE,
  GSK_TRANSFORM_NODE,
  GSK_OPACITY_NODE,
  GSK_COLOR_MATRIX_NODE,
  GSK_REPEAT_NODE,
  GSK_CLIP_NODE,
  GSK_ROUNDED_CLIP_NODE,
  GSK_SHADOW_NODE,
  GSK_BLEND_NODE,
  GSK_CROSS_FADE_NODE,
  GSK_TEXT_NODE,
  GSK_BLUR_NODE,
  GSK_DEBUG_NODE,
  GSK_GL_SHADER_NODE
};

enum GdkMemoryFormat : int;

using GskRenderNode = struct _GskRenderNode;
using GtkIconPaintable = struct _GtkIconPaintable;
using GdkTexture = struct _GdkTexture;
using GdkSnapshot = struct _GdkSnapshot;
using GtkSnapshot = GdkSnapshot;
using GdkPaintable = struct _GdkPaintable;
using GtkNative = struct _GtkNative;
using GdkSurface = struct _GdkSurface;
using GdkToplevel = struct _GdkToplevel;

constexpr GdkMemoryFormat GDK_MEMORY_B8G8R8A8 = static_cast<GdkMemoryFormat>(3);

constexpr GdkModifierType GDK_ALT_MASK = GDK_MOD1_MASK;
#elif GTK_MAJOR_VERSION == 4
enum GtkWidgetHelpType : int;
enum GtkWindowType : int;

using GtkWidgetPath = struct _GtkWidgetPath;
using GtkContainer = struct _GtkContainer;
using GdkEventKey = struct _GdkEventKey;
using GdkWindow = struct _GdkWindow;
using GdkKeymap = struct _GdkKeymap;
using GtkIconInfo = struct _GtkIconInfo;
using GdkScreen = struct _GdkScreen;
using GdkColor = struct _GdkColor;

using GdkEventFunc = void (*)(GdkEvent* event, gpointer data);

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

struct _GdkColor {
  guint32 pixel;
  guint16 red;
  guint16 green;
  guint16 blue;
};

constexpr int GTK_ICON_LOOKUP_USE_BUILTIN = 1 << 2;
constexpr int GTK_ICON_LOOKUP_GENERIC_FALLBACK = 1 << 3;
constexpr int GTK_ICON_LOOKUP_FORCE_SIZE = 1 << 4;

constexpr auto GTK_WINDOW_TOPLEVEL = static_cast<GtkWindowType>(0);
#else
#error "Unsupported GTK version"
#endif
}

#endif  // UI_GTK_GTK_TYPES_H_

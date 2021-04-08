// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_COMPAT_H_
#define UI_GTK_GTK_COMPAT_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/version.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gtk/gtk_types.h"

extern "C" {
#include "ui/gtk/gdk.sigs"
#include "ui/gtk/gdk_pixbuf.sigs"
#include "ui/gtk/gio.sigs"
#include "ui/gtk/gsk.sigs"
#include "ui/gtk/gtk.sigs"
}

namespace gtk {

// Loads libgtk and related libraries and returns true on success.
COMPONENT_EXPORT(GTK) bool LoadGtk(int gtk_version);

const base::Version& GtkVersion();

// Returns true iff the runtime version of Gtk used meets
// |major|.|minor|.|micro|. LoadGtk() must have been called
// prior to calling this function.
bool GtkCheckVersion(int major, int minor = 0, int micro = 0);

// The below functions replace GTK functions whose interface has
// changed across versions, but whose (symbol) names have not.

void GtkInit(const std::vector<std::string>& args);

gfx::Insets GtkStyleContextGetPadding(GtkStyleContext* context);

gfx::Insets GtkStyleContextGetBorder(GtkStyleContext* context);

gfx::Insets GtkStyleContextGetMargin(GtkStyleContext* context);

bool GtkImContextFilterKeypress(GtkIMContext* context, GdkEventKey* event);

bool GtkFileChooserSetCurrentFolder(GtkFileChooser* dialog,
                                    const base::FilePath& path);

void GtkRenderIcon(GtkStyleContext* context,
                   cairo_t* cr,
                   GdkPixbuf* pixbuf,
                   GdkTexture* texture,
                   double x,
                   double y);

ScopedGObject<GListModel> Gtk4FileChooserGetFiles(GtkFileChooser* dialog);

ScopedGObject<GtkIconInfo> Gtk3IconThemeLookupByGicon(GtkIconTheme* theme,
                                                      GIcon* icon,
                                                      int size,
                                                      GtkIconLookupFlags flags);

ScopedGObject<GtkIconPaintable> Gtk4IconThemeLookupByGicon(
    GtkIconTheme* theme,
    GIcon* icon,
    int size,
    int scale,
    GtkTextDirection direction,
    GtkIconLookupFlags flags);

ScopedGObject<GtkIconPaintable> Gtk4IconThemeLookupIcon(
    GtkIconTheme* theme,
    const char* icon_name,
    const char* fallbacks[],
    int size,
    int scale,
    GtkTextDirection direction,
    GtkIconLookupFlags flags);

// generate_stubs cannot forward to C-style variadic functions, so the
// functions below wrap the corresponding GTK va_list functions.

void GtkStyleContextGet(GtkStyleContext* context, ...);

void GtkStyleContextGetStyle(GtkStyleContext* context, ...);

}  // namespace gtk

#endif

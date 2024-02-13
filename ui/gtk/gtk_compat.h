// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_COMPAT_H_
#define UI_GTK_GTK_COMPAT_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#include <optional>

#include "base/files/file_path.h"
#include "base/version.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gtk/gtk_types.h"

#if GTK_CHECK_VERSION(4, 1, 1)
#define UI_GTK_CONST const
#else
#define UI_GTK_CONST
#endif

extern "C" {
#include "ui/gtk/gdk.sigs"
#include "ui/gtk/gdk_pixbuf.sigs"
#include "ui/gtk/gio.sigs"
#include "ui/gtk/gsk.sigs"
#include "ui/gtk/gtk.sigs"
}

#define GDK_KEY_PRESS Do_not_use_GDK_KEY_PRESS_because_it_is_not_ABI_compatible
#define GDK_KEY_RELEASE \
  Do_not_use_GDK_KEY_RELEASE_because_it_is_not_ABI_compatible

using SkColor = uint32_t;

namespace gtk {

// Loads libgtk and related libraries and returns true on success.
bool LoadGtk();

const base::Version& GtkVersion();

// Returns true iff the runtime version of Gtk used meets
// |major|.|minor|.|micro|. LoadGtk() must have been called
// prior to calling this function.
bool GtkCheckVersion(uint32_t major, uint32_t minor = 0, uint32_t micro = 0);

// The below functions replace GTK functions whose interface has
// changed across versions, but whose (symbol) names have not.

[[nodiscard]] bool GtkInitCheck(int* argc, char** argv);

gfx::Insets GtkStyleContextGetPadding(GtkStyleContext* context);

gfx::Insets GtkStyleContextGetBorder(GtkStyleContext* context);

gfx::Insets GtkStyleContextGetMargin(GtkStyleContext* context);

SkColor GtkStyleContextGetColor(GtkStyleContext* context);

// Only available in Gtk3.
SkColor GtkStyleContextGetBackgroundColor(GtkStyleContext* context);

// Only available in Gtk3.
std::optional<SkColor> GtkStyleContextLookupColor(GtkStyleContext* context,
                                                  const gchar* color_name);

bool GtkImContextFilterKeypress(GtkIMContext* context, GdkEventKey* event);

bool GtkFileChooserSetCurrentFolder(GtkFileChooser* dialog,
                                    const base::FilePath& path);

void GtkRenderIcon(GtkStyleContext* context,
                   cairo_t* cr,
                   GdkPixbuf* pixbuf,
                   GdkTexture* texture,
                   double x,
                   double y);

GtkWidget* GtkToplevelWindowNew();

void GtkCssProviderLoadFromData(GtkCssProvider* css_provider,
                                const char* data,
                                gssize length);

ScopedGObject<GListModel> Gtk4FileChooserGetFiles(GtkFileChooser* dialog);

ScopedGObject<GtkIconInfo> Gtk3IconThemeLookupByGiconForScale(
    GtkIconTheme* theme,
    GIcon* icon,
    int size,
    int scale,
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

// These variadic functions do not have corresponding va_list equivalents,
// so instances with only a fixed set of arguments are provided.

GtkWidget* GtkFileChooserDialogNew(const gchar* title,
                                   GtkWindow* parent,
                                   GtkFileChooserAction action,
                                   const gchar* first_button_text,
                                   GtkResponseType first_response,
                                   const gchar* second_button_text,
                                   GtkResponseType second_response);

GtkTreeStore* GtkTreeStoreNew(GType type);

// These functions have dropped "const" in their signatures, so cannot be
// declared in *.sigs.

GdkEventType GdkEventGetEventType(GdkEvent* event);

guint32 GdkEventGetTime(GdkEvent* event);

// Some enum values have changed between versions.

GdkEventType GdkKeyPress();

GdkEventType GdkKeyRelease();

}  // namespace gtk

#endif  // UI_GTK_GTK_COMPAT_H_

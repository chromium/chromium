// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_compat.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/no_destructor.h"
#include "ui/gtk/gtk_stubs.h"

namespace gtk {

// IMPORTANT: All functions in this file that call dlsym()'ed
// functions should be annotated with DISABLE_CFI_ICALL.

namespace {

void* DlOpen(const char* library_name) {
  void* library = dlopen(library_name, RTLD_LAZY | RTLD_GLOBAL);
  CHECK(library);
  return library;
}

void* DlSym(void* library, const char* name) {
  void* symbol = dlsym(library, name);
  CHECK(symbol);
  return symbol;
}

template <typename T>
auto DlCast(void* symbol) {
  return reinterpret_cast<T*>(symbol);
}

void* GetLibGio() {
  static void* libgio = DlOpen("libgio-2.0.so.0");
  return libgio;
}

void* GetLibGdkPixbuf() {
  static void* libgdk_pixbuf = DlOpen("libgdk_pixbuf-2.0.so.0");
  return libgdk_pixbuf;
}

void* GetLibGdk3() {
  static void* libgdk3 = DlOpen("libgdk-3.so.0");
  return libgdk3;
}

void* GetLibGtk3() {
  static void* libgtk3 = DlOpen("libgtk-3.so.0");
  return libgtk3;
}

void* GetLibGtk4() {
  static void* libgtk4 = DlOpen("libgtk-4.so.1");
  return libgtk4;
}

void* GetLibGtk() {
  if (GtkCheckVersion(4))
    return GetLibGtk4();
  return GetLibGtk3();
}

gfx::Insets InsetsFromGtkBorder(const GtkBorder& border) {
  return gfx::Insets(border.top, border.left, border.bottom, border.right);
}

}  // namespace

bool LoadGtk(int gtk_version) {
  if (gtk_version < 4) {
    ui_gtk::InitializeGdk_pixbuf(GetLibGdkPixbuf());
    ui_gtk::InitializeGdk(GetLibGdk3());
    ui_gtk::InitializeGtk(GetLibGtk3());
  } else {
    // In GTK4 mode, we require some newer gio symbols that aren't available in
    // Ubuntu Xenial or Debian Stretch.  Fortunately, GTK4 itself depends on a
    // newer version of glib (which provides gio), so if we're using GTK4, we
    // can safely assume the system has the required gio symbols.
    ui_gtk::InitializeGio(GetLibGio());
    // In GTK4, libgtk provides all gdk_*, gsk_*, and gtk_* symbols.
    ui_gtk::InitializeGdk(GetLibGtk4());
    ui_gtk::InitializeGsk(GetLibGtk4());
    ui_gtk::InitializeGtk(GetLibGtk4());
  }
  return true;
}

const base::Version& GtkVersion() {
  static base::NoDestructor<base::Version> gtk_version(
      std::vector<uint32_t>{gtk_get_major_version(), gtk_get_minor_version(),
                            gtk_get_micro_version()});
  return *gtk_version;
}

bool GtkCheckVersion(int major, int minor, int micro) {
  return GtkVersion() >= base::Version({major, minor, micro});
}

DISABLE_CFI_ICALL
void GtkInit(const std::vector<std::string>& args) {
  static void* gtk_init = DlSym(GetLibGtk(), "gtk_init");
  if (GtkCheckVersion(4)) {
    DlCast<void()>(gtk_init)();
  } else {
    // gtk_init() modifies argv, so make a copy first.
    size_t args_chars = 0;
    for (const auto& arg : args)
      args_chars += arg.size() + 1;
    std::vector<char> args_copy(args_chars);
    std::vector<char*> argv;
    char* dst = args_copy.data();
    for (const auto& arg : args) {
      argv.push_back(strcpy(dst, arg.c_str()));
      dst += arg.size() + 1;
    }

    int gtk_argc = argv.size();
    char** gtk_argv = argv.data();
    {
      // http://crbug.com/423873
      ANNOTATE_SCOPED_MEMORY_LEAK;
      DlCast<void(int*, char***)>(gtk_init)(&gtk_argc, &gtk_argv);
    }
  }
}

DISABLE_CFI_ICALL
gfx::Insets GtkStyleContextGetPadding(GtkStyleContext* context) {
  static void* get_padding =
      DlSym(GetLibGtk(), "gtk_style_context_get_padding");
  GtkBorder padding;
  if (GtkCheckVersion(4)) {
    DlCast<void(GtkStyleContext*, GtkBorder*)>(get_padding)(context, &padding);
  } else {
    DlCast<void(GtkStyleContext*, GtkStateFlags, GtkBorder*)>(get_padding)(
        context, gtk_style_context_get_state(context), &padding);
  }
  return InsetsFromGtkBorder(padding);
}

DISABLE_CFI_ICALL
gfx::Insets GtkStyleContextGetBorder(GtkStyleContext* context) {
  static void* get_border = DlSym(GetLibGtk(), "gtk_style_context_get_border");
  GtkBorder border;
  if (GtkCheckVersion(4)) {
    DlCast<void(GtkStyleContext*, GtkBorder*)>(get_border)(context, &border);
  } else {
    DlCast<void(GtkStyleContext*, GtkStateFlags, GtkBorder*)>(get_border)(
        context, gtk_style_context_get_state(context), &border);
  }
  return InsetsFromGtkBorder(border);
}

DISABLE_CFI_ICALL
gfx::Insets GtkStyleContextGetMargin(GtkStyleContext* context) {
  static void* get_margin = DlSym(GetLibGtk(), "gtk_style_context_get_margin");
  GtkBorder margin;
  if (GtkCheckVersion(4)) {
    DlCast<void(GtkStyleContext*, GtkBorder*)>(get_margin)(context, &margin);
  } else {
    DlCast<void(GtkStyleContext*, GtkStateFlags, GtkBorder*)>(get_margin)(
        context, gtk_style_context_get_state(context), &margin);
  }
  return InsetsFromGtkBorder(margin);
}

DISABLE_CFI_ICALL
bool GtkImContextFilterKeypress(GtkIMContext* context, GdkEventKey* event) {
  static void* filter = DlSym(GetLibGtk(), "gtk_im_context_filter_keypress");
  if (GtkCheckVersion(4)) {
    return DlCast<bool(GtkIMContext*, GdkEvent*)>(filter)(
        context, reinterpret_cast<GdkEvent*>(event));
  }
  return DlCast<bool(GtkIMContext*, GdkEventKey*)>(filter)(context, event);
}

DISABLE_CFI_ICALL
bool GtkFileChooserSetCurrentFolder(GtkFileChooser* dialog,
                                    const base::FilePath& path) {
  static void* set = DlSym(GetLibGtk(), "gtk_file_chooser_set_current_folder");
  if (GtkCheckVersion(4)) {
    auto file = TakeGObject(g_file_new_for_path(path.value().c_str()));
    return DlCast<bool(GtkFileChooser*, GFile*, GError**)>(set)(dialog, file,
                                                                nullptr);
  }
  return DlCast<bool(GtkFileChooser*, const gchar*)>(set)(dialog,
                                                          path.value().c_str());
}

DISABLE_CFI_ICALL
void GtkRenderIcon(GtkStyleContext* context,
                   cairo_t* cr,
                   GdkPixbuf* pixbuf,
                   GdkTexture* texture,
                   double x,
                   double y) {
  static void* render = DlSym(GetLibGtk(), "gtk_render_icon");
  if (GtkCheckVersion(4)) {
    DCHECK(texture);
    DlCast<void(GtkStyleContext*, cairo_t*, GdkTexture*, double, double)>(
        render)(context, cr, texture, x, y);
  } else {
    DCHECK(pixbuf);
    DlCast<void(GtkStyleContext*, cairo_t*, GdkPixbuf*, double, double)>(
        render)(context, cr, pixbuf, x, y);
  }
}

ScopedGObject<GListModel> Gtk4FileChooserGetFiles(GtkFileChooser* dialog) {
  DCHECK(GtkCheckVersion(4));
  static void* get = DlSym(GetLibGtk(), "gtk_file_chooser_get_files");
  return TakeGObject(DlCast<GListModel*(GtkFileChooser*)>(get)(dialog));
}

void GtkStyleContextGet(GtkStyleContext* context, ...) {
  va_list args;
  va_start(args, context);
  gtk_style_context_get_valist(context, gtk_style_context_get_state(context),
                               args);
  va_end(args);
}

void GtkStyleContextGetStyle(GtkStyleContext* context, ...) {
  va_list args;
  va_start(args, context);
  gtk_style_context_get_style_valist(context, args);
  va_end(args);
}

DISABLE_CFI_ICALL
ScopedGObject<GtkIconInfo> Gtk3IconThemeLookupByGicon(
    GtkIconTheme* theme,
    GIcon* icon,
    int size,
    GtkIconLookupFlags flags) {
  DCHECK(!GtkCheckVersion(4));
  static void* lookup = DlSym(GetLibGtk(), "gtk_icon_theme_lookup_by_gicon");
  return TakeGObject(
      DlCast<GtkIconInfo*(GtkIconTheme*, GIcon*, int, GtkIconLookupFlags)>(
          lookup)(theme, icon, size, flags));
}

DISABLE_CFI_ICALL
ScopedGObject<GtkIconPaintable> Gtk4IconThemeLookupIcon(
    GtkIconTheme* theme,
    const char* icon_name,
    const char* fallbacks[],
    int size,
    int scale,
    GtkTextDirection direction,
    GtkIconLookupFlags flags) {
  static void* lookup = DlSym(GetLibGtk(), "gtk_icon_theme_lookup_icon");
  return TakeGObject(
      DlCast<GtkIconPaintable*(GtkIconTheme*, const char*, const char*[], int,
                               int, GtkTextDirection, GtkIconLookupFlags)>(
          lookup)(theme, icon_name, fallbacks, size, scale, direction, flags));
}

DISABLE_CFI_ICALL
ScopedGObject<GtkIconPaintable> Gtk4IconThemeLookupByGicon(
    GtkIconTheme* theme,
    GIcon* icon,
    int size,
    int scale,
    GtkTextDirection direction,
    GtkIconLookupFlags flags) {
  static void* lookup = DlSym(GetLibGtk(), "gtk_icon_theme_lookup_by_gicon");
  DCHECK(GtkCheckVersion(4));
  return TakeGObject(
      DlCast<GtkIconPaintable*(GtkIconTheme*, GIcon*, int, int,
                               GtkTextDirection, GtkIconLookupFlags)>(lookup)(
          theme, icon, size, scale, direction, flags));
}

}  // namespace gtk

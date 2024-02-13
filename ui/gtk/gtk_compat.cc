// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_compat.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/color_palette.h"
#include "ui/gtk/gtk_stubs.h"

namespace gtk {

// IMPORTANT: All functions in this file that call dlsym()'ed
// functions should be annotated with DISABLE_CFI_DLSYM.

namespace {

const char kGtkVersionFlag[] = "gtk-version";

struct Gdk3Rgba {
  gdouble r;
  gdouble g;
  gdouble b;
  gdouble a;
};

struct Gdk4Rgba {
  float r;
  float g;
  float b;
  float a;
};

template <typename T>
SkColor GdkRgbaToSkColor(const T& color) {
  return SkColorSetARGB(color.a * 255, color.r * 255, color.g * 255,
                        color.b * 255);
}

void* DlOpen(const char* library_name, bool check = true) {
  void* library = dlopen(library_name, RTLD_LAZY | RTLD_GLOBAL);
  CHECK(!check || library);
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

void* GetLibGtk3(bool check = true) {
  static void* libgtk3 = DlOpen("libgtk-3.so.0", check);
  return libgtk3;
}

void* GetLibGtk4(bool check = true) {
  static void* libgtk4 = DlOpen("libgtk-4.so.1", check);
  return libgtk4;
}

void* GetLibGtk() {
  if (GtkCheckVersion(4))
    return GetLibGtk4();
  return GetLibGtk3();
}

bool LoadGtk3() {
  if (!GetLibGtk3(false))
    return false;
  ui_gtk::InitializeGdk_pixbuf(GetLibGdkPixbuf());
  ui_gtk::InitializeGdk(GetLibGdk3());
  ui_gtk::InitializeGtk(GetLibGtk3());
  return true;
}

bool LoadGtk4() {
  if (!GetLibGtk4(false))
    return false;
  // In GTK4 mode, we require some newer gio symbols that aren't available
  // in Ubuntu Xenial or Debian Stretch.  Fortunately, GTK4 itself depends
  // on a newer version of glib (which provides gio), so if we're using
  // GTK4, we can safely assume the system has the required gio symbols.
  ui_gtk::InitializeGio(GetLibGio());
  // In GTK4, libgtk provides all gdk_*, gsk_*, and gtk_* symbols.
  ui_gtk::InitializeGdk(GetLibGtk4());
  ui_gtk::InitializeGsk(GetLibGtk4());
  ui_gtk::InitializeGtk(GetLibGtk4());
  return true;
}

bool LoadGtkImpl() {
  auto* cmd = base::CommandLine::ForCurrentProcess();
  unsigned int gtk_version;
  if (!base::StringToUint(cmd->GetSwitchValueASCII(kGtkVersionFlag),
                          &gtk_version)) {
    gtk_version = 0;
  }
  // Prefer GTK3 for now as the GTK4 ecosystem is still immature.
  return gtk_version == 4 ? LoadGtk4() || LoadGtk3() : LoadGtk3() || LoadGtk4();
}

gfx::Insets InsetsFromGtkBorder(const GtkBorder& border) {
  return gfx::Insets::TLBR(border.top, border.left, border.bottom,
                           border.right);
}

}  // namespace

bool LoadGtk() {
  static bool loaded = LoadGtkImpl();
  return loaded;
}

const base::Version& GtkVersion() {
  static base::NoDestructor<base::Version> gtk_version(
      std::vector<uint32_t>{gtk_get_major_version(), gtk_get_minor_version(),
                            gtk_get_micro_version()});
  return *gtk_version;
}

bool GtkCheckVersion(uint32_t major, uint32_t minor, uint32_t micro) {
  return GtkVersion() >= base::Version({major, minor, micro});
}

DISABLE_CFI_DLSYM
bool GtkInitCheck(int* argc, char** argv) {
  static void* gtk_init_check = DlSym(GetLibGtk(), "gtk_init_check");
  if (GtkCheckVersion(4))
    return DlCast<gboolean()>(gtk_init_check)();

  {
    // http://crbug.com/423873
    ANNOTATE_SCOPED_MEMORY_LEAK;
    return DlCast<gboolean(int*, char***)>(gtk_init_check)(argc, &argv);
  }
}

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
SkColor GtkStyleContextGetColor(GtkStyleContext* context) {
  static void* get_color = DlSym(GetLibGtk(), "gtk_style_context_get_color");
  if (GtkCheckVersion(4)) {
    Gdk4Rgba color;
    DlCast<void(GtkStyleContext*, Gdk4Rgba*)>(get_color)(context, &color);
    return GdkRgbaToSkColor(color);
  }
  Gdk3Rgba color;
  DlCast<void(GtkStyleContext*, GtkStateFlags, Gdk3Rgba*)>(get_color)(
      context, gtk_style_context_get_state(context), &color);
  return GdkRgbaToSkColor(color);
}

DISABLE_CFI_DLSYM
SkColor GtkStyleContextGetBackgroundColor(GtkStyleContext* context) {
  DCHECK(!GtkCheckVersion(4));
  static void* get_bg_color =
      DlSym(GetLibGtk(), "gtk_style_context_get_background_color");
  Gdk3Rgba color;
  DlCast<void(GtkStyleContext*, GtkStateFlags, Gdk3Rgba*)>(get_bg_color)(
      context, gtk_style_context_get_state(context), &color);
  return GdkRgbaToSkColor(color);
}

DISABLE_CFI_DLSYM
std::optional<SkColor> GtkStyleContextLookupColor(GtkStyleContext* context,
                                                  const gchar* color_name) {
  DCHECK(!GtkCheckVersion(4));
  static void* lookup_color =
      DlSym(GetLibGtk(), "gtk_style_context_lookup_color");
  Gdk3Rgba color;
  if (DlCast<gboolean(GtkStyleContext*, const gchar*, Gdk3Rgba*)>(lookup_color)(
          context, color_name, &color)) {
    return GdkRgbaToSkColor(color);
  }
  return std::nullopt;
}

DISABLE_CFI_DLSYM
bool GtkImContextFilterKeypress(GtkIMContext* context, GdkEventKey* event) {
  static void* filter = DlSym(GetLibGtk(), "gtk_im_context_filter_keypress");
  if (GtkCheckVersion(4)) {
    return DlCast<bool(GtkIMContext*, GdkEvent*)>(filter)(
        context, reinterpret_cast<GdkEvent*>(event));
  }
  return DlCast<bool(GtkIMContext*, GdkEventKey*)>(filter)(context, event);
}

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
void GtkRenderIcon(GtkStyleContext* context,
                   cairo_t* cr,
                   GdkPixbuf* pixbuf,
                   GdkTexture* texture,
                   double x,
                   double y) {
  static void* render = DlSym(GetLibGtk(), "gtk_render_icon");
  if (texture) {
    DCHECK(GtkCheckVersion(4));
    DlCast<void(GtkStyleContext*, cairo_t*, GdkTexture*, double, double)>(
        render)(context, cr, texture, x, y);
  } else if (pixbuf) {
    DCHECK(!GtkCheckVersion(4));
    DlCast<void(GtkStyleContext*, cairo_t*, GdkPixbuf*, double, double)>(
        render)(context, cr, pixbuf, x, y);
  }
}

DISABLE_CFI_DLSYM
GtkWidget* GtkToplevelWindowNew() {
  static void* window_new = DlSym(GetLibGtk(), "gtk_window_new");
  if (GtkCheckVersion(4))
    return DlCast<GtkWidget*()>(window_new)();
  return DlCast<GtkWidget*(GtkWindowType)>(window_new)(GTK_WINDOW_TOPLEVEL);
}

DISABLE_CFI_DLSYM
void GtkCssProviderLoadFromData(GtkCssProvider* css_provider,
                                const char* data,
                                gssize length) {
  static void* load = DlSym(GetLibGtk(), "gtk_css_provider_load_from_data");
  if (GtkCheckVersion(4)) {
    DlCast<void(GtkCssProvider*, const char*, gssize)>(load)(css_provider, data,
                                                             length);
  } else {
    DlCast<gboolean(GtkCssProvider*, const char*, gssize, GError**)>(load)(
        css_provider, data, length, nullptr);
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

DISABLE_CFI_DLSYM
ScopedGObject<GtkIconInfo> Gtk3IconThemeLookupByGiconForScale(
    GtkIconTheme* theme,
    GIcon* icon,
    int size,
    int scale,
    GtkIconLookupFlags flags) {
  DCHECK(!GtkCheckVersion(4));
  static void* lookup =
      DlSym(GetLibGtk(), "gtk_icon_theme_lookup_by_gicon_for_scale");
  return TakeGObject(
      DlCast<GtkIconInfo*(GtkIconTheme*, GIcon*, int, int, GtkIconLookupFlags)>(
          lookup)(theme, icon, size, scale, flags));
}

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
GtkWidget* GtkFileChooserDialogNew(const gchar* title,
                                   GtkWindow* parent,
                                   GtkFileChooserAction action,
                                   const gchar* first_button_text,
                                   GtkResponseType first_response,
                                   const gchar* second_button_text,
                                   GtkResponseType second_response) {
  static void* create = DlSym(GetLibGtk(), "gtk_file_chooser_dialog_new");
  return DlCast<GtkWidget*(const gchar*, GtkWindow*, GtkFileChooserAction,
                           const gchar*, ...)>(create)(
      title, parent, action, first_button_text, first_response,
      second_button_text, second_response, nullptr);
}

DISABLE_CFI_DLSYM
GtkTreeStore* GtkTreeStoreNew(GType type) {
  static void* create = DlSym(GetLibGtk(), "gtk_tree_store_new");
  return DlCast<GtkTreeStore*(gint, ...)>(create)(1, type);
}

DISABLE_CFI_DLSYM
GdkEventType GdkEventGetEventType(GdkEvent* event) {
  static void* get = DlSym(GetLibGtk(), "gdk_event_get_event_type");
  return DlCast<GdkEventType(GdkEvent*)>(get)(event);
}

DISABLE_CFI_DLSYM
guint32 GdkEventGetTime(GdkEvent* event) {
  static void* get = DlSym(GetLibGtk(), "gdk_event_get_time");
  return DlCast<guint32(GdkEvent*)>(get)(event);
}

GdkEventType GdkKeyPress() {
  return static_cast<GdkEventType>(GtkCheckVersion(4) ? 4 : 8);
}

GdkEventType GdkKeyRelease() {
  return static_cast<GdkEventType>(GtkCheckVersion(4) ? 5 : 9);
}

}  // namespace gtk

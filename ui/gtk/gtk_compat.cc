// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_compat.h"

#include <dlfcn.h>

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/color_palette.h"
#include "ui/gtk/gtk_stubs.h"

namespace gtk {

namespace {

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

template <typename T>
struct DlSymWrapper {
  // An exclusion is necessary because not using a raw_ptr results in an error:
  // "Use raw_ptr<T> instead of a raw pointer.", but using raw_ptr results in a
  // different error: "raw_ptr<T> doesn't work with this kind of pointee type
  // T".
  RAW_PTR_EXCLUSION T* fn = nullptr;

  template <typename... Args>
  DISABLE_CFI_DLSYM auto operator()(Args&&... args) {
    CHECK(fn);
    return fn(std::forward<Args>(args)...);
  }
};

template <typename T>
auto DlSym(void* library, const char* name) {
  void* symbol = dlsym(library, name);
  return DlSymWrapper<T>{reinterpret_cast<T*>(symbol)};
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
  if (GtkCheckVersion(4)) {
    return GetLibGtk4();
  }
  return GetLibGtk3();
}

bool LoadGtk3() {
  if (!GetLibGtk3(false)) {
    return false;
  }
  ui_gtk::InitializeGdk_pixbuf(GetLibGdkPixbuf());
  ui_gtk::InitializeGdk(GetLibGdk3());
  ui_gtk::InitializeGtk(GetLibGtk3());
  return true;
}

bool LoadGtk4() {
  if (!GetLibGtk4(false)) {
    return false;
  }
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

bool LoadGtkImpl(ui::LinuxUiBackend backend) {
  // If GTK3 or GTK4 is somehow already loaded, then the preloaded library must
  // be used, because GTK3 and GTK4 have conflicting symbols and cannot be
  // loaded simultaneously.
  if (dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_GLOBAL | RTLD_NOLOAD)) {
    return LoadGtk3();
  }
  if (dlopen("libgtk-4.so.1", RTLD_LAZY | RTLD_GLOBAL | RTLD_NOLOAD)) {
    return LoadGtk4();
  }

  auto* cmd = base::CommandLine::ForCurrentProcess();
  unsigned int gtk_version;
  if (!base::StringToUint(cmd->GetSwitchValueASCII(switches::kGtkVersionFlag),
                          &gtk_version)) {
    gtk_version = 0;
  }
  auto env = base::Environment::Create();
  const auto desktop = base::nix::GetDesktopEnvironment(env.get());
  if (!gtk_version && desktop == base::nix::DESKTOP_ENVIRONMENT_GNOME) {
    // GNOME is currently the only desktop to support GTK4 starting with version
    // 42+. Try to match the loaded GTK version with the GNOME GTK version.
    // Checking the GNOME version is not necessary since GTK4 is available iff
    // GNOME is version 42+. This is the case for Debian, Ubuntu, and the
    // RPM-based distributions that are supported.
    gtk_version = 4;
  }
  // Default to GTK4 on GNOME except on X11 where GTK IMEs are still immature.
  // This may be enabled unconditionally when support for Ubuntu 22.04 ends,
  // since IME issues have been addressed in later releases. Allow the command
  // line switch to override this.
  return gtk_version == 4 && (cmd->HasSwitch(switches::kGtkVersionFlag) ||
                              backend == ui::LinuxUiBackend::kWayland)
             ? LoadGtk4() || LoadGtk3()
             : LoadGtk3() || LoadGtk4();
}

gfx::Insets InsetsFromGtkBorder(const GtkBorder& border) {
  return gfx::Insets::TLBR(border.top, border.left, border.bottom,
                           border.right);
}

}  // namespace

bool LoadGtk(ui::LinuxUiBackend backend) {
  static bool loaded = LoadGtkImpl(backend);
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

bool GtkInitCheck(int* argc, char** argv) {
  if (GtkCheckVersion(4)) {
    static auto gtk_init_check =
        DlSym<gboolean()>(GetLibGtk(), "gtk_init_check");
    return gtk_init_check();
  }

  {
    // http://crbug.com/423873
    ANNOTATE_SCOPED_MEMORY_LEAK;
    static auto gtk_init_check =
        DlSym<gboolean(int*, char***)>(GetLibGtk(), "gtk_init_check");
    return gtk_init_check(argc, &argv);
  }
}

gfx::Insets GtkStyleContextGetPadding(GtkStyleContext* context) {
  GtkBorder padding;
  if (GtkCheckVersion(4)) {
    static auto get_padding = DlSym<void(GtkStyleContext*, GtkBorder*)>(
        GetLibGtk(), "gtk_style_context_get_padding");
    get_padding(context, &padding);
  } else {
    static auto get_padding =
        DlSym<void(GtkStyleContext*, GtkStateFlags, GtkBorder*)>(
            GetLibGtk(), "gtk_style_context_get_padding");
    get_padding(context, gtk_style_context_get_state(context), &padding);
  }
  return InsetsFromGtkBorder(padding);
}

gfx::Insets GtkStyleContextGetBorder(GtkStyleContext* context) {
  GtkBorder border;
  if (GtkCheckVersion(4)) {
    static auto get_border = DlSym<void(GtkStyleContext*, GtkBorder*)>(
        GetLibGtk(), "gtk_style_context_get_border");
    get_border(context, &border);
  } else {
    static auto get_border =
        DlSym<void(GtkStyleContext*, GtkStateFlags, GtkBorder*)>(
            GetLibGtk(), "gtk_style_context_get_border");
    get_border(context, gtk_style_context_get_state(context), &border);
  }
  return InsetsFromGtkBorder(border);
}

gfx::Insets GtkStyleContextGetMargin(GtkStyleContext* context) {
  GtkBorder margin;
  if (GtkCheckVersion(4)) {
    static auto get_margin = DlSym<void(GtkStyleContext*, GtkBorder*)>(
        GetLibGtk(), "gtk_style_context_get_margin");
    get_margin(context, &margin);
  } else {
    static auto get_margin =
        DlSym<void(GtkStyleContext*, GtkStateFlags, GtkBorder*)>(
            GetLibGtk(), "gtk_style_context_get_margin");
    get_margin(context, gtk_style_context_get_state(context), &margin);
  }
  return InsetsFromGtkBorder(margin);
}

SkColor GtkStyleContextGetColor(GtkStyleContext* context) {
  if (GtkCheckVersion(4)) {
    static auto get_color = DlSym<void(GtkStyleContext*, Gdk4Rgba*)>(
        GetLibGtk(), "gtk_style_context_get_color");
    Gdk4Rgba color;
    get_color(context, &color);
    return GdkRgbaToSkColor(color);
  }

  static auto get_color =
      DlSym<void(GtkStyleContext*, GtkStateFlags, Gdk3Rgba*)>(
          GetLibGtk(), "gtk_style_context_get_color");
  Gdk3Rgba color;
  get_color(context, gtk_style_context_get_state(context), &color);
  return GdkRgbaToSkColor(color);
}

SkColor GtkStyleContextGetBackgroundColor(GtkStyleContext* context) {
  DCHECK(!GtkCheckVersion(4));
  static auto get_bg_color =
      DlSym<void(GtkStyleContext*, GtkStateFlags, Gdk3Rgba*)>(
          GetLibGtk(), "gtk_style_context_get_background_color");
  Gdk3Rgba color;
  get_bg_color(context, gtk_style_context_get_state(context), &color);
  return GdkRgbaToSkColor(color);
}

std::optional<SkColor> GtkStyleContextLookupColor(GtkStyleContext* context,
                                                  const gchar* color_name) {
  DCHECK(!GtkCheckVersion(4));
  static auto lookup_color =
      DlSym<gboolean(GtkStyleContext*, const gchar*, Gdk3Rgba*)>(
          GetLibGtk(), "gtk_style_context_lookup_color");
  Gdk3Rgba color;
  if (lookup_color(context, color_name, &color)) {
    return GdkRgbaToSkColor(color);
  }
  return std::nullopt;
}

bool GtkImContextFilterKeypress(GtkIMContext* context, GdkEventKey* event) {
  if (GtkCheckVersion(4)) {
    static auto filter = DlSym<bool(GtkIMContext*, GdkEvent*)>(
        GetLibGtk(), "gtk_im_context_filter_keypress");
    return filter(context, reinterpret_cast<GdkEvent*>(event));
  }
  static auto filter = DlSym<bool(GtkIMContext*, GdkEventKey*)>(
      GetLibGtk(), "gtk_im_context_filter_keypress");
  return filter(context, event);
}

bool GtkFileChooserSetCurrentFolder(GtkFileChooser* dialog,
                                    const base::FilePath& path) {
  if (GtkCheckVersion(4)) {
    static auto set = DlSym<bool(GtkFileChooser*, GFile*, GError**)>(
        GetLibGtk(), "gtk_file_chooser_set_current_folder");
    auto file = TakeGObject(g_file_new_for_path(path.value().c_str()));
    return set(dialog, file, nullptr);
  }

  static auto set = DlSym<bool(GtkFileChooser*, const gchar*)>(
      GetLibGtk(), "gtk_file_chooser_set_current_folder");
  return set(dialog, path.value().c_str());
}

void GtkRenderIcon(GtkStyleContext* context,
                   cairo_t* cr,
                   GdkPixbuf* pixbuf,
                   GdkTexture* texture,
                   double x,
                   double y) {
  if (texture) {
    DCHECK(GtkCheckVersion(4));
    static auto render =
        DlSym<void(GtkStyleContext*, cairo_t*, GdkTexture*, double, double)>(
            GetLibGtk(), "gtk_render_icon");
    render(context, cr, texture, x, y);
  } else if (pixbuf) {
    DCHECK(!GtkCheckVersion(4));
    static auto render =
        DlSym<void(GtkStyleContext*, cairo_t*, GdkPixbuf*, double, double)>(
            GetLibGtk(), "gtk_render_icon");
    render(context, cr, pixbuf, x, y);
  }
}

GtkWidget* GtkToplevelWindowNew() {
  if (GtkCheckVersion(4)) {
    static auto window_new = DlSym<GtkWidget*()>(GetLibGtk(), "gtk_window_new");
    return window_new();
  }
  static auto window_new =
      DlSym<GtkWidget*(GtkWindowType)>(GetLibGtk(), "gtk_window_new");
  return window_new(GTK_WINDOW_TOPLEVEL);
}

void GtkCssProviderLoadFromData(GtkCssProvider* css_provider,
                                const char* data,
                                gssize length) {
  if (GtkCheckVersion(4)) {
    static auto load = DlSym<void(GtkCssProvider*, const char*, gssize)>(
        GetLibGtk(), "gtk_css_provider_load_from_data");
    load(css_provider, data, length);
  } else {
    static auto load =
        DlSym<gboolean(GtkCssProvider*, const char*, gssize, GError**)>(
            GetLibGtk(), "gtk_css_provider_load_from_data");
    load(css_provider, data, length, nullptr);
  }
}

ScopedGObject<GListModel> Gtk4FileChooserGetFiles(GtkFileChooser* dialog) {
  DCHECK(GtkCheckVersion(4));
  static auto get = DlSym<GListModel*(GtkFileChooser*)>(
      GetLibGtk(), "gtk_file_chooser_get_files");
  return TakeGObject(get(dialog));
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

ScopedGObject<GtkIconInfo> Gtk3IconThemeLookupByGiconForScale(
    GtkIconTheme* theme,
    GIcon* icon,
    int size,
    int scale,
    GtkIconLookupFlags flags) {
  DCHECK(!GtkCheckVersion(4));
  static auto lookup =
      DlSym<GtkIconInfo*(GtkIconTheme*, GIcon*, int, int, GtkIconLookupFlags)>(
          GetLibGtk(), "gtk_icon_theme_lookup_by_gicon_for_scale");
  return TakeGObject(lookup(theme, icon, size, scale, flags));
}

ScopedGObject<GtkIconPaintable> Gtk4IconThemeLookupIcon(
    GtkIconTheme* theme,
    const char* icon_name,
    const char* fallbacks[],
    int size,
    int scale,
    GtkTextDirection direction,
    GtkIconLookupFlags flags) {
  static auto lookup =
      DlSym<GtkIconPaintable*(GtkIconTheme*, const char*, const char*[], int,
                              int, GtkTextDirection, GtkIconLookupFlags)>(
          GetLibGtk(), "gtk_icon_theme_lookup_icon");
  return TakeGObject(
      lookup(theme, icon_name, fallbacks, size, scale, direction, flags));
}

ScopedGObject<GtkIconPaintable> Gtk4IconThemeLookupByGicon(
    GtkIconTheme* theme,
    GIcon* icon,
    int size,
    int scale,
    GtkTextDirection direction,
    GtkIconLookupFlags flags) {
  DCHECK(GtkCheckVersion(4));
  static auto lookup = DlSym<GtkIconPaintable*(
      GtkIconTheme*, GIcon*, int, int, GtkTextDirection, GtkIconLookupFlags)>(
      GetLibGtk(), "gtk_icon_theme_lookup_by_gicon");
  return TakeGObject(lookup(theme, icon, size, scale, direction, flags));
}

GtkWidget* GtkFileChooserDialogNew(const gchar* title,
                                   GtkWindow* parent,
                                   GtkFileChooserAction action,
                                   const gchar* first_button_text,
                                   GtkResponseType first_response,
                                   const gchar* second_button_text,
                                   GtkResponseType second_response) {
  static auto create = DlSym<GtkWidget*(
      const gchar*, GtkWindow*, GtkFileChooserAction, const gchar*, ...)>(
      GetLibGtk(), "gtk_file_chooser_dialog_new");
  return create(title, parent, action, first_button_text, first_response,
                second_button_text, second_response, nullptr);
}

GtkTreeStore* GtkTreeStoreNew(GType type) {
  static auto create =
      DlSym<GtkTreeStore*(gint, ...)>(GetLibGtk(), "gtk_tree_store_new");
  return create(1, type);
}

GdkEventType GdkEventGetEventType(GdkEvent* event) {
  static auto get =
      DlSym<GdkEventType(GdkEvent*)>(GetLibGtk(), "gdk_event_get_event_type");
  return get(event);
}

guint32 GdkEventGetTime(GdkEvent* event) {
  static auto get =
      DlSym<guint32(GdkEvent*)>(GetLibGtk(), "gdk_event_get_time");
  return get(event);
}

GdkEventType GdkKeyPress() {
  return static_cast<GdkEventType>(GtkCheckVersion(4) ? 4 : 8);
}

GdkEventType GdkKeyRelease() {
  return static_cast<GdkEventType>(GtkCheckVersion(4) ? 5 : 9);
}

}  // namespace gtk

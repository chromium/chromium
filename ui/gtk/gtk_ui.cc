// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_ui.h"

#include <cairo.h>
#include <pango/pango.h>

#include <cmath>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"
#include "base/nix/xdg_util.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "chrome/browser/themes/theme_properties.h"  // nogncheck
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/linux/fake_input_method_context.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/display/display.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_manager.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_key_bindings_handler.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/gtk/gtk_ui_platform_stub.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/input_method_context_impl_gtk.h"
#include "ui/gtk/native_theme_gtk.h"
#include "ui/gtk/nav_button_provider_gtk.h"
#include "ui/gtk/printing/print_dialog_gtk.h"
#include "ui/gtk/printing/printing_gtk_util.h"
#include "ui/gtk/select_file_dialog_linux_gtk.h"
#include "ui/gtk/settings_provider_gtk.h"
#include "ui/gtk/window_frame_provider_gtk.h"
#include "ui/linux/cursor_theme_manager_observer.h"
#include "ui/linux/device_scale_factor_observer.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_button_order_observer.h"
#include "ui/native_theme/native_theme.h"
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/window/window_button_order_provider.h"

#if BUILDFLAG(OZONE_PLATFORM_WAYLAND)
#define USE_WAYLAND
#endif
#if BUILDFLAG(OZONE_PLATFORM_X11)
#define USE_X11
#endif

#if defined(USE_WAYLAND)
#include "ui/gtk/wayland/gtk_ui_platform_wayland.h"
#endif

#if defined(USE_X11)
#include "ui/gtk/x/gtk_ui_platform_x11.h"
#endif

namespace gtk {

namespace {

// Stores the GtkUi singleton instance
const GtkUi* g_gtk_ui = nullptr;

const double kDefaultDPI = 96;

// Number of app indicators used (used as part of app-indicator id).
int indicators_count;

// The unknown content type.
const char kUnknownContentType[] = "application/octet-stream";

// Returns a gfx::FontRenderParams corresponding to GTK's configuration.
gfx::FontRenderParams GetGtkFontRenderParams() {
  GtkSettings* gtk_settings = gtk_settings_get_default();
  CHECK(gtk_settings);
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = nullptr;
  gchar* rgba = nullptr;
  g_object_get(gtk_settings, "gtk-xft-antialias", &antialias, "gtk-xft-hinting",
               &hinting, "gtk-xft-hintstyle", &hint_style, "gtk-xft-rgba",
               &rgba, nullptr);

  gfx::FontRenderParams params;
  params.antialiasing = antialias != 0;

  if (hinting == 0 || !hint_style || strcmp(hint_style, "hintnone") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  } else if (strcmp(hint_style, "hintslight") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_SLIGHT;
  } else if (strcmp(hint_style, "hintmedium") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_MEDIUM;
  } else if (strcmp(hint_style, "hintfull") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_FULL;
  } else {
    LOG(WARNING) << "Unexpected gtk-xft-hintstyle \"" << hint_style << "\"";
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  }

  if (!rgba || strcmp(rgba, "none") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
  } else if (strcmp(rgba, "rgb") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB;
  } else if (strcmp(rgba, "bgr") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR;
  } else if (strcmp(rgba, "vrgb") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB;
  } else if (strcmp(rgba, "vbgr") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR;
  } else {
    LOG(WARNING) << "Unexpected gtk-xft-rgba \"" << rgba << "\"";
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }

  g_free(hint_style);
  g_free(rgba);

  return params;
}

std::unique_ptr<GtkUiPlatform> CreateGtkUiPlatform(ui::LinuxUiBackend backend) {
  switch (backend) {
    case ui::LinuxUiBackend::kStub:
      return std::make_unique<GtkUiPlatformStub>();
#if defined(USE_X11)
    case ui::LinuxUiBackend::kX11:
      return std::make_unique<GtkUiPlatformX11>();
#endif
#if defined(USE_WAYLAND)
    case ui::LinuxUiBackend::kWayland:
      return std::make_unique<GtkUiPlatformWayland>();
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

GtkUi::GtkUi() : window_frame_actions_() {
  DCHECK(!g_gtk_ui);
  g_gtk_ui = this;
}

GtkUi::~GtkUi() {
  DCHECK_EQ(g_gtk_ui, this);
  g_gtk_ui = nullptr;
}

// static
GtkUiPlatform* GtkUi::GetPlatform() {
  DCHECK(g_gtk_ui) << "GtkUi instance is not set.";
  return g_gtk_ui->platform_.get();
}

bool GtkUi::Initialize() {
  if (!LoadGtk() || !GtkCheckVersion(3, 20)) {
    return false;
  }

  auto* delegate = ui::LinuxUiDelegate::GetInstance();
  DCHECK(delegate);
  platform_ = CreateGtkUiPlatform(delegate->GetBackend());

  // Avoid GTK initializing atk-bridge, and let AuraLinux implementation
  // do it once it is ready.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("NO_AT_BRIDGE", "1");
  // gtk_init_check() modifies argv, so make a copy first.
  CmdLineArgs cmd_line = CopyCmdLine(*base::CommandLine::ForCurrentProcess());
  if (!GtkInitFromCommandLine(&cmd_line.argc, cmd_line.argv.data())) {
    return false;
  }
  native_theme_ = NativeThemeGtk::instance();

  using Action = ui::LinuxUi::WindowFrameAction;
  using ActionSource = ui::LinuxUi::WindowFrameActionSource;
  window_frame_actions_ = {
      {ActionSource::kDoubleClick, Action::kToggleMaximize},
      {ActionSource::kMiddleClick, Action::kNone},
      {ActionSource::kRightClick, Action::kMenu}};

  GtkSettings* settings = gtk_settings_get_default();
  g_signal_connect_after(settings, "notify::gtk-theme-name",
                         G_CALLBACK(OnThemeChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-icon-theme-name",
                         G_CALLBACK(OnThemeChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-application-prefer-dark-theme",
                         G_CALLBACK(OnThemeChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-cursor-theme-name",
                         G_CALLBACK(OnCursorThemeNameChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-cursor-theme-size",
                         G_CALLBACK(OnCursorThemeSizeChangedThunk), this);

  // Listen for DPI changes.
  auto* dpi_callback = G_CALLBACK(OnDeviceScaleFactorMaybeChangedThunk);
  if (GtkCheckVersion(4)) {
    g_signal_connect_after(settings, "notify::gtk-xft-dpi", dpi_callback, this);
  } else {
    GdkScreen* screen = gdk_screen_get_default();
    g_signal_connect_after(screen, "notify::resolution", dpi_callback, this);
  }

  // Listen for scale factor changes.  We would prefer to listen on
  // a GdkScreen, but there is no scale-factor property, so use an
  // unmapped window instead.
  g_signal_connect(GetDummyWindow(), "notify::scale-factor",
                   G_CALLBACK(OnDeviceScaleFactorMaybeChangedThunk), this);

  LoadGtkValues();

  // We must build this after GTK gets initialized.
  settings_provider_ = std::make_unique<SettingsProviderGtk>(this);

  indicators_count = 0;

  platform_->OnInitialized(GetDummyWindow());

  return true;
}

ui::NativeTheme* GtkUi::GetNativeTheme() const {
  return native_theme_;
}

bool GtkUi::GetColor(int id, SkColor* color, bool use_custom_frame) const {
  for (const ColorMap& color_map :
       {colors_,
        use_custom_frame ? custom_frame_colors_ : native_frame_colors_}) {
    auto it = color_map.find(id);
    if (it != color_map.end()) {
      *color = it->second;
      return true;
    }
  }

  return false;
}

bool GtkUi::GetDisplayProperty(int id, int* result) const {
  if (id == ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR) {
    *result = 0;
    return true;
  }
  return false;
}

void GtkUi::GetFocusRingColor(SkColor* color) const {
  *color = focus_ring_color_;
}

void GtkUi::GetActiveSelectionBgColor(SkColor* color) const {
  *color = active_selection_bg_color_;
}

void GtkUi::GetActiveSelectionFgColor(SkColor* color) const {
  *color = active_selection_fg_color_;
}

void GtkUi::GetInactiveSelectionBgColor(SkColor* color) const {
  *color = inactive_selection_bg_color_;
}

void GtkUi::GetInactiveSelectionFgColor(SkColor* color) const {
  *color = inactive_selection_fg_color_;
}

base::TimeDelta GtkUi::GetCursorBlinkInterval() const {
  // From http://library.gnome.org/devel/gtk/unstable/GtkSettings.html, this is
  // the default value for gtk-cursor-blink-time.
  static const gint kGtkDefaultCursorBlinkTime = 1200;

  // Dividing GTK's cursor blink cycle time (in milliseconds) by this value
  // yields an appropriate value for
  // blink::RendererPreferences::caret_blink_interval.
  static const double kGtkCursorBlinkCycleFactor = 2000.0;

  gint cursor_blink_time = kGtkDefaultCursorBlinkTime;
  gboolean cursor_blink = TRUE;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-blink-time",
               &cursor_blink_time, "gtk-cursor-blink", &cursor_blink, nullptr);
  return cursor_blink
             ? base::Seconds(cursor_blink_time / kGtkCursorBlinkCycleFactor)
             : base::TimeDelta();
}

gfx::Image GtkUi::GetIconForContentType(const std::string& content_type,
                                        int dip_size,
                                        float scale) const {
  // This call doesn't take a reference.
  GtkIconTheme* theme = GetDefaultIconTheme();

  // GTK expects an integral scale. If `scale` is integral, pass it to GTK;
  // otherwise pretend the scale is 1 and manually recalculate `size`.
  int size;
  int scale_int;
  int scale_floor = base::ClampFloor(scale);
  int scale_ceil = base::ClampCeil(scale);
  if (scale_floor == scale_ceil) {
    scale_int = scale_floor;
    size = dip_size;
  } else {
    scale_int = 1;
    size = scale * dip_size;
  }

  std::string content_types[] = {content_type, kUnknownContentType};

  for (size_t i = 0; i < std::size(content_types); ++i) {
    auto icon = TakeGObject(g_content_type_get_icon(content_type.c_str()));
    SkBitmap bitmap;
    if (GtkCheckVersion(4)) {
      auto icon_paintable = Gtk4IconThemeLookupByGicon(
          theme, icon.get(), size, scale_int, GTK_TEXT_DIR_NONE,
          static_cast<GtkIconLookupFlags>(0));
      if (!icon_paintable) {
        continue;
      }

      auto* paintable = GlibCast<GdkPaintable>(icon_paintable.get(),
                                               gdk_paintable_get_type());
      auto* snapshot = gtk_snapshot_new();
      gdk_paintable_snapshot(paintable, snapshot, size, size);
      auto* node = gtk_snapshot_free_to_node(snapshot);
      GdkTexture* texture = GetTextureFromRenderNode(node);

      bitmap.allocN32Pixels(gdk_texture_get_width(texture),
                            gdk_texture_get_height(texture));
      gdk_texture_download(texture, static_cast<guchar*>(bitmap.getAddr(0, 0)),
                           bitmap.rowBytes());

      gsk_render_node_unref(node);
    } else {
      auto icon_info = Gtk3IconThemeLookupByGiconForScale(
          theme, icon.get(), size, scale_int,
          static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_FORCE_SIZE));
      if (!icon_info) {
        continue;
      }
      auto* surface =
          gtk_icon_info_load_surface(icon_info.get(), nullptr, nullptr);
      if (!surface) {
        continue;
      }
      DCHECK_EQ(cairo_surface_get_type(surface), CAIRO_SURFACE_TYPE_IMAGE);
      DCHECK_EQ(cairo_image_surface_get_format(surface), CAIRO_FORMAT_ARGB32);

      SkImageInfo image_info =
          SkImageInfo::Make(cairo_image_surface_get_width(surface),
                            cairo_image_surface_get_height(surface),
                            kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);
      if (!bitmap.installPixels(
              image_info, cairo_image_surface_get_data(surface),
              image_info.minRowBytes(),
              [](void*, void* surface) {
                cairo_surface_destroy(
                    reinterpret_cast<cairo_surface_t*>(surface));
              },
              surface)) {
        continue;
      }
    }
    gfx::ImageSkia image_skia =
        gfx::ImageSkia::CreateFromBitmap(bitmap, scale_int);
    image_skia.MakeThreadSafe();
    return gfx::Image(image_skia);
  }
  return gfx::Image();
}

void GtkUi::SetWindowButtonOrdering(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  views::WindowButtonOrderProvider::GetInstance()->SetWindowButtonOrder(
      leading_buttons, trailing_buttons);

  for (auto& observer : window_button_order_observer_list_) {
    observer.OnWindowButtonOrderingChange();
  }
}

void GtkUi::SetWindowFrameAction(WindowFrameActionSource source,
                                 WindowFrameAction action) {
  window_frame_actions_[source] = action;
}

std::unique_ptr<ui::LinuxInputMethodContext> GtkUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  return GetPlatform()->CreateInputMethodContext(delegate);
}

gfx::FontRenderParams GtkUi::GetDefaultFontRenderParams() const {
  static gfx::FontRenderParams params = GetGtkFontRenderParams();
  return params;
}

void GtkUi::GetDefaultFontDescription(std::string* family_out,
                                      int* size_pixels_out,
                                      int* style_out,
                                      int* weight_out,
                                      gfx::FontRenderParams* params_out) const {
  *family_out = default_font_family_;
  *size_pixels_out = default_font_size_pixels_;
  *style_out = default_font_style_;
  *weight_out = static_cast<int>(default_font_weight_);
  *params_out = default_font_render_params_;
}

ui::SelectFileDialog* GtkUi::CreateSelectFileDialog(
    void* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) const {
  return new SelectFileDialogLinuxGtk(
      static_cast<ui::SelectFileDialog::Listener*>(listener),
      std::move(policy));
}

ui::LinuxUi::WindowFrameAction GtkUi::GetWindowFrameAction(
    WindowFrameActionSource source) {
  return window_frame_actions_[source];
}

bool GtkUi::PreferDarkTheme() const {
  gboolean dark = false;
  g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
               &dark, nullptr);
  return dark;
}

bool GtkUi::AnimationsEnabled() const {
  gboolean animations_enabled = false;
  g_object_get(gtk_settings_get_default(), "gtk-enable-animations",
               &animations_enabled, nullptr);
  return animations_enabled;
}

void GtkUi::AddWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {
  window_button_order_observer_list_.AddObserver(observer);
}

void GtkUi::RemoveWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {
  window_button_order_observer_list_.RemoveObserver(observer);
}

std::unique_ptr<ui::NavButtonProvider> GtkUi::CreateNavButtonProvider() {
  return std::make_unique<gtk::NavButtonProviderGtk>();
}

ui::WindowFrameProvider* GtkUi::GetWindowFrameProvider(bool solid_frame) {
  auto& provider =
      solid_frame ? solid_frame_provider_ : transparent_frame_provider_;
  if (!provider) {
    provider = std::make_unique<gtk::WindowFrameProviderGtk>(solid_frame);
  }
  return provider.get();
}

// Mapping from GDK dead keys to corresponding printable character.
static struct {
  guint gdk_key;
  guint16 unicode;
} kDeadKeyMapping[] = {
    {GDK_KEY_dead_grave, 0x0060},      {GDK_KEY_dead_acute, 0x0027},
    {GDK_KEY_dead_circumflex, 0x005e}, {GDK_KEY_dead_tilde, 0x007e},
    {GDK_KEY_dead_diaeresis, 0x00a8},
};

base::flat_map<std::string, std::string> GtkUi::GetKeyboardLayoutMap() {
  GdkDisplay* display = gdk_display_get_default();
  GdkKeymap* keymap = nullptr;
  if (!GtkCheckVersion(4)) {
    keymap = gdk_keymap_get_for_display(display);
    if (!keymap) {
      return {};
    }
  }

  auto layouts = std::make_unique<ui::DomKeyboardLayoutManager>();
  auto map = base::flat_map<std::string, std::string>();

  for (unsigned int i_domcode = 0;
       i_domcode < ui::kWritingSystemKeyDomCodeEntries; ++i_domcode) {
    ui::DomCode domcode = ui::writing_system_key_domcodes[i_domcode];
    guint16 keycode = ui::KeycodeConverter::DomCodeToNativeKeycode(domcode);
    GdkKeymapKey* keys = nullptr;
    guint* keyvals = nullptr;
    gint n_entries = 0;

    // The order of the layouts is based on the system default ordering in
    // Keyboard Settings. The currently active layout does not affect this
    // order.
    const bool success =
        GtkCheckVersion(4) ? gdk_display_map_keycode(display, keycode, &keys,
                                                     &keyvals, &n_entries)
                           : gdk_keymap_get_entries_for_keycode(
                                 keymap, keycode, &keys, &keyvals, &n_entries);
    if (success) {
      for (gint i = 0; i < n_entries; ++i) {
        // There are 4 entries per layout group, one each for shift level 0..3.
        // We only care about the unshifted values (level = 0).
        if (keys[i].level == 0) {
          uint16_t unicode = gdk_keyval_to_unicode(keyvals[i]);
          if (unicode == 0) {
            for (const auto& i_dead : kDeadKeyMapping) {
              if (keyvals[i] == i_dead.gdk_key) {
                unicode = i_dead.unicode;
              }
            }
          }
          if (unicode != 0) {
            layouts->GetLayout(keys[i].group)->AddKeyMapping(domcode, unicode);
          }
        }
      }
    }
    g_free(keys);
    g_free(keyvals);
  }
  return layouts->GetFirstAsciiCapableLayout()->GetMap();
}

std::string GtkUi::GetCursorThemeName() {
  gchar* theme = nullptr;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-theme-name", &theme,
               nullptr);
  std::string theme_string;
  if (theme) {
    theme_string = theme;
    g_free(theme);
  }
  return theme_string;
}

int GtkUi::GetCursorThemeSize() {
  gint size = 0;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-theme-size", &size,
               nullptr);
  return size;
}

bool GtkUi::GetTextEditCommandsForEvent(
    const ui::Event& event,
    std::vector<ui::TextEditCommandAuraLinux>* commands) {
  // GTK4 dropped custom key bindings.
  if (GtkCheckVersion(4)) {
    return false;
  }

  // TODO(crbug.com/963419): Use delegate's |GetGdkKeymap| here to
  // determine if GtkUi's key binding handling implementation is used or not.
  // Ozone/Wayland was unintentionally using GtkUi for keybinding handling, so
  // early out here, for now, until a proper solution for ozone is implemented.
  if (!platform_->GetGdkKeymap()) {
    return false;
  }

  // Ensure that we have a keyboard handler.
  if (!key_bindings_handler_) {
    key_bindings_handler_ = std::make_unique<GtkKeyBindingsHandler>();
  }

  return key_bindings_handler_->MatchEvent(event, commands);
}

#if BUILDFLAG(ENABLE_PRINTING)
printing::PrintDialogLinuxInterface* GtkUi::CreatePrintDialog(
    printing::PrintingContextLinux* context) {
  return PrintDialogGtk::CreatePrintDialog(context);
}

gfx::Size GtkUi::GetPdfPaperSize(printing::PrintingContextLinux* context) {
  return GetPdfPaperSizeDeviceUnitsGtk(context);
}
#endif

void GtkUi::OnThemeChanged(GtkSettings* settings, GtkParamSpec* param) {
  colors_.clear();
  custom_frame_colors_.clear();
  native_frame_colors_.clear();
  native_theme_->OnThemeChanged(settings, param);
  LoadGtkValues();
  native_theme_->NotifyOnNativeThemeUpdated();
}

void GtkUi::OnCursorThemeNameChanged(GtkSettings* settings,
                                     GtkParamSpec* param) {
  std::string cursor_theme_name = GetCursorThemeName();
  if (cursor_theme_name.empty()) {
    return;
  }
  for (auto& observer : cursor_theme_observers()) {
    observer.OnCursorThemeNameChanged(cursor_theme_name);
  }
}

void GtkUi::OnCursorThemeSizeChanged(GtkSettings* settings,
                                     GtkParamSpec* param) {
  int cursor_theme_size = GetCursorThemeSize();
  if (!cursor_theme_size) {
    return;
  }
  for (auto& observer : cursor_theme_observers()) {
    observer.OnCursorThemeSizeChanged(cursor_theme_size);
  }
}

void GtkUi::OnDeviceScaleFactorMaybeChanged(void*, GParamSpec*) {
  UpdateDeviceScaleFactor();
}

void GtkUi::LoadGtkValues() {
  // TODO(thomasanderson): GtkThemeService had a comment here about having to
  // muck with the raw Prefs object to remove prefs::kCurrentThemeImages or else
  // we'd regress startup time. Figure out how to do that when we can't access
  // the prefs system from here.
  UpdateDeviceScaleFactor();
  UpdateColors();
}

void GtkUi::UpdateColors() {
  // TODO(tluk): The below code sets various ThemeProvider colors for GTK. Some
  // of these definitions leverage colors that were previously defined by
  // NativeThemeGtk and are now defined as GTK ColorMixers. These ThemeProvider
  // color definitions should be added as recipes to a browser ColorMixer once
  // the Color Pipeline project begins rollout into c/b/ui. In the meantime
  // use the ColorProvider instance from the ColorProviderManager corresponding
  // to the theme bits associated with the NativeThemeGtk instance to ensure
  // we do not regress existing behavior during the transition.
  const auto color_scheme = native_theme_->GetDefaultSystemColorScheme();
  const auto* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(
          {(color_scheme == ui::NativeTheme::ColorScheme::kDark)
               ? ui::ColorProviderManager::ColorMode::kDark
               : ui::ColorProviderManager::ColorMode::kLight,
           (color_scheme == ui::NativeTheme::ColorScheme::kPlatformHighContrast)
               ? ui::ColorProviderManager::ContrastMode::kHigh
               : ui::ColorProviderManager::ContrastMode::kNormal,
           ui::SystemTheme::kGtk,
           // Some theme colors, e.g. COLOR_NTP_LINK, are derived from color
           // provider colors. We assume that those sources' colors won't change
           // with frame type.
           ui::ColorProviderManager::FrameType::kChromium});

  SkColor location_bar_border = GetBorderColor("GtkEntry#entry");
  if (SkColorGetA(location_bar_border)) {
    colors_[ThemeProperties::COLOR_LOCATION_BAR_BORDER] = location_bar_border;
  }

  inactive_selection_bg_color_ = GetSelectionBgColor(
      "GtkTextView#textview.view:backdrop "
      "#text:backdrop #selection:backdrop");
  inactive_selection_fg_color_ = GetFgColor(
      "GtkTextView#textview.view:backdrop "
      "#text:backdrop #selection:backdrop");

  SkColor tab_border = GetBorderColor("GtkButton#button");
  // Separates the toolbar from the bookmark bar or butter bars.
  colors_[ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR] = tab_border;
  // Separates entries in the downloads bar.
  colors_[ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR] = tab_border;

  colors_[ThemeProperties::COLOR_NTP_BACKGROUND] =
      color_provider->GetColor(ui::kColorTextfieldBackground);
  colors_[ThemeProperties::COLOR_NTP_TEXT] =
      color_provider->GetColor(ui::kColorTextfieldForeground);
  colors_[ThemeProperties::COLOR_NTP_HEADER] =
      GetBorderColor("GtkButton#button");

  SkColor tab_text_color = GetFgColor("GtkLabel#label");
  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON] = tab_text_color;
  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_HOVERED] = tab_text_color;
  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_PRESSED] = tab_text_color;
  colors_[ThemeProperties::COLOR_TOOLBAR_TEXT] = tab_text_color;

  colors_[ThemeProperties::COLOR_NTP_LINK] =
      color_provider->GetColor(ui::kColorTextfieldSelectionBackground);

  // Generate the colors that we pass to Blink.
  focus_ring_color_ =
      color_provider->GetColor(ui::kColorFocusableBorderFocused);

  // Some GTK themes only define the text selection colors on the GtkEntry
  // class, so we need to use that for getting selection colors.
  active_selection_bg_color_ =
      color_provider->GetColor(ui::kColorTextfieldSelectionBackground);
  active_selection_fg_color_ =
      color_provider->GetColor(ui::kColorTextfieldSelectionForeground);

  // Generate colors that depend on whether or not a custom window frame is
  // used.  These colors belong in |color_map| below, not |colors_|.
  for (bool custom_frame : {false, true}) {
    ColorMap& color_map =
        custom_frame ? custom_frame_colors_ : native_frame_colors_;
    const std::string header_selector =
        custom_frame ? "#headerbar.header-bar.titlebar" : "GtkMenuBar#menubar";
    const std::string header_selector_inactive = header_selector + ":backdrop";
    const SkColor frame_color =
        SkColorSetA(GetBgColor(header_selector), SK_AlphaOPAQUE);
    const SkColor frame_color_inactive =
        SkColorSetA(GetBgColor(header_selector_inactive), SK_AlphaOPAQUE);

    color_map[ThemeProperties::COLOR_FRAME_ACTIVE] = frame_color;
    color_map[ThemeProperties::COLOR_FRAME_INACTIVE] = frame_color_inactive;
    color_map[ThemeProperties::COLOR_FRAME_ACTIVE_INCOGNITO] = frame_color;
    color_map[ThemeProperties::COLOR_FRAME_INACTIVE_INCOGNITO] =
        frame_color_inactive;

    // Compose the window color on the frame color to ensure the resulting tab
    // color is opaque.
    SkColor tab_color =
        color_utils::GetResultingPaintColor(GetBgColor(""), frame_color);

    color_map[ThemeProperties::COLOR_TOOLBAR] = tab_color;
    color_map[ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE] =
        tab_color;
    color_map[ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE] =
        tab_color;

    const SkColor background_tab_text_color =
        GetFgColor(header_selector + " GtkLabel#label.title");
    const SkColor background_tab_text_color_inactive =
        GetFgColor(header_selector_inactive + " GtkLabel#label.title");

    color_map[ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE] =
        background_tab_text_color;
    color_map[ThemeProperties::
                  COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO] =
        background_tab_text_color;
    color_map[ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE] =
        background_tab_text_color_inactive;
    color_map[ThemeProperties::
                  COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO] =
        background_tab_text_color_inactive;

    // These colors represent the border drawn around tabs and between
    // the tabstrip and toolbar.
    SkColor toolbar_top_separator = GetBorderColor(
        header_selector + " GtkSeparator#separator.vertical.titlebutton");
    SkColor toolbar_top_separator_inactive =
        GetBorderColor(header_selector +
                       ":backdrop GtkSeparator#separator.vertical.titlebutton");

    auto toolbar_top_separator_has_good_contrast = [&]() {
      // This constant is copied from chrome/browser/themes/theme_service.cc.
      const float kMinContrastRatio = 2.f;

      SkColor active = color_utils::GetResultingPaintColor(
          toolbar_top_separator, frame_color);
      SkColor inactive = color_utils::GetResultingPaintColor(
          toolbar_top_separator_inactive, frame_color_inactive);
      return color_utils::GetContrastRatio(frame_color, active) >=
                 kMinContrastRatio &&
             color_utils::GetContrastRatio(frame_color_inactive, inactive) >=
                 kMinContrastRatio;
    };

    if (!toolbar_top_separator_has_good_contrast()) {
      toolbar_top_separator =
          GetBorderColor(header_selector + " GtkButton#button");
      toolbar_top_separator_inactive =
          GetBorderColor(header_selector + ":backdrop GtkButton#button");
    }

    // If we can't get a contrasting stroke from the theme, have ThemeService
    // provide a stroke color for us.
    if (toolbar_top_separator_has_good_contrast()) {
      color_map[ThemeProperties::COLOR_TAB_STROKE_FRAME_ACTIVE] =
          toolbar_top_separator;
      color_map[ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE] =
          toolbar_top_separator_inactive;
      color_map[ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE] =
          toolbar_top_separator;
      color_map[ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE] =
          toolbar_top_separator_inactive;
    }
  }
}

void GtkUi::UpdateDefaultFont() {
  gfx::SetFontRenderParamsDeviceScaleFactor(device_scale_factor_);

  auto fake_label = TakeGObject(gtk_label_new(nullptr));
  PangoContext* pc = gtk_widget_get_pango_context(fake_label);
  const PangoFontDescription* desc = pango_context_get_font_description(pc);

  // Use gfx::FontRenderParams to select a family and determine the rendering
  // settings.
  gfx::FontRenderParamsQuery query;
  query.families =
      base::SplitString(pango_font_description_get_family(desc), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (pango_font_description_get_size_is_absolute(desc)) {
    // If the size is absolute, it's specified in Pango units. There are
    // PANGO_SCALE Pango units in a device unit (pixel).
    const int size_pixels = pango_font_description_get_size(desc) / PANGO_SCALE;
    default_font_size_pixels_ = size_pixels;
    query.pixel_size = size_pixels;
  } else {
    // Non-absolute sizes are in points (again scaled by PANGO_SIZE).
    // Round the value when converting to pixels to match GTK's logic.
    const double size_points = pango_font_description_get_size(desc) /
                               static_cast<double>(PANGO_SCALE);
    default_font_size_pixels_ =
        static_cast<int>(kDefaultDPI / 72.0 * size_points + 0.5);
    query.point_size = static_cast<int>(size_points);
  }

  query.style = gfx::Font::NORMAL;
  query.weight =
      static_cast<gfx::Font::Weight>(pango_font_description_get_weight(desc));
  // TODO(davemoore): What about PANGO_STYLE_OBLIQUE?
  if (pango_font_description_get_style(desc) == PANGO_STYLE_ITALIC) {
    query.style |= gfx::Font::ITALIC;
  }

  default_font_render_params_ =
      gfx::GetFontRenderParams(query, &default_font_family_);
  default_font_style_ = query.style;
}

float GtkUi::GetRawDeviceScaleFactor() {
  if (display::Display::HasForceDeviceScaleFactor()) {
    return display::Display::GetForcedDeviceScaleFactor();
  }

  float scale = gtk_widget_get_scale_factor(GetDummyWindow());
  DCHECK_GT(scale, 0.0);

  double resolution = 0;
  if (GtkCheckVersion(4)) {
    auto* settings = gtk_settings_get_default();
    int dpi = 0;
    g_object_get(settings, "gtk-xft-dpi", &dpi, nullptr);
    resolution = dpi / 1024.0;
  } else {
    GdkScreen* screen = gdk_screen_get_default();
    resolution = gdk_screen_get_resolution(screen);
  }
  if (resolution > 0) {
    scale *= resolution / kDefaultDPI;
  }

  // Round to the nearest 64th so that UI can losslessly multiply and divide
  // the scale factor.
  scale = roundf(scale * 64) / 64;

  return scale;
}

void GtkUi::UpdateDeviceScaleFactor() {
  float old_device_scale_factor = device_scale_factor_;
  device_scale_factor_ = GetRawDeviceScaleFactor();
  if (device_scale_factor_ != old_device_scale_factor) {
    for (ui::DeviceScaleFactorObserver& observer :
         device_scale_factor_observer_list()) {
      observer.OnDeviceScaleFactorChanged();
    }
  }
  UpdateDefaultFont();
}

float GtkUi::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

}  // namespace gtk

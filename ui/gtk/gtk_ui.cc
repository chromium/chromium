// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_ui.h"

#include <cairo.h>
#include <glib.h>
#include <pango/pango.h>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"
#include "base/nix/xdg_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/themes/theme_properties.h"  // nogncheck
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/linux/fake_input_method_context.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/display/display.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_manager.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/linux/fontconfig_util.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gtk/gtk_color_mixers.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_key_bindings_handler.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/gtk/gtk_ui_platform_stub.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/input_method_context_impl_gtk.h"
#include "ui/gtk/native_theme_gtk.h"
#include "ui/gtk/nav_button_provider_gtk.h"
#include "ui/gtk/os_settings_provider_gtk.h"
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
#include "ui/linux/primary_paste_pref_observer.h"
#include "ui/linux/window_button_order_observer.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/window/window_button_order_provider.h"

#if BUILDFLAG(IS_OZONE_WAYLAND)
#include "ui/gtk/wayland/gtk_ui_platform_wayland.h"
#endif  // BUILDFLAG(IS_OZONE_WAYLAND)

#if BUILDFLAG(IS_OZONE_X11)
#include "ui/gtk/x/gtk_ui_platform_x11.h"
#endif  // BUILDFLAG(IS_OZONE_X11)

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

  if (hinting == 0 || !hint_style ||
      UNSAFE_TODO(strcmp(hint_style, "hintnone")) == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  } else if (UNSAFE_TODO(strcmp(hint_style, "hintslight")) == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_SLIGHT;
  } else if (UNSAFE_TODO(strcmp(hint_style, "hintmedium")) == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_MEDIUM;
  } else if (UNSAFE_TODO(strcmp(hint_style, "hintfull")) == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_FULL;
  } else {
    LOG(WARNING) << "Unexpected gtk-xft-hintstyle \"" << hint_style << "\"";
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  }

  if (!rgba || UNSAFE_TODO(strcmp(rgba, "none")) == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
  } else if (UNSAFE_TODO(strcmp(rgba, "rgb")) == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB;
  } else if (UNSAFE_TODO(strcmp(rgba, "bgr")) == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR;
  } else if (UNSAFE_TODO(strcmp(rgba, "vrgb")) == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB;
  } else if (UNSAFE_TODO(strcmp(rgba, "vbgr")) == 0) {
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
#if BUILDFLAG(IS_OZONE_X11)
    case ui::LinuxUiBackend::kX11:
      return std::make_unique<GtkUiPlatformX11>();
#endif  // BUILDFLAG(IS_OZONE_X11)
#if BUILDFLAG(IS_OZONE_WAYLAND)
    case ui::LinuxUiBackend::kWayland:
      return std::make_unique<GtkUiPlatformWayland>();
#endif  // BUILDFLAG(IS_OZONE_WAYLAND)
    default:
      NOTREACHED();
  }
}

int GetXftDpi() {
  int dpi = -1;
  g_object_get(gtk_settings_get_default(), "gtk-xft-dpi", &dpi, nullptr);
  return dpi < 0 ? 0 : dpi;
}

double FontScale() {
  double resolution = 0;
  if (const int dpi = GetXftDpi()) {
    resolution = dpi / 1024.0;
  } else {
    GdkScreen* screen = gdk_screen_get_default();
    resolution = gdk_screen_get_resolution(screen);
  }
  const double font_scale = resolution > 0 ? resolution / kDefaultDPI : 1.0;
  // Round to the nearest 1/64th so that UI can losslessly multiply and divide
  // the scale factor.
  return std::round(font_scale * 64) / 64;
}

// Some misconfigured systems have missing or corrupted schemas, see
// https://crbug.com/434763642. Avoid initializing GTK in this case to prevent
// a crash.
bool IsValidSchema(ui::LinuxUiBackend backend) {
  struct GFreeDeleter {
    void operator()(gchar* ptr) const { g_free(ptr); }
  };
  struct GVariantDeleter {
    void operator()(GVariant* ptr) const { g_variant_unref(ptr); }
  };
  struct GSettingsSchemaKeyDeleter {
    void operator()(GSettingsSchemaKey* ptr) const {
      g_settings_schema_key_unref(ptr);
    }
  };
  struct GSettingsSchemaDeleter {
    void operator()(GSettingsSchema* ptr) const {
      g_settings_schema_unref(ptr);
    }
  };

  struct {
    const char* interface;
    std::array<const char*, 3> keys;
  } static constexpr kInterfaces[] = {
      {"org.gnome.desktop.interface",
       {"font-antialiasing", "font-hinting", "font-rgba-order"}},
      {"org.gnome.settings-daemon.plugins.xsettings",
       {"antialiasing", "hinting", "rgba-order"}},
  };

  if (backend != ui::LinuxUiBackend::kWayland) {
    // The GTK codepath using these schemas is only used on Wayland.
    return true;
  }

  auto* source = g_settings_schema_source_get_default();
  if (!source) {
    return true;
  }

  for (const auto& interface : kInterfaces) {
    std::unique_ptr<GSettingsSchema, GSettingsSchemaDeleter> schema(
        g_settings_schema_source_lookup(source, interface.interface,
                                        /*recursive=*/true));
    if (!schema) {
      // Not an error, try the next schema.
      continue;
    }

    for (const char* key_string : interface.keys) {
      // Checking for the key first is required, otherwise
      // g_settings_schema_get_key() could crash.
      if (!g_settings_schema_has_key(schema.get(), key_string)) {
        LOG(ERROR) << "Schema " << interface.interface << " does not have key "
                   << key_string;
        return false;
      }

      std::unique_ptr<GSettingsSchemaKey, GSettingsSchemaKeyDeleter> key(
          g_settings_schema_get_key(schema.get(), key_string));
      if (!key) {
        LOG(ERROR) << "Schema " << interface.interface << " has key "
                   << key_string << ", but g_settings_schema_get_key() failed";
        return false;
      }

      std::unique_ptr<GVariant, GVariantDeleter> range(
          g_settings_schema_key_get_range(key.get()));
      if (!range) {
        LOG(ERROR) << "Schema " << interface.interface << " key " << key_string
                   << " has no range, but it is required";
        return false;
      }

      char* type_string = nullptr;
      g_variant_get(range.get(), "(sv)", &type_string, nullptr);
      std::unique_ptr<gchar, GFreeDeleter> type_string_deleter(type_string);
      if (!type_string || type_string != std::string_view("enum")) {
        LOG(ERROR) << "Schema " << interface.interface << " key " << key_string
                   << " must be an enum";
        return false;
      }
    }

    // Valid schema. Return now since GTK uses the first present schema.
    return true;
  }

  // No schema found. This is acceptable, because GTK will fallback to using
  // default values.
  return true;
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
  const auto* delegate = ui::LinuxUiDelegate::GetInstance();
  DCHECK(delegate);
  const auto backend = delegate->GetBackend();

  if (!IsValidSchema(backend)) {
    return false;
  }

  if (!LoadGtk(backend) || !GtkCheckVersion(3, 20)) {
    return false;
  }

  // Gtk initialization through pango may call FcInit() before we get to that.
  // Retrieve global FontConfig config here to call FcInit() with configuration
  // we control.
  gfx::GetGlobalFontConfig();

  platform_ = CreateGtkUiPlatform(backend);

  // Avoid GTK initializing atk-bridge, and let AuraLinux implementation
  // do it once it is ready.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("NO_AT_BRIDGE", "1");
  // gtk_init_check() modifies argv, so make a copy first.
  CmdLineArgs cmd_line = CopyCmdLine(*base::CommandLine::ForCurrentProcess());
  if (!GtkInitFromCommandLine(&cmd_line.argc, cmd_line.argv.data())) {
    return false;
  }

  os_settings_provider_ = std::make_unique<OsSettingsProviderGtk>();
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(&GtkUi::AddGtkNativeColorMixer,
                          base::Unretained(this)));
  native_theme_ = NativeThemeGtk::instance();

  using Action = ui::LinuxUi::WindowFrameAction;
  using ActionSource = ui::LinuxUi::WindowFrameActionSource;
  window_frame_actions_ = {
      {ActionSource::kDoubleClick, Action::kToggleMaximize},
      {ActionSource::kMiddleClick, Action::kNone},
      {ActionSource::kRightClick, Action::kMenu}};

  auto connect = [&](auto* sender, const char* detailed_signal, auto receiver) {
    // Unretained() is safe since GtkUi will own the ScopedGSignal.
    signals_.emplace_back(sender, detailed_signal,
                          base::BindRepeating(receiver, base::Unretained(this)),
                          G_CONNECT_AFTER);
  };

  GtkSettings* settings = gtk_settings_get_default();
  connect(settings, "notify::gtk-theme-name", &GtkUi::OnThemeChanged);
  connect(settings, "notify::gtk-icon-theme-name", &GtkUi::OnThemeChanged);
  connect(settings, "notify::gtk-application-prefer-dark-theme",
          &GtkUi::OnThemeChanged);
  connect(settings, "notify::gtk-cursor-theme-name",
          &GtkUi::OnCursorThemeNameChanged);
  connect(settings, "notify::gtk-cursor-theme-size",
          &GtkUi::OnCursorThemeSizeChanged);
  connect(settings, "notify::gtk-enable-animations",
          &GtkUi::OnEnableAnimationsChanged);
  connect(settings, "notify::gtk-enable-primary-paste",
          &GtkUi::OnPrimaryPasteChanged);

  // Listen for DPI changes, if supported.
  if (GetXftDpi() > 0) {
    connect(settings, "notify::gtk-xft-dpi", &GtkUi::OnGtkXftDpiChanged);
  } else {
    GdkScreen* screen = gdk_screen_get_default();
    connect(screen, "notify::resolution", &GtkUi::OnScreenResolutionChanged);
  }

  // Listen for scale factor changes.
  GdkDisplay* display = gdk_display_get_default();
  if (GtkCheckVersion(4)) {
    GListModel* monitors = gdk_display_get_monitors(display);
    connect(monitors, "items-changed", &GtkUi::OnMonitorsChanged);
    const guint n_monitors = g_list_model_get_n_items(monitors);
    OnMonitorsChanged(monitors, 0, 0, n_monitors);
  } else {
    connect(display, "monitor-added", &GtkUi::OnMonitorAdded);
    connect(display, "monitor-removed", &GtkUi::OnMonitorRemoved);
    const int n_monitors = gdk_display_get_n_monitors(display);
    for (int i = 0; i < n_monitors; i++) {
      TrackMonitor(gdk_display_get_monitor(display, i));
    }
  }

  LoadGtkValues();

  // We must build this after GTK gets initialized.
  settings_provider_ = std::make_unique<SettingsProviderGtk>(this);

  indicators_count = 0;

  platform_->OnInitialized();

  return true;
}

void GtkUi::InitializeFontSettings() {
  gfx::SetFontRenderParamsDeviceScaleFactor(display_config().primary_scale);

  auto fake_label = TakeGObject(gtk_label_new(nullptr));
  PangoContext* pc = gtk_widget_get_pango_context(fake_label);
  const PangoFontDescription* desc = pango_context_get_font_description(pc);

  // Use gfx::FontRenderParams to select a family and determine the rendering
  // settings.
  gfx::FontRenderParamsQuery query;
  query.families =
      base::SplitString(pango_font_description_get_family(desc), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  double pango_size =
      pango_font_description_get_size(desc) / static_cast<double>(PANGO_SCALE);
  if (GtkCheckVersion(4)) {
    pango_size /= FontScale();
  }
  double size_pixels;
  if (pango_font_description_get_size_is_absolute(desc)) {
    // If the size is absolute, it's specified in Pango units. There are
    // PANGO_SCALE Pango units in a device unit (pixel).
    size_pixels = pango_size;
    query.pixel_size = std::round(size_pixels);
  } else {
    // Non-absolute sizes are in points (again scaled by PANGO_SIZE).
    // Round the value when converting to pixels to match GTK's logic.
    size_pixels = pango_size * kDefaultDPI / 72.0;
    query.point_size = std::round(pango_size);
  }
  if (!platform_->IncludeFontScaleInDeviceScale()) {
    size_pixels *= FontScale();
  }

  query.style = gfx::Font::NORMAL;
  query.weight =
      static_cast<gfx::Font::Weight>(pango_font_description_get_weight(desc));
  // TODO(davemoore): What about PANGO_STYLE_OBLIQUE?
  if (pango_font_description_get_style(desc) == PANGO_STYLE_ITALIC) {
    query.style |= gfx::Font::ITALIC;
  }

  std::string default_font_family;
  default_font_render_params_ =
      gfx::GetFontRenderParams(query, &default_font_family);
  set_default_font_settings(FontSettings{
      .family = std::move(default_font_family),
      .size_pixels = base::ClampRound<int>(size_pixels),
      .style = query.style,
      .weight = static_cast<int>(query.weight),
  });
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
    if (!image_skia.isNull()) {
      image_skia.MakeThreadSafe();
    }
    return gfx::Image(image_skia);
  }
  return gfx::Image();
}

void GtkUi::SetWindowButtonOrdering(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  views::WindowButtonOrderProvider::GetInstance()->SetWindowButtonOrder(
      leading_buttons, trailing_buttons);

  window_button_order_observer_list_.Notify(
      &ui::WindowButtonOrderObserver::OnWindowButtonOrderingChange);
}

void GtkUi::SetWindowFrameAction(WindowFrameActionSource source,
                                 WindowFrameAction action) {
  window_frame_actions_[source] = action;
}

std::unique_ptr<ui::LinuxInputMethodContext> GtkUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGtkIme)) {
    return nullptr;
  }
  return GetPlatform()->CreateInputMethodContext(delegate);
}

gfx::FontRenderParams GtkUi::GetDefaultFontRenderParams() {
  static gfx::FontRenderParams params = GetGtkFontRenderParams();
  return params;
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

bool GtkUi::PrimaryPasteEnabled() const {
  gboolean paste_enabled = false;
  g_object_get(gtk_settings_get_default(), "gtk-enable-primary-paste",
               &paste_enabled, nullptr);
  return paste_enabled;
}

bool GtkUi::PreferDarkTheme() const {
  gboolean dark = false;
  g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
               &dark, nullptr);
  return dark;
}

std::vector<std::string> GtkUi::GetCmdLineFlagsForCopy() const {
  const auto& gtk_version = GtkVersion();
  uint32_t major_version =
      gtk_version.IsValid() ? gtk_version.components()[0] : 0;
  return {std::string(switches::kUiToolkitFlag) + "=gtk",
          std::string(switches::kGtkVersionFlag) + "=" +
              base::NumberToString(major_version)};
}

void GtkUi::SetDarkTheme(bool dark) {
  auto* settings = gtk_settings_get_default();
  g_object_set(settings, "gtk-application-prefer-dark-theme", dark, nullptr);
  // OnThemeChanged() will be called via the
  // notify::gtk-application-prefer-dark-theme handler to update the colors.
}

void GtkUi::SetAccentColor(std::optional<SkColor> accent_color) {
  accent_color_ = accent_color;
  native_theme_->NotifyOnNativeThemeUpdated();
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

ui::WindowFrameProvider* GtkUi::GetWindowFrameProvider(bool solid_frame,
                                                       bool tiled,
                                                       bool maximized) {
  auto& provider = frame_providers_[solid_frame][tiled][maximized];
  if (!provider) {
    provider = std::make_unique<gtk::WindowFrameProviderGtk>(solid_frame, tiled,
                                                             maximized);
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

  for (const ui::DomCode domcode : ui::kWritingSystemKeyDomCodes) {
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
        if (UNSAFE_TODO(keys[i]).level == 0) {
          uint16_t unicode = gdk_keyval_to_unicode(UNSAFE_TODO(keyvals[i]));
          if (unicode == 0) {
            for (const auto& i_dead : kDeadKeyMapping) {
              if (UNSAFE_TODO(keyvals[i]) == i_dead.gdk_key) {
                unicode = i_dead.unicode;
              }
            }
          }
          if (unicode != 0) {
            layouts->GetLayout(UNSAFE_TODO(keys[i]).group)
                ->AddKeyMapping(domcode, unicode);
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
  if (platform_->IncludeScaleInCursorSize()) {
    CHECK(GtkCheckVersion(4));
    GdkDisplay* display = gdk_display_get_default();
    GListModel* list = gdk_display_get_monitors(display);
    auto n_monitors = g_list_model_get_n_items(list);
    if (n_monitors) {
      GdkMonitor* primary =
          static_cast<GdkMonitor*>(g_list_model_get_item(list, 0));
      size *= gdk_monitor_get_scale_factor(primary);
    }
  }
  return size;
}

ui::TextEditCommand GtkUi::GetTextEditCommandForEvent(const ui::Event& event,
                                                      int text_flags) {
  // Skip mapping arrow keys to edit commands for vertical text fields in a
  // renderer.  Blink handles them.  See crbug.com/484651.
  if (text_flags & ui::TEXT_INPUT_FLAG_VERTICAL) {
    ui::KeyboardCode code = event.AsKeyEvent()->key_code();
    if (code == ui::VKEY_LEFT || code == ui::VKEY_RIGHT ||
        code == ui::VKEY_UP || code == ui::VKEY_DOWN) {
      return ui::TextEditCommand::INVALID_COMMAND;
    }
  }

  // Ensure that we have a keyboard handler.
  if (!key_bindings_handler_) {
    key_bindings_handler_ = std::make_unique<GtkKeyBindingsHandler>();
  }

  return key_bindings_handler_->MatchEvent(event);
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
  LoadGtkValues();
  native_theme_->NotifyOnNativeThemeUpdated();
}

void GtkUi::OnCursorThemeNameChanged(GtkSettings* settings,
                                     GtkParamSpec* param) {
  std::string cursor_theme_name = GetCursorThemeName();
  if (cursor_theme_name.empty()) {
    return;
  }
  cursor_theme_observers().Notify(
      &ui::CursorThemeManagerObserver::OnCursorThemeNameChanged,
      cursor_theme_name);
}

void GtkUi::OnCursorThemeSizeChanged(GtkSettings* settings,
                                     GtkParamSpec* param) {
  int cursor_theme_size = GetCursorThemeSize();
  if (!cursor_theme_size) {
    return;
  }
  cursor_theme_observers().Notify(
      &ui::CursorThemeManagerObserver::OnCursorThemeSizeChanged,
      cursor_theme_size);
}

void GtkUi::OnEnableAnimationsChanged(GtkSettings* settings,
                                      GtkParamSpec* param) {
  gfx::Animation::UpdatePrefersReducedMotion();
}

void GtkUi::OnPrimaryPasteChanged(GtkSettings* settings, GtkParamSpec* param) {
  primary_paste_observers().Notify(
      &ui::PrimaryPastePrefObserver::OnPrimaryPastePrefChanged);
}

void GtkUi::OnGtkXftDpiChanged(GtkSettings* settings, GParamSpec* param) {
  UpdateDeviceScaleFactor();
}

void GtkUi::OnScreenResolutionChanged(GdkScreen* screen, GParamSpec* param) {
  UpdateDeviceScaleFactor();
}

void GtkUi::OnMonitorChanged(GdkMonitor* monitor, GParamSpec* param) {
  UpdateDeviceScaleFactor();
}

void GtkUi::OnMonitorAdded(GdkDisplay* display, GdkMonitor* monitor) {
  TrackMonitor(monitor);
  UpdateDeviceScaleFactor();
}

void GtkUi::OnMonitorRemoved(GdkDisplay* display, GdkMonitor* monitor) {
  monitor_signals_.erase(monitor);
  UpdateDeviceScaleFactor();
}

void GtkUi::OnMonitorsChanged(GListModel* list,
                              guint position,
                              guint removed,
                              guint added) {
  const guint n_monitors = g_list_model_get_n_items(list);
  std::unordered_set<GdkMonitor*> monitors;
  for (size_t i = 0; i < n_monitors; ++i) {
    auto* monitor = static_cast<GdkMonitor*>(g_list_model_get_item(list, i));
    if (!base::Contains(monitor_signals_, monitor)) {
      TrackMonitor(monitor);
    }
    monitors.insert(monitor);
  }
  std::erase_if(monitor_signals_, [&](const auto& pair) {
    return !base::Contains(monitors, pair.first);
  });
  UpdateDeviceScaleFactor();
}

void GtkUi::LoadGtkValues() {
  // TODO(thomasanderson): GtkThemeService had a comment here about having to
  // muck with the raw Prefs object to remove prefs::kCurrentThemeImages or else
  // we'd regress startup time. Figure out how to do that when we can't access
  // the prefs system from here.
  UpdateDeviceScaleFactor();

  // TODO(tluk): The below code sets various ThemeProvider colors for GTK. Some
  // of these definitions leverage colors that were previously defined by
  // NativeThemeGtk and are now defined as GTK ColorMixers. These ThemeProvider
  // color definitions should be added as recipes to a browser ColorMixer once
  // the Color Pipeline project begins rollout into c/b/ui. In the meantime
  // use the ColorProvider instance from the ColorProviderManager corresponding
  // to the theme bits associated with the NativeThemeGtk instance to ensure
  // we do not regress existing behavior during the transition.
  ui::ColorProviderKey key;
  if (ui::OsSettingsProvider::Get().PreferredColorScheme() ==
      ui::NativeTheme::PreferredColorScheme::kDark) {
    key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
  }
  // Some theme colors, e.g. COLOR_NTP_LINK, are derived from color provider
  // colors. We assume that those sources' colors won't change with frame type.
  key.system_theme = ui::SystemTheme::kGtk;
  const auto* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);

  SkColor location_bar_border = GetBorderColor("entry");
  if (SkColorGetA(location_bar_border)) {
    colors_[ThemeProperties::COLOR_LOCATION_BAR_BORDER] = location_bar_border;
  }

  inactive_selection_bg_color_ = GetBgColor(
      "textview.view:backdrop "
      "text:backdrop selection:backdrop");
  inactive_selection_fg_color_ = GetFgColor(
      "textview.view:backdrop "
      "text:backdrop selection:backdrop");

  SkColor tab_border = GetBorderColor("button");
  // Separates the toolbar from the bookmark bar or butter bars.
  colors_[ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR] = tab_border;
  // Separates entries in the downloads bar.
  colors_[ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR] = tab_border;

  colors_[ThemeProperties::COLOR_NTP_BACKGROUND] =
      color_provider->GetColor(ui::kColorTextfieldBackground);
  colors_[ThemeProperties::COLOR_NTP_TEXT] =
      color_provider->GetColor(ui::kColorTextfieldForeground);
  colors_[ThemeProperties::COLOR_NTP_HEADER] = GetBorderColor("button");

  SkColor tab_text_color = GetFgColor("label");
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
        custom_frame ? "headerbar.header-bar.titlebar" : "menubar";
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
        GetFgColor(header_selector + " label.title");
    const SkColor background_tab_text_color_inactive =
        GetFgColor(header_selector_inactive + " label.title");

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
    SkColor toolbar_top_separator =
        GetBorderColor(header_selector + " separator.vertical.titlebutton");
    SkColor toolbar_top_separator_inactive = GetBorderColor(
        header_selector + ":backdrop separator.vertical.titlebutton");

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
      toolbar_top_separator = GetBorderColor(header_selector + " button");
      toolbar_top_separator_inactive =
          GetBorderColor(header_selector + ":backdrop button");
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

void GtkUi::TrackMonitor(GdkMonitor* monitor) {
  auto connect = [&](const char* detailed_signal) {
    // Unretained() is safe since GtkUi will own the ScopedGSignal.
    return ScopedGSignal(
        monitor, detailed_signal,
        base::BindRepeating(&GtkUi::OnMonitorChanged, base::Unretained(this)),
        G_CONNECT_AFTER);
  };
  monitor_signals_[monitor] = {connect("notify::geometry"),
                               connect("notify::scale-factor")};
}

display::DisplayConfig GtkUi::GetDisplayConfig() const {
  display::DisplayConfig config;
  if (display::Display::HasForceDeviceScaleFactor()) {
    config.primary_scale = display::Display::GetForcedDeviceScaleFactor();
    return config;
  }

  const double font_scale =
      platform_->IncludeFontScaleInDeviceScale() ? FontScale() : 1.0;

  GdkDisplay* display = gdk_display_get_default();
  GdkMonitor* primary = nullptr;
  std::vector<GdkMonitor*> monitors;
  if (GtkCheckVersion(4)) {
    GListModel* list = gdk_display_get_monitors(display);
    auto n_monitors = g_list_model_get_n_items(list);
    if (!n_monitors) {
      return config;
    }
    primary = static_cast<GdkMonitor*>(g_list_model_get_item(list, 0));
    monitors.reserve(n_monitors);
    for (unsigned int i = 0; i < n_monitors; ++i) {
      monitors.push_back(
          static_cast<GdkMonitor*>(g_list_model_get_item(list, i)));
    }
  } else {
    const int n_monitors = gdk_display_get_n_monitors(display);
    monitors.reserve(n_monitors);
    for (int i = 0; i < n_monitors; i++) {
      monitors.push_back(gdk_display_get_monitor(display, i));
    }
    // In GDK3 Wayland this is always NULL; Fallback to the first monitor then.
    // https://gitlab.gnome.org/GNOME/gtk/-/issues/1028
    primary = gdk_display_get_primary_monitor(display);
    if (!primary && !monitors.empty()) {
      primary = monitors.front();
    }
  }
  if (!primary) {
    return config;
  }
  config.primary_scale =
      std::max(1, gdk_monitor_get_scale_factor(primary)) * font_scale;
  config.font_scale = font_scale;
  config.display_geometries.reserve(monitors.size());
  for (GdkMonitor* monitor : monitors) {
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    int monitor_scale = std::max(1, gdk_monitor_get_scale_factor(monitor));
    config.display_geometries.emplace_back(
        gfx::Rect(monitor_scale * geometry.x, monitor_scale * geometry.y,
                  monitor_scale * geometry.width,
                  monitor_scale * geometry.height),
        monitor_scale * font_scale);
  }
  return config;
}

void GtkUi::AddGtkNativeColorMixer(ui::ColorProvider* provider,
                                   const ui::ColorProviderKey& key) {
  gtk::AddGtkNativeColorMixer(provider, key, accent_color_);
}

void GtkUi::UpdateDeviceScaleFactor() {
  auto new_config = GetDisplayConfig();
  if (display_config() != new_config) {
    display_config() = std::move(new_config);
    device_scale_factor_observer_list().Notify(
        &ui::DeviceScaleFactorObserver::OnDeviceScaleFactorChanged);
  }
  set_default_font_settings(std::nullopt);
  default_font_render_params_.reset();
  // On GTK4, the cursor theme size depends on the display scale factor.
  OnCursorThemeSizeChanged(gtk_settings_get_default(), nullptr);
}

}  // namespace gtk

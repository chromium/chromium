// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// IMPORTANT NOTE: All QtUi members that use `shim_` must be decorated
// with DISABLE_CFI_VCALL.

#include "ui/qt/qt_ui.h"

#include <dlfcn.h>

#include <algorithm>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/memory/raw_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_canvas.h"
#include "chrome/browser/themes/theme_properties.h"  // nogncheck
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/font_render_params_linux.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/linux/device_scale_factor_observer.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_base.h"
#include "ui/qt/qt_interface.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/controls/button/label_button_border.h"

namespace qt {

namespace {

const char kQtVersionFlag[] = "qt-version";

void* LoadLibrary(const base::FilePath& path) {
  return dlopen(path.value().c_str(), RTLD_NOW | RTLD_GLOBAL);
}

bool PreferQt6() {
  auto* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(kQtVersionFlag)) {
    std::string qt_version_string = cmd->GetSwitchValueASCII(kQtVersionFlag);
    unsigned int qt_version = 0;
    if (base::StringToUint(qt_version_string, &qt_version)) {
      switch (qt_version) {
        case 5:
          return false;
        case 6:
          return true;
        default:
          LOG(ERROR) << "Unsupported QT version " << qt_version;
      }
    } else {
      LOG(ERROR) << "Unable to parse QT version " << qt_version_string;
    }
  }

  auto env = base::Environment::Create();
  auto desktop = base::nix::GetDesktopEnvironment(env.get());
  return desktop == base::nix::DESKTOP_ENVIRONMENT_KDE6;
}

int Qt5WeightToCssWeight(int weight) {
  struct {
    int qt_weight;
    int css_weight;
  } constexpr kMapping[] = {
      // https://doc.qt.io/qt-5/qfont.html#Weight-enum
      {0, 100},  {12, 200}, {25, 300}, {50, 400}, {57, 500},
      {63, 600}, {75, 700}, {81, 800}, {87, 900}, {99, 1000},
  };

  weight = std::clamp(weight, 0, 99);
  for (size_t i = 0; i < std::size(kMapping) - 1; i++) {
    const auto& lo = kMapping[i];
    const auto& hi = kMapping[i + 1];
    if (weight <= hi.qt_weight) {
      return (weight - lo.qt_weight) * (hi.css_weight - lo.css_weight) /
                 (hi.qt_weight - lo.qt_weight) +
             lo.css_weight;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return kMapping[std::size(kMapping) - 1].css_weight;
}

gfx::FontRenderParams::Hinting QtHintingToGfxHinting(
    qt::FontHinting hinting,
    gfx::FontRenderParams::Hinting default_hinting) {
  switch (hinting) {
    case FontHinting::kDefault:
      return default_hinting;
    case FontHinting::kNone:
      return gfx::FontRenderParams::HINTING_NONE;
    case FontHinting::kLight:
      return gfx::FontRenderParams::HINTING_SLIGHT;
    case FontHinting::kFull:
      return gfx::FontRenderParams::HINTING_FULL;
  }
}

}  // namespace

class QtNativeTheme : public ui::NativeThemeAura {
 public:
  explicit QtNativeTheme(QtInterface* shim)
      : ui::NativeThemeAura(/*use_overlay_scrollbars=*/false,
                            /*should_only_use_dark_colors=*/false,
                            ui::SystemTheme::kQt),
        shim_(shim) {}
  QtNativeTheme(const QtNativeTheme&) = delete;
  QtNativeTheme& operator=(const QtNativeTheme&) = delete;
  ~QtNativeTheme() override = default;

  void ThemeChanged(bool prefer_dark_theme) {
    set_use_dark_colors(IsForcedDarkMode() || prefer_dark_theme);
    set_preferred_color_scheme(CalculatePreferredColorScheme());

    NotifyOnNativeThemeUpdated();
  }

  // ui::NativeTheme:
  DISABLE_CFI_VCALL
  void PaintFrameTopArea(cc::PaintCanvas* canvas,
                         State state,
                         const gfx::Rect& rect,
                         const FrameTopAreaExtraParams& frame_top_area,
                         ColorScheme color_scheme) const override {
    auto image = shim_->DrawHeader(
        rect.width(), rect.height(), frame_top_area.default_background_color,
        frame_top_area.is_active ? ColorState::kNormal : ColorState::kInactive,
        frame_top_area.use_custom_frame);
    SkImageInfo image_info = SkImageInfo::Make(
        image.width, image.height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.installPixels(
        image_info, image.data_argb.Take(), image_info.minRowBytes(),
        [](void* data, void*) { free(data); }, nullptr);
    bitmap.setImmutable();
    canvas->drawImage(cc::PaintImage::CreateFromBitmap(std::move(bitmap)),
                      rect.x(), rect.y());
  }

 private:
  raw_ptr<QtInterface> const shim_;
};

QtUi::QtUi(ui::LinuxUi* fallback_linux_ui)
    : fallback_linux_ui_(fallback_linux_ui) {}

QtUi::~QtUi() = default;

std::unique_ptr<ui::LinuxInputMethodContext> QtUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  return fallback_linux_ui_
             ? fallback_linux_ui_->CreateInputMethodContext(delegate)
             : nullptr;
}

gfx::FontRenderParams QtUi::GetDefaultFontRenderParams() {
  if (!font_params_.has_value()) {
    InitializeFontSettings();
  }
  return *font_params_;
}

ui::SelectFileDialog* QtUi::CreateSelectFileDialog(
    void* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) const {
  return fallback_linux_ui_ ? fallback_linux_ui_->CreateSelectFileDialog(
                                  listener, std::move(policy))
                            : nullptr;
}

DISABLE_CFI_DLSYM
DISABLE_CFI_VCALL
bool QtUi::Initialize() {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_MODULE, &path)) {
    return false;
  }
  void* libqt_shim = nullptr;
  auto load_libqt_shim = [&](int qt_version) -> bool {
    auto file_name = base::StringPrintf("libqt%d_shim.so", qt_version);
    if ((libqt_shim = LoadLibrary(path.Append(file_name)))) {
      qt_version_ = qt_version;
    }
    return libqt_shim;
  };
  PreferQt6() ? load_libqt_shim(6) || load_libqt_shim(5)
              : load_libqt_shim(5) || load_libqt_shim(6);
  if (!libqt_shim) {
    return false;
  }
  void* create_qt_interface = dlsym(libqt_shim, "CreateQtInterface");
  DCHECK(create_qt_interface);

  // Under certain conditions, a hang may occur in libICE when reading from the
  // ICE connection.  Chrome doesn't use QT's session save/restore capabilities
  // and instead manages it's own sessions, so this is not needed anyway.  Unset
  // SESSION_MANAGER to prevent creating an ICE connection.  See [1] and [2].
  // [1] https://crbug.com/1450759
  // [2] https://bugreports.qt.io/browse/QTBUG-38599
  base::ScopedEnvironmentVariableOverride env_override("SESSION_MANAGER");

  auto cmd_line = *base::CommandLine::ForCurrentProcess();
  if (auto* delegate = ui::LinuxUiDelegate::GetInstance()) {
    // Ensure QT is initialized with the same display server protocol as Chrome.
    // In particular, when running under XWayland, make sure to use the xcb QT
    // backend instead of the wayland backend.
    switch (delegate->GetBackend()) {
      case ui::LinuxUiBackend::kStub:
        break;
      case ui::LinuxUiBackend::kX11:
        cmd_line.AppendArg("-platform");
        cmd_line.AppendArg("xcb");
        break;
      case ui::LinuxUiBackend::kWayland:
        cmd_line.AppendArg("-platform");
        cmd_line.AppendArg("wayland");
        break;
    }
  }
  cmd_line_ = CopyCmdLine(cmd_line);
  shim_.reset((reinterpret_cast<decltype(&CreateQtInterface)>(
      create_qt_interface)(this, &cmd_line_.argc, cmd_line_.argv.data())));
  native_theme_ = std::make_unique<QtNativeTheme>(shim_.get());
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(&QtUi::AddNativeColorMixer, base::Unretained(this)));
  ScaleFactorMaybeChangedImpl();

  return true;
}

DISABLE_CFI_VCALL
void QtUi::InitializeFontSettings() {
  auto params = shim_->GetFontRenderParams();
  auto desc = shim_->GetFontDescription();

  gfx::FontRenderParamsQuery query;
  query.families = {desc.family.c_str()};
  // Points are defined at 72 DPI and pixels are 96 DPI by default.
  constexpr double kPointToPixelRatio = 96.0 / 72.0;
  if (desc.size_pixels > 0) {
    query.pixel_size = desc.size_pixels;
    query.point_size = std::round(query.pixel_size / kPointToPixelRatio);
  } else {
    query.point_size = desc.size_points;
    query.pixel_size = std::round(query.point_size * kPointToPixelRatio);
  }
  query.style = desc.is_italic ? gfx::Font::ITALIC : gfx::Font::NORMAL;
  int weight =
      qt_version_ == 5 ? Qt5WeightToCssWeight(desc.weight) : desc.weight;
  query.weight = static_cast<gfx::Font::Weight>(weight);

  gfx::FontRenderParams fc_params;
  gfx::QueryFontconfig(query, &fc_params, nullptr);
  font_params_ = gfx::FontRenderParams{
      .antialiasing = params.antialiasing,
      .use_bitmaps = params.use_bitmaps,
      .hinting = QtHintingToGfxHinting(params.hinting, fc_params.hinting),
      // QT doesn't expose a subpixel rendering setting, so fall back to
      // fontconfig for it.
      .subpixel_rendering = fc_params.subpixel_rendering,
  };
  set_default_font_settings(FontSettings{
      .family = std::move(query.families[0]),
      .size_pixels = query.pixel_size,
      .style = query.style,
      .weight = static_cast<int>(query.weight),
  });
}

ui::NativeTheme* QtUi::GetNativeTheme() const {
  return native_theme_.get();
}

bool QtUi::GetColor(int id, SkColor* color, bool use_custom_frame) const {
  auto value = GetColor(id, use_custom_frame);
  if (value) {
    *color = *value;
  }
  return value.has_value();
}

bool QtUi::GetDisplayProperty(int id, int* result) const {
  switch (id) {
    case ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR:
      *result = false;
      return true;
    default:
      return false;
  }
}

DISABLE_CFI_VCALL
void QtUi::GetFocusRingColor(SkColor* color) const {
  *color = shim_->GetColor(ColorType::kHighlightBg, ColorState::kNormal);
}

DISABLE_CFI_VCALL
void QtUi::GetActiveSelectionBgColor(SkColor* color) const {
  *color = shim_->GetColor(ColorType::kHighlightBg, ColorState::kNormal);
}

DISABLE_CFI_VCALL
void QtUi::GetActiveSelectionFgColor(SkColor* color) const {
  *color = shim_->GetColor(ColorType::kHighlightFg, ColorState::kNormal);
}

DISABLE_CFI_VCALL
void QtUi::GetInactiveSelectionBgColor(SkColor* color) const {
  *color = shim_->GetColor(ColorType::kHighlightBg, ColorState::kInactive);
}

DISABLE_CFI_VCALL
void QtUi::GetInactiveSelectionFgColor(SkColor* color) const {
  *color = shim_->GetColor(ColorType::kHighlightFg, ColorState::kInactive);
}

DISABLE_CFI_VCALL
base::TimeDelta QtUi::GetCursorBlinkInterval() const {
  return base::Milliseconds(shim_->GetCursorBlinkIntervalMs());
}

DISABLE_CFI_VCALL
gfx::Image QtUi::GetIconForContentType(const std::string& content_type,
                                       int size,
                                       float scale) const {
  Image image =
      shim_->GetIconForContentType(String(content_type.c_str()), size * scale);
  if (!image.data_argb.size()) {
    return {};
  }

  SkImageInfo image_info = SkImageInfo::Make(
      image.width, image.height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.installPixels(
      image_info, image.data_argb.Take(), image_info.minRowBytes(),
      [](void* data, void*) { free(data); }, nullptr);
  gfx::ImageSkia image_skia =
      gfx::ImageSkia::CreateFromBitmap(bitmap, image.scale);
  image_skia.MakeThreadSafe();
  return gfx::Image(image_skia);
}

QtUi::WindowFrameAction QtUi::GetWindowFrameAction(
    WindowFrameActionSource source) {
  // QT doesn't have settings for the window frame action since it prefers
  // server-side decorations.  So use the hardcoded behavior of a QMdiSubWindow,
  // which also matches the default Chrome behavior when there's no LinuxUI.
  switch (source) {
    case WindowFrameActionSource::kDoubleClick:
      return WindowFrameAction::kToggleMaximize;
    case WindowFrameActionSource::kMiddleClick:
      return WindowFrameAction::kNone;
    case WindowFrameActionSource::kRightClick:
      return WindowFrameAction::kMenu;
  }
}

DISABLE_CFI_VCALL
bool QtUi::PreferDarkTheme() const {
  return color_utils::IsDark(
      shim_->GetColor(ColorType::kWindowBg, ColorState::kNormal));
}

DISABLE_CFI_VCALL
void QtUi::SetDarkTheme(bool dark) {
  // Qt::ColorScheme is only available in QT 6.5 and later.
}

DISABLE_CFI_VCALL
void QtUi::SetAccentColor(std::optional<SkColor> accent_color) {
  accent_color_ = accent_color;
  ThemeChanged();
}

DISABLE_CFI_VCALL
bool QtUi::AnimationsEnabled() const {
  return shim_->GetAnimationDurationMs() > 0;
}

void QtUi::AddWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {
  if (fallback_linux_ui_) {
    fallback_linux_ui_->AddWindowButtonOrderObserver(observer);
  }
}

void QtUi::RemoveWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {
  if (fallback_linux_ui_) {
    fallback_linux_ui_->RemoveWindowButtonOrderObserver(observer);
  }
}

std::unique_ptr<ui::NavButtonProvider> QtUi::CreateNavButtonProvider() {
  // QT prefers server-side decorations.
  return nullptr;
}

ui::WindowFrameProvider* QtUi::GetWindowFrameProvider(bool solid_frame,
                                                      bool tiled) {
  // QT prefers server-side decorations.
  return nullptr;
}

base::flat_map<std::string, std::string> QtUi::GetKeyboardLayoutMap() {
  return fallback_linux_ui_ ? fallback_linux_ui_->GetKeyboardLayoutMap()
                            : base::flat_map<std::string, std::string>{};
}

std::string QtUi::GetCursorThemeName() {
  // This is only used on X11 where QT obtains the cursor theme from XSettings.
  // However, ui/base/x/x11_cursor_loader.cc already handles this.
  return std::string();
}

int QtUi::GetCursorThemeSize() {
  // This is only used on X11 where QT obtains the cursor size from XSettings.
  // However, ui/base/x/x11_cursor_loader.cc already handles this.
  return 0;
}

bool QtUi::GetTextEditCommandsForEvent(
    const ui::Event& event,
    int text_flags,
    std::vector<ui::TextEditCommandAuraLinux>* commands) {
  // QT doesn't have "key themes" (eg. readline bindings) like GTK.
  return false;
}

#if BUILDFLAG(ENABLE_PRINTING)
printing::PrintDialogLinuxInterface* QtUi::CreatePrintDialog(
    printing::PrintingContextLinux* context) {
  return fallback_linux_ui_ ? fallback_linux_ui_->CreatePrintDialog(context)
                            : nullptr;
}

gfx::Size QtUi::GetPdfPaperSize(printing::PrintingContextLinux* context) {
  return fallback_linux_ui_ ? fallback_linux_ui_->GetPdfPaperSize(context)
                            : gfx::Size();
}
#endif

void QtUi::FontChanged() {
  set_default_font_settings(std::nullopt);
  font_params_ = std::nullopt;
}

void QtUi::ThemeChanged() {
  native_theme_->ThemeChanged(PreferDarkTheme());
}

void QtUi::ScaleFactorMaybeChanged() {
  // This gets called whenever the monitor configuration changes. Handle the
  // scale change asynchronously to allow the change to propagate to QT's scale
  // factor. This also coalesces scale change events together.
  if (!scale_factor_task_active_) {
    scale_factor_task_active_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&QtUi::ScaleFactorMaybeChangedImpl,
                                  weak_factory_.GetWeakPtr()));
  }
}

DISABLE_CFI_VCALL
void QtUi::AddNativeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  if (key.system_theme != ui::SystemTheme::kQt) {
    return;
  }

  ui::ColorMixer& mixer = provider->AddMixer();
  // These color constants are required by native_chrome_color_mixer_linux.cc
  struct {
    ui::ColorId id;
    ColorType role;
    ColorState state = ColorState::kNormal;
  } const kMaps[] = {
      // Core colors
      {ui::kColorDisabledForeground, ColorType::kWindowFg,
       ColorState::kDisabled},
      {ui::kColorEndpointBackground, ColorType::kEntryBg},
      {ui::kColorEndpointForeground, ColorType::kEntryFg},
      {ui::kColorMidground, ColorType::kMidground},
      {ui::kColorPrimaryBackground, ColorType::kWindowBg},
      {ui::kColorPrimaryForeground, ColorType::kWindowFg},
      {ui::kColorSecondaryForeground, ColorType::kWindowFg,
       ColorState::kDisabled},
      {ui::kColorSubtleAccent, ColorType::kHighlightBg, ColorState::kInactive},
      {ui::kColorSubtleEmphasisBackground, ColorType::kWindowBg},

      // UI element colors
      {ui::kColorMenuBackground, ColorType::kEntryBg},
      {ui::kColorMenuItemForeground, ColorType::kEntryFg},
      {ui::kColorMenuItemForegroundHighlighted, ColorType::kHighlightFg},
      {ui::kColorMenuItemForegroundSelected, ColorType::kHighlightFg},
      {ui::kColorBubbleBackground, ColorType::kEntryBg},
      {ui::kColorBubbleFooterBackground, ColorType::kWindowBg},
      {ui::kColorTextSelectionForeground, ColorType::kHighlightFg},

      // Platform-specific UI elements
      {ui::kColorNativeButtonBorder, ColorType::kMidground},
      {ui::kColorNativeHeaderButtonBorderActive, ColorType::kMidground},
      {ui::kColorNativeHeaderButtonBorderInactive, ColorType::kMidground,
       ColorState::kInactive},
      {ui::kColorNativeHeaderSeparatorBorderActive, ColorType::kMidground},
      {ui::kColorNativeHeaderSeparatorBorderInactive, ColorType::kMidground,
       ColorState::kInactive},
      {ui::kColorNativeLabelForeground, ColorType::kWindowFg},
      {ui::kColorNativeTextfieldBorderUnfocused, ColorType::kMidground,
       ColorState::kInactive},
      {ui::kColorNativeToolbarBackground, ColorType::kButtonBg},
  };
  for (const auto& map : kMaps) {
    mixer[map.id] = {shim_->GetColor(map.role, map.state)};
  }

  const ui::ColorId kAccentIds[] = {
      ui::kColorAccent,
      ui::kColorItemHighlight,
      ui::kColorItemSelectionBackground,
      ui::kColorMenuSelectionBackground,
      ui::kColorTextSelectionBackground,
      ui::kColorMenuItemBackgroundHighlighted,
      ui::kColorMenuItemBackgroundSelected,
  };
  const SkColor accent = accent_color_.value_or(
      shim_->GetColor(ColorType::kHighlightBg, ColorState::kNormal));
  for (ui::ColorId accent_id : kAccentIds) {
    mixer[accent_id] = {accent};
  }

  const bool use_custom_frame =
      key.frame_type == ui::ColorProviderKey::FrameType::kChromium;
  mixer[ui::kColorFrameActive] = {
      shim_->GetFrameColor(ColorState::kNormal, use_custom_frame)};
  mixer[ui::kColorFrameInactive] = {
      shim_->GetFrameColor(ColorState::kInactive, use_custom_frame)};

  const SkColor button_fg =
      shim_->GetColor(ColorType::kButtonFg, ColorState::kNormal);
  mixer[ui::kColorNativeTabForegroundInactiveFrameActive] =
      ui::BlendForMinContrast({button_fg}, {ui::kColorFrameActive});
  mixer[ui::kColorNativeTabForegroundInactiveFrameInactive] =
      ui::BlendForMinContrast({button_fg}, {ui::kColorFrameInactive});
}

DISABLE_CFI_VCALL
std::optional<SkColor> QtUi::GetColor(int id, bool use_custom_frame) const {
  switch (id) {
    case ThemeProperties::COLOR_LOCATION_BAR_BORDER:
      return shim_->GetColor(ColorType::kEntryFg, ColorState::kNormal);
    case ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
      return shim_->GetColor(ColorType::kButtonFg, ColorState::kNormal);
    case ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR:
      return shim_->GetColor(ColorType::kButtonFg, ColorState::kNormal);
    case ThemeProperties::COLOR_NTP_BACKGROUND:
      return shim_->GetColor(ColorType::kEntryBg, ColorState::kNormal);
    case ThemeProperties::COLOR_NTP_TEXT:
      return shim_->GetColor(ColorType::kEntryFg, ColorState::kNormal);
    case ThemeProperties::COLOR_NTP_HEADER:
      return shim_->GetColor(ColorType::kButtonFg, ColorState::kNormal);
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON:
      return shim_->GetColor(ColorType::kWindowFg, ColorState::kNormal);
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_HOVERED:
      return shim_->GetColor(ColorType::kWindowFg, ColorState::kNormal);
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_PRESSED:
      return shim_->GetColor(ColorType::kWindowFg, ColorState::kNormal);
    case ThemeProperties::COLOR_TOOLBAR_TEXT:
      return shim_->GetColor(ColorType::kWindowFg, ColorState::kNormal);
    case ThemeProperties::COLOR_NTP_LINK:
      return shim_->GetColor(ColorType::kHighlightBg, ColorState::kNormal);
    case ThemeProperties::COLOR_FRAME_ACTIVE:
      return shim_->GetFrameColor(ColorState::kNormal, use_custom_frame);
    case ThemeProperties::COLOR_FRAME_INACTIVE:
      return shim_->GetFrameColor(ColorState::kInactive, use_custom_frame);
    case ThemeProperties::COLOR_FRAME_ACTIVE_INCOGNITO:
      return shim_->GetFrameColor(ColorState::kNormal, use_custom_frame);
    case ThemeProperties::COLOR_FRAME_INACTIVE_INCOGNITO:
      return shim_->GetFrameColor(ColorState::kInactive, use_custom_frame);
    case ThemeProperties::COLOR_TOOLBAR:
      return shim_->GetColor(ColorType::kButtonBg, ColorState::kNormal);
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE:
      return shim_->GetColor(ColorType::kButtonBg, ColorState::kNormal);
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE:
      return shim_->GetColor(ColorType::kButtonBg, ColorState::kInactive);
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE:
      return color_utils::BlendForMinContrast(
                 shim_->GetColor(ColorType::kButtonBg, ColorState::kNormal),
                 shim_->GetFrameColor(ColorState::kNormal, use_custom_frame))
          .color;
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE:
      return color_utils::BlendForMinContrast(
                 shim_->GetColor(ColorType::kButtonBg, ColorState::kInactive),
                 shim_->GetFrameColor(ColorState::kInactive, use_custom_frame))
          .color;
    case ThemeProperties::COLOR_TAB_STROKE_FRAME_ACTIVE:
      return color_utils::BlendForMinContrast(
                 shim_->GetColor(ColorType::kButtonBg, ColorState::kNormal),
                 shim_->GetColor(ColorType::kButtonBg, ColorState::kNormal),
                 SK_ColorBLACK, 2.0)
          .color;
    case ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE:
      return color_utils::BlendForMinContrast(
                 shim_->GetColor(ColorType::kButtonBg, ColorState::kInactive),
                 shim_->GetColor(ColorType::kButtonBg, ColorState::kInactive),
                 SK_ColorBLACK, 2.0)
          .color;
    default:
      return std::nullopt;
  }
}

DISABLE_CFI_VCALL
void QtUi::ScaleFactorMaybeChangedImpl() {
  scale_factor_task_active_ = false;
  qt::MonitorScale* qt_monitors;
  display::DisplayConfig new_config;
  size_t n_monitors =
      shim_->GetMonitorConfig(&qt_monitors, &new_config.primary_scale);
  std::vector<display::DisplayGeometry> ui_monitors;
  ui_monitors.reserve(n_monitors);
  for (size_t i = 0; i < n_monitors; i++) {
    const qt::MonitorScale& monitor = qt_monitors[i];
    ui_monitors.push_back(display::DisplayGeometry{
        {monitor.x_px, monitor.y_px, monitor.width_px, monitor.height_px},
        monitor.scale});
  }
  if (display_config() != new_config) {
    display_config() = std::move(new_config);
    device_scale_factor_observer_list().Notify(
        &ui::DeviceScaleFactorObserver::OnDeviceScaleFactorChanged);
  }
}

std::unique_ptr<ui::LinuxUiAndTheme> CreateQtUi(
    ui::LinuxUi* fallback_linux_ui) {
  return std::make_unique<QtUi>(fallback_linux_ui);
}

}  // namespace qt

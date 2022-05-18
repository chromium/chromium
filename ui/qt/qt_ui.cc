// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/qt_ui.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/font_render_params_linux.h"
#include "ui/gfx/image/image.h"
#include "ui/qt/qt_interface.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/controls/button/label_button_border.h"

namespace qt {

namespace {

int QtWeightToCssWeight(int weight) {
  struct {
    int qt_weight;
    int css_weight;
  } constexpr kMapping[] = {
      // https://doc.qt.io/qt-5/qfont.html#Weight-enum
      {0, 100},  {12, 200}, {25, 300}, {50, 400}, {57, 500},
      {63, 600}, {75, 700}, {81, 800}, {87, 900}, {99, 1000},
  };

  weight = base::clamp(weight, 0, 99);
  for (size_t i = 0; i < std::size(kMapping) - 1; i++) {
    const auto& lo = kMapping[i];
    const auto& hi = kMapping[i + 1];
    if (weight <= hi.qt_weight) {
      return (weight - lo.qt_weight) * (hi.css_weight - lo.css_weight) /
                 (hi.qt_weight - lo.qt_weight) +
             lo.css_weight;
    }
  }
  NOTREACHED();
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

QtUi::QtUi() = default;

QtUi::~QtUi() = default;

std::unique_ptr<ui::LinuxInputMethodContext> QtUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

gfx::FontRenderParams QtUi::GetDefaultFontRenderParams() const {
  return font_params_;
}

void QtUi::GetDefaultFontDescription(std::string* family_out,
                                     int* size_pixels_out,
                                     int* style_out,
                                     gfx::Font::Weight* weight_out,
                                     gfx::FontRenderParams* params_out) const {
  if (family_out)
    *family_out = font_family_;
  if (size_pixels_out)
    *size_pixels_out = font_size_pixels_;
  if (style_out)
    *style_out = font_style_;
  if (weight_out)
    *weight_out = font_weight_;
  if (params_out)
    *params_out = font_params_;
}

ui::SelectFileDialog* QtUi::CreateSelectFileDialog(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool QtUi::Initialize() {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_MODULE, &path))
    return false;
  path = path.Append("libqt5_shim.so");
  void* libqt_shim = dlopen(path.value().c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (!libqt_shim)
    return false;
  void* create_qt_interface = dlsym(libqt_shim, "CreateQtInterface");
  DCHECK(create_qt_interface);

  cmd_line_ = CopyCmdLine(*base::CommandLine::ForCurrentProcess());
  shim_.reset((reinterpret_cast<decltype(&CreateQtInterface)>(
      create_qt_interface)(this, &cmd_line_.argc, cmd_line_.argv.data())));

  FontChanged();

  return true;
}

bool QtUi::GetTint(int id, color_utils::HSL* tint) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool QtUi::GetColor(int id, SkColor* color, bool use_custom_frame) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool QtUi::GetDisplayProperty(int id, int* result) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

SkColor QtUi::GetFocusRingColor() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kPlaceholderColor;
}

SkColor QtUi::GetActiveSelectionBgColor() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kPlaceholderColor;
}

SkColor QtUi::GetActiveSelectionFgColor() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kPlaceholderColor;
}

SkColor QtUi::GetInactiveSelectionBgColor() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kPlaceholderColor;
}

SkColor QtUi::GetInactiveSelectionFgColor() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kPlaceholderColor;
}

base::TimeDelta QtUi::GetCursorBlinkInterval() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::TimeDelta();
}

ui::NativeTheme* QtUi::GetNativeTheme(aura::Window* window) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::NativeTheme::GetInstanceForNativeUi();
}

ui::NativeTheme* QtUi::GetNativeTheme(bool use_system_theme) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::NativeTheme::GetInstanceForNativeUi();
}

void QtUi::SetUseSystemThemeCallback(UseSystemThemeCallback callback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool QtUi::GetDefaultUsesSystemTheme() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

gfx::Image QtUi::GetIconForContentType(const std::string& content_type,
                                       int size,
                                       float scale) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Image();
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

float QtUi::GetDeviceScaleFactor() const {
  return shim_->GetScaleFactor();
}

bool QtUi::PreferDarkTheme() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool QtUi::AnimationsEnabled() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

std::unique_ptr<views::NavButtonProvider> QtUi::CreateNavButtonProvider() {
  // QT prefers server-side decorations.
  return nullptr;
}

views::WindowFrameProvider* QtUi::GetWindowFrameProvider(bool solid_frame) {
  // QT prefers server-side decorations.
  return nullptr;
}

base::flat_map<std::string, std::string> QtUi::GetKeyboardLayoutMap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
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

std::vector<std::string> QtUi::GetAvailableSystemThemeNamesForTest() const {
  // In QT, themes are binary plugins that are loaded on start and can't be
  // changed at runtime.  The style may change, but there's no common interface
  // for doing this from a client.  Return a single empty theme here to
  // represent the current theme.
  return {std::string()};
}

void QtUi::SetSystemThemeByNameForTest(const std::string& theme_name) {
  // Ensure we only get passed the "current theme" name that we returned from
  // GetAvailableSystemThemeNamesForTest() above.
  DCHECK(theme_name.empty());
}

bool QtUi::MatchEvent(const ui::Event& event,
                      std::vector<ui::TextEditCommandAuraLinux>* commands) {
  // QT doesn't have "key themes" (eg. readline bindings) like GTK.
  return false;
}

void QtUi::FontChanged() {
  auto params = shim_->GetFontRenderParams();
  auto desc = shim_->GetFontDescription();

  font_family_ = desc.family.c_str();
  if (desc.size_pixels > 0) {
    font_size_pixels_ = desc.size_pixels;
    font_size_points_ = font_size_pixels_ / GetDeviceScaleFactor();
  } else {
    font_size_points_ = desc.size_points;
    font_size_pixels_ = font_size_points_ * GetDeviceScaleFactor();
  }
  font_style_ = desc.is_italic ? gfx::Font::ITALIC : gfx::Font::NORMAL;
  font_weight_ =
      static_cast<gfx::Font::Weight>(QtWeightToCssWeight(desc.weight));

  gfx::FontRenderParamsQuery query;
  query.families = {font_family_};
  query.pixel_size = font_size_pixels_;
  query.point_size = font_size_points_;
  query.style = font_style_;
  query.weight = font_weight_;

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
}

std::unique_ptr<views::LinuxUI> CreateQtUi() {
  return std::make_unique<QtUi>();
}

}  // namespace qt

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/qt_ui.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/controls/button/label_button_border.h"

namespace qt {

QtUi::QtUi() = default;

QtUi::~QtUi() = default;

std::unique_ptr<ui::LinuxInputMethodContext> QtUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

gfx::FontRenderParams QtUi::GetDefaultFontRenderParams() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

void QtUi::GetDefaultFontDescription(std::string* family_out,
                                     int* size_pixels_out,
                                     int* style_out,
                                     gfx::Font::Weight* weight_out,
                                     gfx::FontRenderParams* params_out) const {
  NOTIMPLEMENTED_LOG_ONCE();
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
      create_qt_interface)(&cmd_line_.argc, cmd_line_.argv.data())));

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
  return nullptr;
}

ui::NativeTheme* QtUi::GetNativeTheme(bool use_system_theme) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
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

std::unique_ptr<views::Border> QtUi::CreateNativeBorder(
    views::LabelButton* owning_button,
    std::unique_ptr<views::LabelButtonBorder> border) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

void QtUi::AddWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void QtUi::RemoveWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

QtUi::WindowFrameAction QtUi::GetWindowFrameAction(
    WindowFrameActionSource source) {
  NOTIMPLEMENTED_LOG_ONCE();
  return views::LinuxUI::WindowFrameAction::kNone;
}

void QtUi::NotifyWindowManagerStartupComplete() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void QtUi::UpdateDeviceScaleFactor() {
  NOTIMPLEMENTED_LOG_ONCE();
}

float QtUi::GetDeviceScaleFactor() const {
  return shim_->GetScaleFactor();
}

void QtUi::AddDeviceScaleFactorObserver(
    views::DeviceScaleFactorObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void QtUi::RemoveDeviceScaleFactorObserver(
    views::DeviceScaleFactorObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
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
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

views::WindowFrameProvider* QtUi::GetWindowFrameProvider(bool solid_frame) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

base::flat_map<std::string, std::string> QtUi::GetKeyboardLayoutMap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

std::string QtUi::GetCursorThemeName() {
  NOTIMPLEMENTED_LOG_ONCE();
  return std::string();
}

int QtUi::GetCursorThemeSize() {
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
}

bool QtUi::MatchEvent(const ui::Event& event,
                      std::vector<ui::TextEditCommandAuraLinux>* commands) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

std::vector<std::string> QtUi::GetAvailableSystemThemeNamesForTest() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

void QtUi::SetSystemThemeByNameForTest(const std::string& theme_name) {
  NOTIMPLEMENTED_LOG_ONCE();
}

std::unique_ptr<views::LinuxUI> CreateQtUi() {
  return std::make_unique<QtUi>();
}

}  // namespace qt

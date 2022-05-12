// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_QT_UI_H_
#define UI_QT_QT_UI_H_

#include <memory>

#include "base/component_export.h"
#include "ui/qt/qt_interface.h"
#include "ui/views/linux_ui/linux_ui.h"

namespace qt {

// Interface to QT desktop features.
class QtUi : public views::LinuxUI {
 public:
  QtUi();

  QtUi(const QtUi&) = delete;
  QtUi& operator=(const QtUi&) = delete;

  ~QtUi() override;

  // ui::LinuxInputMethodContextFactory:
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override;

  // gfx::LinuxFontDelegate:
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      gfx::Font::Weight* weight_out,
      gfx::FontRenderParams* params_out) const override;

  // ui::ShellDialogLinux:
  ui::SelectFileDialog* CreateSelectFileDialog(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) const override;

  // views::LinuxUI:
  bool Initialize() override;
  bool GetTint(int id, color_utils::HSL* tint) const override;
  bool GetColor(int id, SkColor* color, bool use_custom_frame) const override;
  bool GetDisplayProperty(int id, int* result) const override;
  SkColor GetFocusRingColor() const override;
  SkColor GetActiveSelectionBgColor() const override;
  SkColor GetActiveSelectionFgColor() const override;
  SkColor GetInactiveSelectionBgColor() const override;
  SkColor GetInactiveSelectionFgColor() const override;
  base::TimeDelta GetCursorBlinkInterval() const override;
  ui::NativeTheme* GetNativeTheme(aura::Window* window) const override;
  ui::NativeTheme* GetNativeTheme(bool use_system_theme) const override;
  void SetUseSystemThemeCallback(UseSystemThemeCallback callback) override;
  bool GetDefaultUsesSystemTheme() const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size,
                                   float scale) const override;
  std::unique_ptr<views::Border> CreateNativeBorder(
      views::LabelButton* owning_button,
      std::unique_ptr<views::LabelButtonBorder> border) override;
  void AddWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  void RemoveWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) override;
  void NotifyWindowManagerStartupComplete() override;
  void UpdateDeviceScaleFactor() override;
  float GetDeviceScaleFactor() const override;
  void AddDeviceScaleFactorObserver(
      views::DeviceScaleFactorObserver* observer) override;
  void RemoveDeviceScaleFactorObserver(
      views::DeviceScaleFactorObserver* observer) override;
  bool PreferDarkTheme() const override;
  bool AnimationsEnabled() const override;
  std::unique_ptr<views::NavButtonProvider> CreateNavButtonProvider() override;
  views::WindowFrameProvider* GetWindowFrameProvider(bool solid_frame) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  std::string GetCursorThemeName() override;
  int GetCursorThemeSize() override;
  std::vector<std::string> GetAvailableSystemThemeNamesForTest() const override;
  void SetSystemThemeByNameForTest(const std::string& theme_name) override;

  // ui::TextEditKeybindingDelegate:
  bool MatchEvent(const ui::Event& event,
                  std::vector<ui::TextEditCommandAuraLinux>* commands) override;

 private:
  // QT modifies argc and argv, and they must be kept alive while
  // `shim_` is alive.
  CmdLineArgs cmd_line_;

  std::unique_ptr<QtInterface> shim_;
};

// This should be the only symbol exported from this component.
COMPONENT_EXPORT(QT)
std::unique_ptr<views::LinuxUI> CreateQtUi();

}  // namespace qt

#endif  // UI_QT_QT_UI_H_

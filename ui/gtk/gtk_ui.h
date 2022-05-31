// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_H_
#define UI_GTK_GTK_UI_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/gfx/color_utils.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/window_frame_provider.h"
#include "ui/views/window/frame_buttons.h"

typedef struct _GParamSpec GParamSpec;
typedef struct _GtkParamSpec GtkParamSpec;
typedef struct _GtkSettings GtkSettings;
typedef struct _GtkStyle GtkStyle;

namespace gtk {
using ColorMap = std::map<int, SkColor>;

class GtkKeyBindingsHandler;
class NativeThemeGtk;
class SettingsProvider;

// Interface to GTK desktop features.
class GtkUi : public views::LinuxUI {
 public:
  GtkUi();

  GtkUi(const GtkUi&) = delete;
  GtkUi& operator=(const GtkUi&) = delete;

  ~GtkUi() override;

  // Static delegate getter, used by different objects (created by GtkUi), e.g:
  // Dialogs, IME Context, when platform-specific functionality is required.
  static GtkUiPlatform* GetPlatform();

  // Setters used by SettingsProvider:
  void SetWindowButtonOrdering(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons);
  void SetWindowFrameAction(WindowFrameActionSource source,
                            WindowFrameAction action);

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
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size,
                                   float scale) const override;
  WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) override;
  float GetDeviceScaleFactor() const override;
  bool PreferDarkTheme() const override;
  bool AnimationsEnabled() const override;
  std::unique_ptr<views::NavButtonProvider> CreateNavButtonProvider() override;
  views::WindowFrameProvider* GetWindowFrameProvider(bool solid_frame) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  std::string GetCursorThemeName() override;
  int GetCursorThemeSize() override;
  std::vector<std::string> GetAvailableSystemThemeNamesForTest() const override;
  void SetSystemThemeByNameForTest(const std::string& theme_name) override;
  ui::NativeTheme* GetNativeTheme() const override;

  // ui::TextEditKeybindingDelegate:
  bool MatchEvent(const ui::Event& event,
                  std::vector<ui::TextEditCommandAuraLinux>* commands) override;

 private:
  using TintMap = std::map<int, color_utils::HSL>;

  CHROMEG_CALLBACK_1(GtkUi, void, OnThemeChanged, GtkSettings*, GtkParamSpec*);

  CHROMEG_CALLBACK_1(GtkUi,
                     void,
                     OnCursorThemeNameChanged,
                     GtkSettings*,
                     GtkParamSpec*);

  CHROMEG_CALLBACK_1(GtkUi,
                     void,
                     OnCursorThemeSizeChanged,
                     GtkSettings*,
                     GtkParamSpec*);

  CHROMEG_CALLBACK_1(GtkUi,
                     void,
                     OnDeviceScaleFactorMaybeChanged,
                     void*,
                     GParamSpec*);

  // Loads all GTK-provided settings.
  void LoadGtkValues();

  // Extracts colors and tints from the GTK theme, both for the
  // ThemeService interface and the colors we send to Blink.
  void UpdateColors();

  // Updates |default_font_*|.
  void UpdateDefaultFont();

  // Updates the device scale factor so that the default font size can be
  // recalculated.
  void UpdateDeviceScaleFactor();

  float GetRawDeviceScaleFactor();

  std::unique_ptr<GtkUiPlatform> platform_;

  NativeThemeGtk* native_theme_;

  // Colors calculated by LoadGtkValues() that are given to the
  // caller while |use_gtk_| is true.
  ColorMap colors_;

  // Frame colors (and colors that depend on frame colors) when using
  // Chrome-rendered borders and titlebar.
  ColorMap custom_frame_colors_;

  // Frame colors (and colors that depend on frame colors) when using
  // system-rendered borders and titlebar.
  ColorMap native_frame_colors_;

  // Colors that we pass to Blink. These are generated each time the theme
  // changes.
  SkColor focus_ring_color_;
  SkColor active_selection_bg_color_;
  SkColor active_selection_fg_color_;
  SkColor inactive_selection_bg_color_;
  SkColor inactive_selection_fg_color_;

  // Details about the default UI font.
  std::string default_font_family_;
  int default_font_size_pixels_ = 0;
  // Bitfield of gfx::Font::Style values.
  int default_font_style_ = gfx::Font::NORMAL;
  gfx::Font::Weight default_font_weight_ = gfx::Font::Weight::NORMAL;
  gfx::FontRenderParams default_font_render_params_;

  std::unique_ptr<SettingsProvider> settings_provider_;

  // This is only used on GTK3.
  std::unique_ptr<GtkKeyBindingsHandler> key_bindings_handler_;

  // The action to take when middle, double, or right clicking the titlebar.
  base::flat_map<WindowFrameActionSource, WindowFrameAction>
      window_frame_actions_;

  float device_scale_factor_ = 1.0f;

  // Paints a native window frame.  Typically only one of these will be
  // non-null.  The exception is when the user starts or stops their compositor
  // while Chrome is running.
  std::unique_ptr<views::WindowFrameProvider> solid_frame_provider_;
  std::unique_ptr<views::WindowFrameProvider> transparent_frame_provider_;
};

}  // namespace gtk

#endif  // UI_GTK_GTK_UI_H_

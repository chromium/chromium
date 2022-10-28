// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_H_
#define UI_GTK_GTK_UI_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_ptr.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/window/frame_buttons.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_context_linux.h"  // nogncheck
#endif

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
class GtkUi : public ui::LinuxUiAndTheme {
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

  // ui::LinuxUi:
  bool Initialize() override;
  base::TimeDelta GetCursorBlinkInterval() const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size,
                                   float scale) const override;
  float GetDeviceScaleFactor() const override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
#if BUILDFLAG(ENABLE_PRINTING)
  printing::PrintDialogLinuxInterface* CreatePrintDialog(
      printing::PrintingContextLinux* context) override;
  gfx::Size GetPdfPaperSize(printing::PrintingContextLinux* context) override;
#endif
  ui::SelectFileDialog* CreateSelectFileDialog(
      void* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) const override;
  std::string GetCursorThemeName() override;
  int GetCursorThemeSize() override;
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const override;
  bool GetTextEditCommandsForEvent(
      const ui::Event& event,
      std::vector<ui::TextEditCommandAuraLinux>* commands) override;
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      int* weight_out,
      gfx::FontRenderParams* params_out) const override;
  bool AnimationsEnabled() const override;
  void AddWindowButtonOrderObserver(
      ui::WindowButtonOrderObserver* observer) override;
  void RemoveWindowButtonOrderObserver(
      ui::WindowButtonOrderObserver* observer) override;
  WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) override;

  // ui::LinuxUiTheme:
  ui::NativeTheme* GetNativeTheme() const override;
  bool GetColor(int id, SkColor* color, bool use_custom_frame) const override;
  bool GetDisplayProperty(int id, int* result) const override;
  SkColor GetFocusRingColor() const override;
  SkColor GetActiveSelectionBgColor() const override;
  SkColor GetActiveSelectionFgColor() const override;
  SkColor GetInactiveSelectionBgColor() const override;
  SkColor GetInactiveSelectionFgColor() const override;
  bool PreferDarkTheme() const override;
  std::unique_ptr<ui::NavButtonProvider> CreateNavButtonProvider() override;
  ui::WindowFrameProvider* GetWindowFrameProvider(bool solid_frame) override;

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

  raw_ptr<NativeThemeGtk> native_theme_;

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
  std::unique_ptr<ui::WindowFrameProvider> solid_frame_provider_;
  std::unique_ptr<ui::WindowFrameProvider> transparent_frame_provider_;

  // Objects to notify when the window frame button order changes.
  base::ObserverList<ui::WindowButtonOrderObserver>::Unchecked
      window_button_order_observer_list_;
};

}  // namespace gtk

#endif  // UI_GTK_GTK_UI_H_

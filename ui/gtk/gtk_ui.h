// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_H_
#define UI_GTK_GTK_UI_H_

#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_ptr.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
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
typedef struct _GdkDisplay GdkDisplay;
typedef struct _GdkMonitor GdkMonitor;
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
  void InitializeFontSettings() override;
  base::TimeDelta GetCursorBlinkInterval() const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size,
                                   float scale) const override;
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
      int text_flags,
      std::vector<ui::TextEditCommandAuraLinux>* commands) override;
  gfx::FontRenderParams GetDefaultFontRenderParams() override;
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
  void GetFocusRingColor(SkColor* color) const override;
  void GetActiveSelectionBgColor(SkColor* color) const override;
  void GetActiveSelectionFgColor(SkColor* color) const override;
  void GetInactiveSelectionBgColor(SkColor* color) const override;
  void GetInactiveSelectionFgColor(SkColor* color) const override;
  bool PreferDarkTheme() const override;
  void SetDarkTheme(bool dark) override;
  void SetAccentColor(std::optional<SkColor> accent_color) override;
  std::unique_ptr<ui::NavButtonProvider> CreateNavButtonProvider() override;
  ui::WindowFrameProvider* GetWindowFrameProvider(bool solid_frame,
                                                  bool tiled) override;

 private:
  using TintMap = std::map<int, color_utils::HSL>;

  void OnThemeChanged(GtkSettings* settings, GtkParamSpec* param);

  void OnCursorThemeNameChanged(GtkSettings* settings, GtkParamSpec* param);

  void OnCursorThemeSizeChanged(GtkSettings* settings, GtkParamSpec* param);

  void OnEnableAnimationsChanged(GtkSettings* settings, GtkParamSpec* param);

  void OnGtkXftDpiChanged(GtkSettings* settings, GParamSpec* param);

  void OnScreenResolutionChanged(GdkScreen* screen, GParamSpec* param);

  void OnMonitorChanged(GdkMonitor* monitor, GParamSpec* param);

  void OnMonitorAdded(GdkDisplay* display, GdkMonitor* monitor);

  void OnMonitorRemoved(GdkDisplay* display, GdkMonitor* monitor);

  void OnMonitorsChanged(GListModel* list,
                         guint position,
                         guint removed,
                         guint added);

  // Loads all GTK-provided settings.
  void LoadGtkValues();

  // Extracts colors and tints from the GTK theme, both for the
  // ThemeService interface and the colors we send to Blink.
  void UpdateColors();

  // Listen for scale factor changes on `monitor`.
  void TrackMonitor(GdkMonitor* monitor);

  // Updates the device scale factor so that the default font size can be
  // recalculated.
  void UpdateDeviceScaleFactor();

  display::DisplayConfig GetDisplayConfig() const;

  void AddGtkNativeColorMixer(ui::ColorProvider* provider,
                              const ui::ColorProviderKey& key);

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

  std::optional<SkColor> accent_color_;

  std::optional<gfx::FontRenderParams> default_font_render_params_;

  std::unique_ptr<SettingsProvider> settings_provider_;

  // This is only used on GTK3.
  std::unique_ptr<GtkKeyBindingsHandler> key_bindings_handler_;

  // The action to take when middle, double, or right clicking the titlebar.
  base::flat_map<WindowFrameActionSource, WindowFrameAction>
      window_frame_actions_;

  // Paints a native window frame.  Typically only one of these will be
  // non-null.  The exception is when the user starts or stops their compositor
  // while Chrome is running.  This 2D array is indexed first by whether the
  // frame is translucent (0) or solid(1), then by whether the frame is normal
  // (0) or tiled (1).
  std::unique_ptr<ui::WindowFrameProvider> frame_providers_[2][2];

  // Objects to notify when the window frame button order changes.
  base::ObserverList<ui::WindowButtonOrderObserver>::Unchecked
      window_button_order_observer_list_;

  std::vector<ScopedGSignal> signals_;
  // Two signals are registered for each monitor, so keep them in a pair.
  std::unordered_map<GdkMonitor*, std::pair<ScopedGSignal, ScopedGSignal>>
      monitor_signals_;
};

}  // namespace gtk

#endif  // UI_GTK_GTK_UI_H_

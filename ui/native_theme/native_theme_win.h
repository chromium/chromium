// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_WIN_H_
#define UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

#include <windows.h>

#include <optional>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/win/registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/native_theme/native_theme.h"

class SkCanvas;

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeWin : public NativeTheme {
 public:
  enum ThemeName {
    BUTTON,
    LIST,
    MENU,
    MENULIST,
    SCROLLBAR,
    STATUS,
    TAB,
    TEXTFIELD,
    TRACKBAR,
    WINDOW,
    PROGRESS,
    SPIN,
    LAST
  };

  NativeThemeWin(const NativeThemeWin&) = delete;
  NativeThemeWin& operator=(const NativeThemeWin&) = delete;

  // Closes cached theme handles so we can unload the DLL or update our UI
  // for a theme change.
  static void CloseHandles();

  // NativeTheme implementation:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  void Paint(cc::PaintCanvas* canvas,
             const ui::ColorProvider* color_provider,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             bool forced_colors,
             PreferredColorScheme color_scheme,
             PreferredContrast contrast,
             const std::optional<SkColor>& accent_color) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;

  PreferredContrast CalculatePreferredContrast() const override;

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeWin>;

  NativeThemeWin();
  ~NativeThemeWin() override;

  PreferredColorScheme CalculatePreferredColorScheme() const;
  void set_in_dark_mode_for_testing(bool in_dark_mode) {
    in_dark_mode_ = in_dark_mode;
  }

 private:
  bool IsUsingHighContrastThemeInternal() const;
  void CloseHandlesInternal();

  // Called by `hwnd_subscription_`.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Updates `accent_color_`. If it changed, notifies callbacks.
  void OnAccentColorMaybeChanged();

  // Update the locally cached set of system colors.
  void UpdateSystemColors();

  // Painting functions that paint to PaintCanvas.
  void PaintMenuSeparator(cc::PaintCanvas* canvas,
                          const ColorProvider* color_provider,
                          const MenuSeparatorExtraParams& params) const;
  void PaintMenuGutter(cc::PaintCanvas* canvas,
                       const ColorProvider* color_provider,
                       const gfx::Rect& rect) const;
  void PaintMenuBackground(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           const gfx::Rect& rect) const;

  // Paint directly to canvas' HDC.
  void PaintDirect(SkCanvas* destination_canvas,
                   HDC hdc,
                   Part part,
                   State state,
                   const gfx::Rect& rect,
                   const ExtraParams& extra) const;

  // Create a temporary HDC, paint to that, clean up the alpha values in the
  // temporary HDC, and then blit the result to canvas.  This is to work around
  // the fact that Windows XP and some classic themes give bogus alpha values.
  void PaintIndirect(cc::PaintCanvas* destination_canvas,
                     Part part,
                     State state,
                     const gfx::Rect& rect,
                     const ExtraParams& extra) const;

  // Various helpers to paint specific parts.
  void PaintButtonClassic(HDC hdc,
                          Part part,
                          State state,
                          RECT* rect,
                          const ButtonExtraParams& extra) const;
  void PaintLeftMenuArrowThemed(HDC hdc,
                                HANDLE handle,
                                int part_id,
                                int state_id,
                                const gfx::Rect& rect) const;
  void PaintScrollbarArrowClassic(HDC hdc,
                                  Part part,
                                  State state,
                                  RECT* rect) const;
  void PaintScrollbarTrackClassic(SkCanvas* canvas,
                                  HDC hdc,
                                  RECT* rect,
                                  const ScrollbarTrackExtraParams& extra) const;
  void PaintHorizontalTrackbarThumbClassic(
      SkCanvas* canvas,
      HDC hdc,
      const RECT& rect,
      const TrackbarExtraParams& extra) const;
  void PaintProgressBarOverlayThemed(HDC hdc,
                                     HANDLE handle,
                                     RECT* bar_rect,
                                     RECT* value_rect,
                                     const ProgressBarExtraParams& extra) const;
  void PaintTextFieldThemed(HDC hdc,
                            HANDLE handle,
                            HBRUSH bg_brush,
                            int part_id,
                            int state_id,
                            RECT* rect,
                            const TextFieldExtraParams& extra) const;
  void PaintTextFieldClassic(HDC hdc,
                             HBRUSH bg_brush,
                             RECT* rect,
                             const TextFieldExtraParams& extra) const;

  // Paints a theme part, with support for scene scaling in high-DPI mode.
  // |theme| is the theme handle. |hdc| is the handle for the device context.
  // |part_id| is the identifier for the part (e.g. thumb gripper). |state_id|
  // is the identifier for the rendering state of the part (e.g. hover). |rect|
  // is the bounds for rendering, expressed in logical coordinates.
  void PaintScaledTheme(HANDLE theme,
                        HDC hdc,
                        int part_id,
                        int state_id,
                        const gfx::Rect& rect) const;

  // Get the windows theme name/part/state.  These three helper functions are
  // used only by GetPartSize(), as each of the corresponding PaintXXX()
  // methods do further validation of the part and state that is required for
  // getting the size.
  static ThemeName GetThemeName(Part part);
  static int GetWindowsPart(Part part, State state, const ExtraParams& extra);
  static int GetWindowsState(Part part, State state, const ExtraParams& extra);

  HRESULT PaintFrameControl(HDC hdc,
                            const gfx::Rect& rect,
                            UINT type,
                            UINT state,
                            bool is_selected,
                            State control_state) const;

  // Returns a handle to the theme data.
  HANDLE GetThemeHandle(ThemeName theme_name) const;

  void RegisterThemeRegkeyObserver();
  void RegisterColorFilteringRegkeyObserver();
  void UpdateDarkModeStatus();
  void UpdatePrefersReducedTransparency();
  void UpdateInvertedColors();

  // Dark Mode/Transparency registry key.
  base::win::RegKey hkcu_themes_regkey_;

  // Inverted colors registry key
  base::win::RegKey hkcu_color_filtering_regkey_;

  // A cache of open theme handles.
  mutable HANDLE theme_handles_[LAST];

  // Color/high contrast mode change observer.
  base::CallbackListSubscription hwnd_subscription_ =
      gfx::SingletonHwnd::GetInstance()->RegisterCallback(
          base::BindRepeating(&NativeThemeWin::OnWndProc,
                              base::Unretained(this)));

  // Accent color subscription.
  base::CallbackListSubscription accent_color_subscription_;

  bool in_dark_mode_ = false;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

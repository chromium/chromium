// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_WIN_H_
#define UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

// A wrapper class for working with custom XP/Vista themes provided in
// uxtheme.dll.  This is a singleton class that can be grabbed using
// NativeThemeWin::instance().
// For more information on visual style parts and states, see:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/userex/topics/partsandstates.asp
#include <windows.h>

#include <optional>

#include "base/no_destructor.h"
#include "base/win/registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/native_theme/native_theme.h"

class SkCanvas;

namespace ui {

// Windows implementation of native theme class.
//
// At the moment, this class in in transition from an older API that consists
// of several PaintXXX methods to an API, inherited from the NativeTheme base
// class, that consists of a single Paint() method with a argument to indicate
// what kind of part to paint.
class NATIVE_THEME_EXPORT NativeThemeWin : public NativeTheme,
                                           public gfx::SysColorChangeListener {
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
             ColorScheme color_scheme,
             bool in_forced_colors,
             const std::optional<SkColor>& accent_color) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;
  bool ShouldUseDarkColors() const override;

  // On Windows, we look at the high contrast setting to calculate the color
  // scheme. If high contrast is enabled, the preferred color scheme calculation
  // will ignore the state of dark mode. Instead, preferred color scheme will be
  // light or dark depending on the OS high contrast theme. If high contrast is
  // off, the preferred color scheme calculation will be based of the state of
  // dark mode.
  PreferredColorScheme CalculatePreferredColorScheme() const override;

  PreferredContrast CalculatePreferredContrast() const override;
  ColorScheme GetDefaultSystemColorScheme() const override;

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeWin>;

  // NativeTheme:
  void ConfigureWebInstance() override;
  std::optional<base::TimeDelta> GetPlatformCaretBlinkInterval() const override;

  NativeThemeWin(bool configure_web_instance, bool should_only_use_dark_colors);
  ~NativeThemeWin() override;

 private:
  bool IsUsingHighContrastThemeInternal() const;
  void CloseHandlesInternal();

  // gfx::SysColorChangeListener:
  void OnSysColorChange() override;

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

  // True if Windows supports dark mode. This does NOT indicate whether the
  // system is in dark mode, only that it is supported by this version of
  // Windows.
  const bool supports_windows_dark_mode_;

  // Dark Mode/Transparency registry key.
  base::win::RegKey hkcu_themes_regkey_;

  // Inverted colors registry key
  base::win::RegKey hkcu_color_filtering_regkey_;

  // A cache of open theme handles.
  mutable HANDLE theme_handles_[LAST];

  // The system color change listener and the updated cache of system colors.
  gfx::ScopedSysColorChangeListener color_change_listener_;

  // Used to notify the web native theme of changes to dark mode, high
  // contrast, preferred color scheme, and preferred contrast.
  std::unique_ptr<NativeTheme::ColorSchemeNativeThemeObserver>
      color_scheme_observer_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

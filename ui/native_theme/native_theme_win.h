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

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeWin : public NativeTheme {
 public:
  NativeThemeWin(const NativeThemeWin&) = delete;
  NativeThemeWin& operator=(const NativeThemeWin&) = delete;

  // Closes cached theme handles so we can unload the DLL or update our UI
  // for a theme change.
  static void CloseHandles();

  // NativeTheme:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra_params) const override;
  PreferredContrast CalculatePreferredContrast() const override;

  PreferredColorScheme CalculatePreferredColorScheme() const;
  void set_in_dark_mode_for_testing(bool in_dark_mode) {
    in_dark_mode_ = in_dark_mode;
  }

 protected:
  NativeThemeWin();
  ~NativeThemeWin() override;

  // NativeTheme:
  void PaintImpl(cc::PaintCanvas* canvas,
                 const ColorProvider* color_provider,
                 Part part,
                 State state,
                 const gfx::Rect& rect,
                 const ExtraParams& extra_params,
                 bool forced_colors,
                 bool dark_mode,
                 PreferredContrast contrast,
                 std::optional<SkColor> accent_color) const override;
  void OnToolkitSettingsChanged(bool force_notify) override;

 private:
  friend class base::NoDestructor<NativeThemeWin>;

  bool IsUsingHighContrastThemeInternal() const;
  void CloseHandlesInternal();

  // Called by `hwnd_subscription_`.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Updates `accent_color_`. If it changed, notifies callbacks.
  void OnAccentColorMaybeChanged();

  // Update the locally cached set of system colors.
  void UpdateSystemColors();

  void RegisterThemeRegkeyObserver();
  void UpdateDarkModeStatus();

  // Dark Mode registry key.
  base::win::RegKey hkcu_themes_regkey_;

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

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_WIN_H_
#define UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

#include <optional>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/win/registry.h"
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
  friend class TestNativeThemeWin;

  PreferredColorScheme CalculatePreferredColorScheme() const override;

  void RegisterThemeRegkeyObserver();
  void UpdateDarkModeStatus();

  // Dark Mode registry key.
  base::win::RegKey hkcu_themes_regkey_;

  bool in_dark_mode_ = false;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

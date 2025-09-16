// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
#define UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeMac : public NativeThemeBase {
 public:
  static const int kButtonCornerRadius = 3;

  // Type of gradient to use on a button background. Use HIGHLIGHTED for the
  // default button of a window and all combobox controls, but only when the
  // window is active.
  enum class ButtonBackgroundType {
    DISABLED,
    HIGHLIGHTED,
    NORMAL,
    PRESSED,
    COUNT
  };

  NativeThemeMac(const NativeThemeMac&) = delete;
  NativeThemeMac& operator=(const NativeThemeMac&) = delete;

  // The minimum size in px for the thumb, given device scale factor `scale`.
  // Exposed publicly for testing.
  static gfx::Size GetThumbMinSize(bool vertical, float scale);

  // NativeThemeBase:
  SkColor GetSystemButtonPressedColor(SkColor base_color) const override;
  PreferredContrast CalculatePreferredContrast() const override;

 protected:
  NativeThemeMac();
  ~NativeThemeMac() override;

  // NativeThemeBase:
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
  void PaintMenuItemBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& extra_params) const override;
  void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& extra_params) const override;

 private:
  friend class base::NoDestructor<NativeThemeMac>;

  // Because this header is #included from C++ source, we can't use Obj-C here.
  // Instead the Obj-C members are defined entirely in the .mm.
  struct ObjCMembers;

  void InitializeDarkModeStateAndObserver();

  std::unique_ptr<ObjCMembers> objc_members_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

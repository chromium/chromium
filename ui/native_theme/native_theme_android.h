// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_ANDROID_H_
#define UI_NATIVE_THEME_NATIVE_THEME_ANDROID_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

// Android implementation of native theme support.
class NativeThemeAndroid : public NativeThemeBase {
 public:
  NativeThemeAndroid(const NativeThemeAndroid&) = delete;
  NativeThemeAndroid& operator=(const NativeThemeAndroid&) = delete;

  // NativeThemeBase:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  SkColor GetSystemColorDeprecated(ColorId color_id,
                                   ColorScheme color_scheme,
                                   bool apply_processing) const override;

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeAndroid>;
  static NativeThemeAndroid* instance();

  // NativeThemeBase:
  void AdjustCheckboxRadioRectForPadding(SkRect* rect) const override;
  float AdjustBorderWidthByZoom(float border_width,
                                float zoom_level) const override;
  // TODO(crbug.com/1165342): Refine hover state behavior on available pointing
  // devices.
  SkColor ControlsAccentColorForState(State state,
                                      ColorScheme color_scheme) const override;
  SkColor ControlsSliderColorForState(State state,
                                      ColorScheme color_scheme) const override;
  SkColor ButtonBorderColorForState(State state,
                                    ColorScheme color_scheme) const override;
  SkColor ButtonFillColorForState(State state,
                                  ColorScheme color_scheme) const override;
  SkColor ControlsBorderColorForState(State state,
                                      ColorScheme color_scheme) const override;
  SkColor ControlsFillColorForState(State state,
                                    ColorScheme color_scheme) const override;

 private:
  NativeThemeAndroid();
  ~NativeThemeAndroid() override;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_ANDROID_H_

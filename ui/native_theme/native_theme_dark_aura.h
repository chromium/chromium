// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_DARK_AURA_H_
#define UI_NATIVE_THEME_NATIVE_THEME_DARK_AURA_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_aura.h"

namespace ui {

// Aura implementation of native theme support for dark mode (used for
// incognito).
class NATIVE_THEME_EXPORT NativeThemeDarkAura : public NativeThemeAura {
 public:
  static NativeThemeDarkAura* instance();

  // NativeThemeAura:
  SkColor GetSystemColor(ColorId color_id,
                         ColorScheme color_scheme) const override;
  bool ShouldUseDarkColors() const override;
  PreferredColorScheme GetPreferredColorScheme() const override;

 private:
  friend class base::NoDestructor<NativeThemeDarkAura>;

  NativeThemeDarkAura();
  ~NativeThemeDarkAura() override;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeDarkAura);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_DARK_AURA_H_

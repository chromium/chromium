// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/test_native_theme.h"

namespace ui {

TestNativeTheme::TestNativeTheme() {
  BeginObservingOsSettingChanges();
}

TestNativeTheme::~TestNativeTheme() = default;

void TestNativeTheme::SetPreferredColorScheme(
    PreferredColorScheme color_scheme) {
  set_preferred_color_scheme(color_scheme);
}

}  // namespace ui

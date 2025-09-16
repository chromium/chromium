// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
#define UI_NATIVE_THEME_TEST_NATIVE_THEME_H_

#include "base/component_export.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) TestNativeTheme : public NativeTheme {
 public:
  TestNativeTheme();

  TestNativeTheme(const TestNativeTheme&) = delete;
  TestNativeTheme& operator=(const TestNativeTheme&) = delete;

  ~TestNativeTheme() override;

  void SetPreferredColorScheme(PreferredColorScheme color_scheme);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_TEST_NATIVE_THEME_H_

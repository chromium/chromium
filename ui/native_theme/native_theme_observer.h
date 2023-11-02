// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_OBSERVER_H_
#define UI_NATIVE_THEME_NATIVE_THEME_OBSERVER_H_

#include "ui/native_theme/native_theme_export.h"

namespace ui {

class NativeTheme;

// Observers which are notified when the native theme changes.
class NATIVE_THEME_EXPORT NativeThemeObserver {
 public:
  // Called when the native theme changes. The observed theme is passed so that
  // observers may handle changes to their associated native theme instances.
  virtual void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {}

  // Called when the caption style changes.
  virtual void OnCaptionStyleUpdated() {}

  // Called when the system Increased Contrast state changes.
  virtual void OnPreferredContrastChanged() {}

 protected:
  virtual ~NativeThemeObserver();
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_OBSERVER_H_

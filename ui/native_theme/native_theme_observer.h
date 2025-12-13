// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_OBSERVER_H_
#define UI_NATIVE_THEME_NATIVE_THEME_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ui {

class NativeTheme;

// Observers which are notified when the native theme changes.
class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeObserver
    : public base::CheckedObserver {
 public:
  ~NativeThemeObserver() override;

  // Called when the native theme changes. The observed theme is passed so that
  // observers may handle changes to their associated native theme instances.
  virtual void OnNativeThemeUpdated(NativeTheme* observed_theme) {}

  // Called when the caption style changes.
  virtual void OnCaptionStyleUpdated() {}
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_OBSERVER_H_

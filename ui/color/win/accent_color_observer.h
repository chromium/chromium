// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_WIN_ACCENT_COLOR_OBSERVER_H_
#define UI_COLOR_WIN_ACCENT_COLOR_OBSERVER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/win/registry.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

// Monitors the HKCU\SOFTWARE\Microsoft\Windows\DWM reg key for changes and
// provides access to the accent color (and related colors), as well as support
// for firing callbacks when changes occur.
class COMPONENT_EXPORT(COLOR) AccentColorObserver {
 public:
  static AccentColorObserver* Get();

  AccentColorObserver();
  AccentColorObserver(const AccentColorObserver&) = delete;
  AccentColorObserver& operator=(const AccentColorObserver&) = delete;
  ~AccentColorObserver();

  // Registers `callback` to be called whenever the accent color changes.
  base::CallbackListSubscription Subscribe(base::RepeatingClosure callback);

  // NOTE: If `accent_color()` does not contain a value, both other colors are
  // guaranteed to also not contain values.
  std::optional<SkColor> accent_color() const { return accent_color_; }
  std::optional<SkColor> accent_color_inactive() const {
    return accent_color_inactive_;
  }
  std::optional<SkColor> accent_border_color() const {
    return accent_border_color_;
  }

  // Returns whether accent colors should be used for window frames.
  // Even when false, a non-null accent color may be used to theme secondary UI
  // or supplied to web content.
  bool ShouldUseAccentColorForWindowFrame() const;

  void SetAccentColorForTesting(std::optional<SkColor> accent_color);
  void SetShouldUseAccentColorForWindowFrameForTesting(bool use_accent_color);

 private:
  void OnDwmKeyUpdated();

  void UpdateAccentColors();

  // Registry key containing the params that determine the accent color.
  std::unique_ptr<base::win::RegKey> dwm_key_;

  base::RepeatingClosureList callbacks_;
  std::optional<SkColor> accent_color_;
  std::optional<SkColor> accent_color_inactive_;
  std::optional<SkColor> accent_border_color_;

  std::optional<bool> should_use_accent_color_for_window_frame_for_testing_;
};

}  // namespace ui

#endif  // UI_COLOR_WIN_ACCENT_COLOR_OBSERVER_H_

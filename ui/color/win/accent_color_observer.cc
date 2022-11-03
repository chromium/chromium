// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/win/accent_color_observer.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/win/windows_version.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/gfx/color_utils.h"

namespace ui {

// static
AccentColorObserver* AccentColorObserver::Get() {
  static base::NoDestructor<AccentColorObserver> observer;
  return observer.get();
}

AccentColorObserver::AccentColorObserver() {
  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    dwm_key_ = std::make_unique<base::win::RegKey>(
        HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", KEY_READ);
    if (dwm_key_->Valid())
      OnDwmKeyUpdated();
    else
      dwm_key_.reset();
  }
}

AccentColorObserver::~AccentColorObserver() = default;

base::CallbackListSubscription AccentColorObserver::Subscribe(
    base::RepeatingClosure callback) {
  return callbacks_.Add(std::move(callback));
}

void AccentColorObserver::OnDwmKeyUpdated() {
  accent_border_color_ = absl::nullopt;
  DWORD colorization_color, colorization_color_balance;
  if ((dwm_key_->ReadValueDW(L"ColorizationColor", &colorization_color) ==
       ERROR_SUCCESS) &&
      (dwm_key_->ReadValueDW(L"ColorizationColorBalance",
                             &colorization_color_balance) == ERROR_SUCCESS)) {
    // The accent border color is a linear blend between the colorization
    // color and the neutral #d9d9d9. colorization_color_balance is the
    // percentage of the colorization color in that blend.
    //
    // On Windows version 1611 colorization_color_balance can be 0xfffffff3 if
    // the accent color is taken from the background and either the background
    // is a solid color or was just changed to a slideshow. It's unclear what
    // that value's supposed to mean, so change it to 80 to match Edge's
    // behavior.
    if (colorization_color_balance > 100)
      colorization_color_balance = 80;

    // colorization_color's high byte is not an alpha value, so replace it
    // with 0xff to make an opaque ARGB color.
    SkColor input_color = SkColorSetA(colorization_color, 0xff);

    accent_border_color_ =
        color_utils::AlphaBlend(input_color, SkColorSetRGB(0xd9, 0xd9, 0xd9),
                                colorization_color_balance / 100.0f);
  }

  accent_color_ = absl::nullopt;
  accent_color_inactive_ = absl::nullopt;
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    DWORD accent_color, color_prevalence;
    bool use_dwm_frame_color =
        dwm_key_->ReadValueDW(L"AccentColor", &accent_color) == ERROR_SUCCESS &&
        dwm_key_->ReadValueDW(L"ColorPrevalence", &color_prevalence) ==
            ERROR_SUCCESS &&
        color_prevalence == 1;
    if (use_dwm_frame_color) {
      accent_color_ = skia::COLORREFToSkColor(accent_color);
      DWORD accent_color_inactive;
      if (dwm_key_->ReadValueDW(L"AccentColorInactive",
                                &accent_color_inactive) == ERROR_SUCCESS) {
        accent_color_inactive_ = skia::COLORREFToSkColor(accent_color_inactive);
      }
    }
  }

  callbacks_.Notify();

  // Watch for future changes.
  if (!dwm_key_->StartWatching(base::BindOnce(
          &AccentColorObserver::OnDwmKeyUpdated, base::Unretained(this)))) {
    dwm_key_.reset();
  }
}

}  // namespace ui

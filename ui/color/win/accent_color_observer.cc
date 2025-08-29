// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/win/accent_color_observer.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/gfx/color_utils.h"

namespace ui {

// static
AccentColorObserver* AccentColorObserver::Get() {
  static base::NoDestructor<AccentColorObserver> observer;
  return observer.get();
}

AccentColorObserver::AccentColorObserver() {
  dwm_key_ = std::make_unique<base::win::RegKey>(
      HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", KEY_READ);
  if (dwm_key_->Valid()) {
    OnDwmKeyUpdated();
  } else {
    dwm_key_.reset();
  }
}

AccentColorObserver::~AccentColorObserver() = default;

base::CallbackListSubscription AccentColorObserver::Subscribe(
    base::RepeatingClosure callback) {
  return callbacks_.Add(std::move(callback));
}

void AccentColorObserver::SetAccentColorForTesting(
    std::optional<SkColor> accent_color) {
  accent_color_ = accent_color;
  if (!accent_color_.has_value()) {
    // Maintain invariant that a null accent color implies null other colors.
    accent_color_inactive_.reset();
    accent_border_color_.reset();
  }
  callbacks_.Notify();
}

void AccentColorObserver::OnDwmKeyUpdated() {
  UpdateAccentColors();
  callbacks_.Notify();

  // Watch for future changes. If there is no task runner, this is a test or
  // tool context and watching is unnecessary.
  if (!base::SequencedTaskRunner::HasCurrentDefault() ||
      !dwm_key_->StartWatching(base::BindOnce(
          &AccentColorObserver::OnDwmKeyUpdated, base::Unretained(this)))) {
    dwm_key_.reset();
  }
}

void AccentColorObserver::UpdateAccentColors() {
  // Ignore accent colors unless color prevalence is enabled.
  accent_color_.reset();
  accent_color_inactive_.reset();
  accent_border_color_.reset();
  if (DWORD color_prevalence = 0;
      dwm_key_->ReadValueDW(L"ColorPrevalence", &color_prevalence) !=
          ERROR_SUCCESS ||
      color_prevalence != 1) {
    return;
  }

  if (DWORD accent_color = 0;
      dwm_key_->ReadValueDW(L"AccentColor", &accent_color) == ERROR_SUCCESS) {
    accent_color_ = skia::COLORREFToSkColor(accent_color);
  } else {
    // When there is no accent color, ignore inactive/border colors.
    return;
  }

  if (DWORD accent_color_inactive = 0;
      dwm_key_->ReadValueDW(L"AccentColorInactive", &accent_color_inactive) ==
      ERROR_SUCCESS) {
    accent_color_inactive_ = skia::COLORREFToSkColor(accent_color_inactive);
  }

  if (DWORD colorization_color, colorization_color_balance;
      dwm_key_->ReadValueDW(L"ColorizationColor", &colorization_color) ==
          ERROR_SUCCESS &&
      dwm_key_->ReadValueDW(L"ColorizationColorBalance",
                            &colorization_color_balance) == ERROR_SUCCESS) {
    // The accent border color is a linear blend between the colorization
    // color and the neutral #d9d9d9. colorization_color_balance is the
    // percentage of the colorization color in that blend.
    //
    // On Windows version 1611 colorization_color_balance can be 0xfffffff3 if
    // the accent color is taken from the background and either the background
    // is a solid color or was just changed to a slideshow. It's unclear what
    // that value's supposed to mean, so change it to 80 to match Edge's
    // behavior.
    if (colorization_color_balance > 100) {
      colorization_color_balance = 80;
    }

    // colorization_color's high byte is not an alpha value, so replace it
    // with 0xff to make an opaque ARGB color.
    SkColor input_color = SkColorSetA(colorization_color, 0xff);

    accent_border_color_ =
        color_utils::AlphaBlend(input_color, SkColorSetRGB(0xd9, 0xd9, 0xd9),
                                colorization_color_balance / 100.0f);
  }
}

}  // namespace ui
